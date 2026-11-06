#pragma once

#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QWebSocket>

#include <functional>

class FightcadeClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY statusChanged)
    Q_PROPERTY(QVariantMap user READ user NOTIFY userChanged)
    Q_PROPERTY(QVariantMap appInfo READ appInfo NOTIFY userChanged)
    Q_PROPERTY(QString sessionCookie READ sessionCookie NOTIFY userChanged)

public:
    explicit FightcadeClient(QObject *parent = nullptr);

    QString status() const { return m_status; }
    QString statusMessage() const { return m_statusMessage; }
    bool connected() const { return m_status == QStringLiteral("connected"); }
    QVariantMap user() const { return m_user; }
    QVariantMap appInfo() const { return m_appInfo; }
    QString sessionCookie() const { return m_sessionCookie; }

    Q_INVOKABLE void login(const QString &username, const QString &password);
    Q_INVOKABLE void autologin(const QString &cookie);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void listWelcome();
    Q_INVOKABLE void listAllChannels();
    Q_INVOKABLE void listChannels(const QVariantMap &filters);
    Q_INVOKABLE void listFilters();
    Q_INVOKABLE void joinChannel(const QString &channelName, bool away = false);
    Q_INVOKABLE void leaveChannel(const QString &channelName);
    Q_INVOKABLE void sendChat(const QString &channelName, const QString &message);
    Q_INVOKABLE void favChannel(const QString &channelName, bool favorite);
    Q_INVOKABLE void setAway(bool away, const QString &channelName = {});
    Q_INVOKABLE void updateUser(const QVariantMap &patch);
    Q_INVOKABLE void challenge(const QString &username, const QString &channelName,
                               int challengeId, int ranked);
    Q_INVOKABLE void acceptChallenge(const QString &username, const QString &channelName,
                                     int challengeId, int ranked);
    Q_INVOKABLE void rejectChallenge(const QString &username, const QString &channelName,
                                     int challengeId);
    Q_INVOKABLE void cancelChallenge(const QString &username, const QString &channelName,
                                     int challengeId);

signals:
    void statusChanged();
    void userChanged();
    void loggedIn(const QVariantMap &user, const QVariantMap &appInfo);
    void loginFailed(const QString &code, const QString &message);
    void loggedOut();
    void connectionLost(const QString &message);
    void welcomeLoaded(const QVariantList &sections);
    void allChannelsLoaded(const QVariantList &channels);
    void channelsLoaded(const QVariantList &channels, int page, int totalChannels);
    void filtersLoaded(const QVariantMap &filters);
    void channelJoined(const QVariantMap &channel);
    void channelJoinFailed(const QString &channelName, const QString &message);
    void channelLeft(const QString &channelName);
    void chatReceived(const QVariantMap &message);
    void chatSent(const QString &channelName);
    void chatFailed(const QString &channelName, const QString &message);
    void serverNotice(const QString &message);
    void userJoined(const QString &channelName, const QVariantMap &user);
    void userLeft(const QString &channelName, const QString &username);
    void userKicked(const QString &channelName);
    void userUpdated(const QVariantMap &update);
    void challengeReceived(const QVariantMap &challenge);
    void challengeCancelled(const QVariantMap &challenge);
    void challengeRejected(const QVariantMap &challenge);
    void challengeAccepted(const QVariantMap &challenge);
    void challengeSent(const QVariantMap &challenge);
    void challengeFailed(const QString &channelName, const QString &message);
    void matchStarted(const QVariantMap &match);
    void quarkStats(const QString &channelName, const QString &quarkId, int spectators);
    void requestFailed(const QString &context, const QString &message);
    void notice(const QString &level, const QString &message);
    void rateLimited(qint64 until);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onTextMessage(const QString &message);
    void heartbeat();

private:
    using RequestCallback = std::function<void(bool, const QJsonObject &, const QString &)>;
    struct PendingRequest {
        RequestCallback callback;
        QString context;
    };

    void requestLocation(const std::function<void(bool, const QString &)> &callback);
    void withSocket(const std::function<void()> &ready, const std::function<void(const QString &)> &error);
    void sendAuth(const QString &action, const QJsonObject &payload);
    void handleAuthResponse(const QJsonObject &payload);
    void sendRequest(const QJsonObject &payload, const QString &context, RequestCallback callback);
    void sendOneWay(const QJsonObject &payload);
    void handleEvent(const QJsonObject &payload);
    void handleChat(const QJsonObject &payload, bool system);
    QVariantMap challengeMap(const QJsonObject &payload, const QString &direction) const;
    void setStatus(const QString &status, const QString &message = {});
    void failPending(const QString &message);
    void noteRateLimit();

    QWebSocket m_socket;
    QNetworkAccessManager m_network;
    QTimer m_heartbeat;
    QHash<int, PendingRequest> m_pending;
    std::function<void()> m_socketReady;
    std::function<void(const QString &)> m_socketError;
    QString m_status = QStringLiteral("disconnected");
    QString m_statusMessage;
    QVariantMap m_user;
    QVariantMap m_appInfo;
    QString m_sessionCookie;
    QString m_authAction;
    QString m_authFallbackCookie;
    qint64 m_authDeadline = 0;
    int m_authGeneration = 0;
    int m_requestIndex = 1;
    qint64 m_lastPong = 0;
    bool m_intentionalClose = false;
    qint64 m_rateLimitedUntil = 0;
};
