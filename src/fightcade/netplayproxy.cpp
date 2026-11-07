#include "netplayproxy.h"
#include "netplaypreflight.h"
#include "portmapping.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHostInfo>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTextStream>
#include <QUdpSocket>
#include <QThread>

#include <algorithm>

namespace {
constexpr int kEmulatorProxyPort = 7001;
constexpr int kLocalBindPort = 6006;
constexpr int kMaximumDatagram = 65535;

QByteArray holePunchPayload(const QByteArray &localToken, const QByteArray &remoteToken = {})
{
    QByteArray result = localToken + ' ';
    result += remoteToken.isEmpty() ? QByteArray("_") : remoteToken;
    if (!remoteToken.isEmpty()) result += " ok";
    return result;
}

QByteArray tokenFromPayload(const QByteArray &payload, int index)
{
    const QList<QByteArray> parts = payload.split(' ');
    return index < parts.size() ? parts.at(index) : QByteArray{};
}
} // namespace

NetplayProxy::NetplayProxy(const QVariantMap &match, QObject *parent)
    : QObject(parent)
    , m_match(match)
{
    m_masterHost = qEnvironmentVariable("DECKADE_PROXY_MASTER_HOST", "ggpo.fightcade.com");
    m_localPortHint = static_cast<quint16>(qEnvironmentVariableIntValue("DECKADE_PROXY_LOCAL_PORT"));
    if (m_localPortHint == 0) m_localPortHint = 7001;
    m_masterBindPortHint = static_cast<quint16>(qEnvironmentVariableIntValue("DECKADE_PROXY_MASTER_BIND_PORT"));
    if (m_masterBindPortHint == 0) m_masterBindPortHint = 6006;
    const QString quarkId = match.value(QStringLiteral("quarkId")).toString();
    const int playerId = match.value(QStringLiteral("playerId")).toInt();
    m_quark = QStringLiteral("%1.%2").arg(quarkId).arg(playerId);
    m_logPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                    .filePath(QStringLiteral("runtime-work/fbneo/logs/proxy-%1.log")
                                  .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
    QDir().mkpath(QFileInfo(m_logPath).absolutePath());
    log(QStringLiteral("created quark=%1 masterPort=%2")
            .arg(m_quark)
            .arg(match.value(QStringLiteral("port")).toInt()));
}

NetplayProxy::~NetplayProxy()
{
    stop();
}

void NetplayProxy::log(const QString &message)
{
    QFile file(m_logPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " " << message << "\n";
}

bool NetplayProxy::bindSockets()
{
    m_master = new QUdpSocket(this);
    m_local = new QUdpSocket(this);

    bool masterBound = m_master->bind(QHostAddress::AnyIPv4, m_masterBindPortHint,
                                      QUdpSocket::DontShareAddress);
    if (!masterBound) {
        masterBound = m_master->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::DontShareAddress);
    }
    if (!masterBound) {
        log(QStringLiteral("could not bind master UDP socket: %1").arg(m_master->errorString()));
        return false;
    }

    int localPort = 0;
    for (int port = m_localPortHint; port < m_localPortHint + 9; ++port) {
        if (m_local->bind(QHostAddress::LocalHost, port, QUdpSocket::DontShareAddress)) {
            localPort = port;
            break;
        }
    }
    if (localPort == 0) {
        log(QStringLiteral("could not bind local proxy UDP port"));
        return false;
    }
    // FBNeo normally sends peer UDP from port 6000. The actual source is
    // learned from its first datagram, but this gives peer packets a valid
    // destination while the match is still starting.
    m_emulatorEndpoint = Endpoint{QHostAddress::LocalHost, 6000};
    log(QStringLiteral("master socket localPort=%1 proxyPort=%2")
            .arg(m_master->localPort())
            .arg(localPort));
    return true;
}

bool NetplayProxy::waitForDatagram(QUdpSocket &socket, QByteArray &payload, Endpoint &from, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (socket.hasPendingDatagrams()) {
            payload.resize(socket.pendingDatagramSize());
            QHostAddress sender;
            quint16 senderPort = 0;
            const qint64 count = socket.readDatagram(payload.data(), payload.size(), &sender, &senderPort);
            if (count < 0) continue;
            payload.resize(static_cast<int>(count));
            from = Endpoint{sender, senderPort};
            return true;
        }
        socket.waitForReadyRead(50);
    }
    return false;
}

