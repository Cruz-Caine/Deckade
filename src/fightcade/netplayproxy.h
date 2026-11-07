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
    bool m_ready = false;
    bool m_sawLocal = false;
    bool m_sawPeer = false;
};
