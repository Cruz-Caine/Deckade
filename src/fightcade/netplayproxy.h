#pragma once

#include <QByteArray>
#include <QObject>
#include <QHostAddress>
#include <QTimer>
#include <QVariantMap>

class QUdpSocket;

class NetplayProxy : public QObject
{
    Q_OBJECT

public:
    explicit NetplayProxy(const QVariantMap &match, QObject *parent = nullptr);
    ~NetplayProxy() override;

    // Blocking setup. Safe to call before starting FBNeo. Returns false when
    // the match should use the native direct/useports fallback instead.
    bool prepare();
    QVariantMap environment() const;
    QString logPath() const { return m_logPath; }

    // How long prepare() waits for the opponent's address.
    void setPeerWaitMs(int milliseconds) { m_peerWaitMs = milliseconds; }
    // Where peer traffic goes before the emulator's first packet reveals it.
    void setEmulatorPort(quint16 port);
    QString masterHost() const { return m_masterHost; }
    quint16 peerPort() const { return m_peerEndpoint.port; }
    quint16 proxyPort() const;
    // Stops relaying and frees the hole-punched port so an emulator can bind
    // it directly. Returns that port.
    quint16 releasePeerPort();
    bool ready() const { return m_ready; }
    void stop();

private slots:
    void forwardLocal();
    void forwardPeer();
    void keepAlive();

private:
    struct Endpoint {
        QHostAddress host;
        quint16 port = 0;
        bool valid() const { return !host.isNull() && port != 0; }
    };

    bool bindSockets();
    bool registerWithMaster();
    bool waitForDatagram(QUdpSocket &socket, QByteArray &payload, Endpoint &from, int timeoutMs);
    bool parsePeerAddress(const QByteArray &payload, Endpoint &endpoint) const;
    bool punchPeer(const Endpoint &target);
    bool isHolePunch(const QByteArray &payload) const;
    void sendToPeer(const QByteArray &payload);
    void sendUsePorts();
    void log(const QString &message);
    void closeSockets();

    QVariantMap m_match;
    QUdpSocket *m_master = nullptr;
    QUdpSocket *m_local = nullptr;
    QTimer m_keepaliveTimer;
    Endpoint m_masterEndpoint;
    Endpoint m_peerEndpoint;
    Endpoint m_emulatorEndpoint;
    QByteArray m_localToken;
    QByteArray m_remoteToken;
    QString m_quark;
    QString m_logPath;
    QString m_masterHost;
    quint16 m_localPortHint = 7001;
    quint16 m_masterBindPortHint = 6006;
    QByteArray m_keepalivePayload;
    int m_peerWaitMs = 5000;
    bool m_ready = false;
    bool m_sawLocal = false;
    bool m_sawPeer = false;
};