bool NetplayProxy::registerWithMaster()
{
    const QString host = m_masterHost;
    const quint16 port = static_cast<quint16>(m_match.value(QStringLiteral("port")).toInt());
    if (port == 0) {
        log(QStringLiteral("invalid master port"));
        return false;
    }
    const QHostInfo info = QHostInfo::fromName(host);
    if (info.addresses().isEmpty()) {
        log(QStringLiteral("could not resolve %1").arg(host));
        return false;
    }
    QHostAddress masterAddress;
    for (const QHostAddress &candidate : info.addresses()) {
        if (candidate.protocol() == QAbstractSocket::IPv4Protocol) {
            masterAddress = candidate;
            break;
        }
    }
    if (masterAddress.isNull()) {
        log(QStringLiteral("no IPv4 address for %1").arg(host));
        return false;
    }
    m_masterEndpoint = Endpoint{masterAddress, port};

    const int proxyPort = m_local->localPort();
    const QByteArray registration = QStringLiteral("%1/%2").arg(m_quark).arg(proxyPort).toUtf8();
    const QByteArray expected = QStringLiteral("ok %1").arg(m_quark).toUtf8();
    for (int attempt = 0; attempt < 2; ++attempt) {
        m_master->writeDatagram(registration, m_masterEndpoint.host, m_masterEndpoint.port);
        QByteArray response;
        Endpoint from;
        if (!waitForDatagram(*m_master, response, from, attempt == 0 ? 2500 : 4000)) {
            log(QStringLiteral("master registration timeout attempt=%1").arg(attempt + 1));
            continue;
        }
        log(QStringLiteral("master response=%1 from=%2:%3")
                .arg(QString::fromUtf8(response), from.host.toString()).arg(from.port));
        if (response == expected) {
            m_master->writeDatagram(QByteArrayLiteral("ok"), m_masterEndpoint.host, m_masterEndpoint.port);
            return true;
        }
    }
    return false;
}

bool NetplayProxy::parsePeerAddress(const QByteArray &payload, Endpoint &endpoint) const
{
    if (payload.size() != 6) return false;
    const auto *bytes = reinterpret_cast<const unsigned char *>(payload.constData());
    const quint32 address = (quint32(bytes[0]) << 24) | (quint32(bytes[1]) << 16)
        | (quint32(bytes[2]) << 8) | quint32(bytes[3]);
    const quint16 port = quint16(bytes[4]) | (quint16(bytes[5]) << 8);
    if (port == 0) return false;
    endpoint = Endpoint{QHostAddress(address), port};
    return true;
}

bool NetplayProxy::punchPeer(const Endpoint &target)
{
    m_peerEndpoint = target;
    m_localToken = QByteArrayLiteral("0.")
        + QByteArray::number(QRandomGenerator::global()->generate64());
    for (int attempt = 0; attempt < 10; ++attempt) {
        sendToPeer(holePunchPayload(m_localToken));
        QByteArray response;
        Endpoint from;
        if (waitForDatagram(*m_master, response, from, 120)) {
            const QList<QByteArray> parts = response.split(' ');
            if (parts.size() >= 2 && parts.at(0).startsWith("0.")) {
                if (from.host == m_peerEndpoint.host && from.port != m_peerEndpoint.port) {
                    m_peerEndpoint.port = from.port;
                }
                m_remoteToken = parts.at(0);
                const bool acknowledged = parts.size() >= 3 && parts.at(1) == m_localToken
                    && parts.at(2) == QByteArrayLiteral("ok");
                if (acknowledged) {
                    m_keepalivePayload = holePunchPayload(m_localToken, m_remoteToken);
                    sendToPeer(m_keepalivePayload);
                    log(QStringLiteral("hole punch established peer=%1:%2")
                            .arg(m_peerEndpoint.host.toString()).arg(m_peerEndpoint.port));
                    return true;
                }
            }
        }
        QThread::msleep(100);
    }
    log(QStringLiteral("hole punch failed peer=%1:%2")
            .arg(target.host.toString()).arg(target.port));
    return false;
}

bool NetplayProxy::isHolePunch(const QByteArray &payload) const
{
    const QList<QByteArray> parts = payload.split(' ');
    return parts.size() >= 2 && parts.at(0).startsWith("0.")
        && (parts.at(1) == QByteArrayLiteral("_") || parts.at(1).startsWith("0."));
}

void NetplayProxy::sendUsePorts()
{
    if (!m_master || m_masterEndpoint.port == 0) return;
    const QByteArray payload = QStringLiteral("useports/%1").arg(m_quark).toUtf8();
    m_master->writeDatagram(payload, m_masterEndpoint.host, m_masterEndpoint.port);
    log(QStringLiteral("sent useports fallback"));
}

void NetplayProxy::sendToPeer(const QByteArray &payload)
{
    if (m_peerEndpoint.valid()) {
        m_master->writeDatagram(payload, m_peerEndpoint.host, m_peerEndpoint.port);
    }
}

