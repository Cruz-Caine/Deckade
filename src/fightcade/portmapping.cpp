#include "portmapping.h"

#include <QFile>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QUrl>
#include <QXmlStreamReader>
#include <QtEndian>

namespace {
QByteArray be16(quint16 v) { uchar b[2]; qToBigEndian(v,b); return QByteArray(reinterpret_cast<char*>(b),2); }
QByteArray be32(quint32 v) { uchar b[4]; qToBigEndian(v,b); return QByteArray(reinterpret_cast<char*>(b),4); }
quint16 readBe16(const QByteArray &d,int o){ return o+2<=d.size()?qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(d.constData()+o)):0; }
quint32 readBe32(const QByteArray &d,int o){ return o+4<=d.size()?qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(d.constData()+o)):0; }

QByteArray udpExchange(const QByteArray &request, const QString &host, quint16 port, int timeoutMs)
{
    QUdpSocket socket;
    if (!socket.bind(QHostAddress::AnyIPv4, 0)) return {};
    socket.writeDatagram(request, QHostAddress(host), port);
    if (!socket.waitForReadyRead(timeoutMs)) return {};
    QByteArray response;
    response.resize(socket.pendingDatagramSize());
    socket.readDatagram(response.data(), response.size());
    return response;
}

bool mapPcp(int port, const QString &gateway, const QString &local, quint32 lifetime)
{
    QByteArray request;
    request.append(char(2)); request.append(char(1)); request.append(char(0)); request.append(char(0));
    request += be32(lifetime);
    request += QByteArray(16, '\0'); // IPv4-mapped local address placeholder
    // PCP wants the client IP in the 16-byte field; use the IPv4-mapped form.
    request.replace(8, 16, QByteArray(10, '\0') + QByteArray::fromHex("ffff")
                            + local.toUtf8());
    request += QByteArray(12, '\0'); // mapping nonce
    request.append(char(17));
    request += QByteArray(3, '\0');
    request += be16(static_cast<quint16>(port));
    request += be16(static_cast<quint16>(port));
    request += QByteArray(16, '\0');
    const QByteArray response = udpExchange(request, gateway, 5351, 1500);
    return response.size() >= 24 && static_cast<quint8>(response.at(0)) == 2
        && static_cast<quint8>(response.at(1)) == 0x81 && static_cast<quint8>(response.at(3)) == 0;
}

bool mapNatPmp(int port, const QString &gateway, quint32 lifetime)
{
    QByteArray request;
    request.append(char(0)); request.append(char(1)); request.append(char(0)); request.append(char(0));
    request += be16(static_cast<quint16>(port)); request += be16(static_cast<quint16>(port));
    request += be32(lifetime);
    const QByteArray response = udpExchange(request, gateway, 5351, 1500);
    return response.size() >= 16 && static_cast<quint8>(response.at(0)) == 0
        && static_cast<quint8>(response.at(1)) == 129
        && static_cast<quint8>(response.at(2)) == 0 && static_cast<quint8>(response.at(3)) == 0;
}

struct UPnPDevice { QUrl controlUrl; QString serviceType; };