bool NetplayProxy::prepare()
{
    if (m_ready) return true;
    if (m_quark.isEmpty()) {
        log(QStringLiteral("missing quark id"));
        return false;
    }
    if (!bindSockets()) {
        closeSockets();
        return false;
    }
    if (qEnvironmentVariableIsEmpty("DECKADE_DISABLE_PREFLIGHT")) {
        for (const QString &warning : NetplayPreflight::warnings(
                 m_masterHost, static_cast<quint16>(m_match.value(QStringLiteral("port")).toInt()),
                 m_local->localPort())) {
            log(QStringLiteral("preflight warning: %1").arg(warning));
        }
    }
    if (qEnvironmentVariableIsEmpty("DECKADE_DISABLE_PORT_MAPPING")) {
        const QList<PortMappingResult> mappings = PortMapping::mapUdp(
            {m_master->localPort(), m_local->localPort(), 6004}, 3600);
        for (const PortMappingResult &mapping : mappings) {
            log(QStringLiteral("port mapping port=%1 protocol=%2 mapped=%3 %4")
                    .arg(mapping.port).arg(mapping.protocol)
                    .arg(mapping.mapped ? QStringLiteral("yes") : QStringLiteral("no"))
                    .arg(mapping.message));
        }
    }
    if (!registerWithMaster()) {
        sendUsePorts();
        closeSockets();
        return false;
    }

    Endpoint peer;
    QByteArray peerData;
    if (!waitForDatagram(*m_master, peerData, peer, 5000)
        || !parsePeerAddress(peerData, peer)) {
        log(QStringLiteral("no peer address from master"));
        sendUsePorts();
        closeSockets();
        return false;
    }
    log(QStringLiteral("peer address=%1:%2").arg(peer.host.toString()).arg(peer.port));
    if (!punchPeer(peer)) {
        sendUsePorts();
        closeSockets();
        return false;
    }

    connect(m_local, &QUdpSocket::readyRead, this, &NetplayProxy::forwardLocal);
    connect(m_master, &QUdpSocket::readyRead, this, &NetplayProxy::forwardPeer);
    m_keepaliveTimer.setInterval(500);
    connect(&m_keepaliveTimer, &QTimer::timeout, this, &NetplayProxy::keepAlive);
    m_keepaliveTimer.start();
    m_ready = true;
    log(QStringLiteral("ready proxyPort=%1").arg(m_local->localPort()));
    return true;
}

void NetplayProxy::forwardLocal()
{
    while (m_local && m_local->hasPendingDatagrams()) {
        QByteArray payload;
        payload.resize(m_local->pendingDatagramSize());
        QHostAddress fromHost;
        quint16 fromPort = 0;
        if (m_local->readDatagram(payload.data(), payload.size(), &fromHost, &fromPort) < 0) continue;
        if (payload.isEmpty() || isHolePunch(payload)) continue;
        m_emulatorEndpoint = Endpoint{fromHost, fromPort};
        m_sawLocal = true;
        sendToPeer(payload);
    }
}

void NetplayProxy::forwardPeer()
{
    while (m_master && m_master->hasPendingDatagrams()) {
        QByteArray payload;
        payload.resize(m_master->pendingDatagramSize());
        QHostAddress fromHost;
        quint16 fromPort = 0;
        if (m_master->readDatagram(payload.data(), payload.size(), &fromHost, &fromPort) < 0) continue;
        if (payload.isEmpty() || isHolePunch(payload)) continue;
        if (fromHost != m_peerEndpoint.host) continue;
        m_sawPeer = true;
        if (m_local && m_emulatorEndpoint.valid()) {
            m_local->writeDatagram(payload, m_emulatorEndpoint.host, m_emulatorEndpoint.port);
        }
    }
}

void NetplayProxy::keepAlive()
{
    if (!m_ready || m_keepalivePayload.isEmpty()) return;
    if (m_sawLocal && m_sawPeer) {
        m_keepaliveTimer.stop();
        log(QStringLiteral("keepalive stopped after bidirectional GGPO"));
        return;
    }
    sendToPeer(m_keepalivePayload);
}

QVariantMap NetplayProxy::environment() const
{
    if (!m_ready) return {};
    return {
        {"MACADE_GGPO_PROXY_HOST", "127.0.0.1"},
        {"MACADE_GGPO_PROXY_PORT", QString::number(m_local->localPort())},
        {"MACADE_GGPO_TCP_REGISTER_PORT", QString::number(m_local->localPort())}
    };
}

void NetplayProxy::closeSockets()
{
    if (m_keepaliveTimer.isActive()) m_keepaliveTimer.stop();
    if (m_master) {
        m_master->close();
        m_master->deleteLater();
        m_master = nullptr;
    }
    if (m_local) {
        m_local->close();
        m_local->deleteLater();
        m_local = nullptr;
    }
    m_ready = false;
}

void NetplayProxy::stop()
{
    closeSockets();
}