QString xmlValue(const QString &xml, const QString &tag)
{
    const QRegularExpression rx(QStringLiteral("<%1>([^<]*)</%1>").arg(tag));
    const auto m = rx.match(xml);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

UPnPDevice discoverUpnp(const QString &local, QString *error)
{
    Q_UNUSED(error)
    const QByteArray search = "M-SEARCH * HTTP/1.1\r\nHOST:239.255.255.250:1900\r\nMAN:\"ssdp:discover\"\r\nMX:1\r\nST:urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";
    const QByteArray response = udpExchange(search, QStringLiteral("239.255.255.250"), 1900, 1800);
    const QString text = QString::fromLatin1(response);
    const QRegularExpression locationRx(QStringLiteral("(?i)location:\\s*([^\\r\\n]+)"));
    const auto match = locationRx.match(text);
    if (!match.hasMatch()) return {};
    const QUrl location(match.captured(1).trimmed());
    QNetworkAccessManager manager;
    QNetworkRequest request(location);
    request.setTransferTimeout(2000);
    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const QString xml = QString::fromUtf8(reply->readAll());
    reply->deleteLater();
    QString serviceType;
    QString controlPath;
    for (const QString &service : xml.split(QStringLiteral("<service>"))) {
        if (!service.contains(QStringLiteral("WANIPConnection"))
            && !service.contains(QStringLiteral("WANPPPConnection"))) continue;
        serviceType = xmlValue(service, QStringLiteral("serviceType"));
        controlPath = xmlValue(service, QStringLiteral("controlURL"));
        break;
    }
    if (serviceType.isEmpty() || controlPath.isEmpty()) return {};
    QUrl control(controlPath);
    if (control.isRelative()) control = location.resolved(control);
    Q_UNUSED(local)
    return UPnPDevice{control, serviceType};
}

bool mapUpnp(int port, const QString &local, const UPnPDevice &device, quint32 lifetime)
{
    if (device.controlUrl.isEmpty()) return false;
    const QString body = QStringLiteral(
        "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:AddPortMapping xmlns:u=\"%1\"><NewRemoteHost></NewRemoteHost><NewExternalPort>%2</NewExternalPort><NewProtocol>UDP</NewProtocol><NewInternalPort>%2</NewInternalPort><NewInternalClient>%3</NewInternalClient><NewEnabled>1</NewEnabled><NewPortMappingDescription>Deckade GGPO</NewPortMappingDescription><NewLeaseDuration>%4</NewLeaseDuration></u:AddPortMapping></s:Body></s:Envelope>")
        .arg(device.serviceType).arg(port).arg(local).arg(lifetime);
    QNetworkAccessManager manager;
    QNetworkRequest request(device.controlUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/xml; charset=\"utf-8\""));
    request.setRawHeader("SOAPAction", QStringLiteral("\"%1#AddPortMapping\"").arg(device.serviceType).toUtf8());
    request.setTransferTimeout(2000);
    QNetworkReply *reply = manager.post(request, body.toUtf8());
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    return code == 200;
}
} // namespace

QString PortMapping::defaultGateway()
{
    QFile route(QStringLiteral("/proc/net/route"));
    if (!route.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    const QList<QByteArray> lines = route.readAll().split('\n');
    for (int i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> fields = lines[i].simplified().split(' ');
        if (fields.size() < 3) continue;
        bool ok = false;
        const quint32 gateway = fields[2].toUInt(&ok, 16);
        if (!ok || gateway == 0) continue;
        return QHostAddress(qFromLittleEndian<quint32>(gateway)).toString();
    }
    return {};
}

QString PortMapping::localIPv4(const QString &gateway)
{
    if (gateway.isEmpty()) return {};
    QUdpSocket socket;
    if (!socket.bind(QHostAddress::AnyIPv4, 0)) return {};
    socket.connectToHost(QHostAddress(gateway), 80);
    if (!socket.waitForConnected(1000)) return {};
    return socket.localAddress().toString();
}

QList<PortMappingResult> PortMapping::mapUdp(const QList<int> &ports, int lifetime)
{
    const QString gateway = defaultGateway();
    QList<PortMappingResult> results;
    if (gateway.isEmpty()) {
        for (int port : ports) results.append({port, QStringLiteral("gateway"), false,
                                               QStringLiteral("No default gateway found.")});
        return results;
    }
    const QString local = localIPv4(gateway);
    QString error;
    const UPnPDevice upnp = discoverUpnp(local, &error);
    for (int port : ports) {
        if (mapPcp(port, gateway, local, static_cast<quint32>(lifetime))) {
            results.append({port, QStringLiteral("PCP"), true, QStringLiteral("Mapped with PCP.")});
        } else if (mapNatPmp(port, gateway, static_cast<quint32>(lifetime))) {
            results.append({port, QStringLiteral("NAT-PMP"), true, QStringLiteral("Mapped with NAT-PMP.")});
        } else if (mapUpnp(port, local, upnp, static_cast<quint32>(lifetime))) {
            results.append({port, QStringLiteral("UPnP"), true, QStringLiteral("Mapped with UPnP.")});
        } else {
            results.append({port, QStringLiteral("PCP/NAT-PMP/UPnP"), false,
                            QStringLiteral("Router did not accept an automatic mapping.")});
        }
    }
    return results;
}
