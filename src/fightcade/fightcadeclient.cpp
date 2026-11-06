#include "fightcadeclient.h"

#include "protocol.h"
#include "userstatus.h"

#include <memory>
#include <QAbstractSocket>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QSet>
#include <QSysInfo>
#include <QUuid>

using namespace FightcadeProtocol;

FightcadeClient::FightcadeClient(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QWebSocket::connected, this, &FightcadeClient::onSocketConnected);
    connect(&m_socket, &QWebSocket::disconnected, this, &FightcadeClient::onSocketDisconnected);
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &FightcadeClient::onTextMessage);
    connect(&m_socket, &QWebSocket::errorOccurred, this, &FightcadeClient::onSocketError);
    connect(&m_socket, &QWebSocket::pong, this, [this] {
        m_lastPong = QDateTime::currentMSecsSinceEpoch();
    });

    m_heartbeat.setInterval(20000);
    connect(&m_heartbeat, &QTimer::timeout, this, &FightcadeClient::heartbeat);
    m_heartbeat.start();
}

void FightcadeClient::setStatus(const QString &status, const QString &message)
{
    if (m_status == status && m_statusMessage == message) {
        return;
    }
    m_status = status;
    m_statusMessage = message;
    emit statusChanged();
}

void FightcadeClient::requestLocation(const std::function<void(bool, const QString &)> &callback)
{
    QNetworkRequest request(QUrl(QString::fromLatin1(ApiUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(UserAgent));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");

    const QByteArray body = QJsonDocument(QJsonObject{{"req", "location"}}).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_network.post(request, body);
    connect(reply, &QNetworkReply::finished, this, [reply, callback] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            callback(false, QStringLiteral("Could not reach Fightcade: %1").arg(reply->errorString()));
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject payload = document.object();
        const QString location = stringValue(payload.value(QStringLiteral("location")));
        if (payload.value(QStringLiteral("res")).toString() != QStringLiteral("OK") || location.isEmpty()) {
            callback(false, QStringLiteral("Fightcade did not return a login location."));
            return;
        }
        callback(true, location);
    });
}

void FightcadeClient::withSocket(const std::function<void()> &ready,
                                 const std::function<void(const QString &)> &error)
{
    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        ready();
        return;
    }

    m_socketReady = ready;
    m_socketError = error;

    if (m_socket.state() == QAbstractSocket::ConnectingState) {
        return;
    }

    QNetworkRequest request(QUrl(QString::fromLatin1(WebSocketUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(UserAgent));
    m_socket.open(request);
}

void FightcadeClient::sendAuth(const QString &action, const QJsonObject &payload)
{
    m_authAction = action;
    m_socket.sendTextMessage(QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));

    const int generation = ++m_authGeneration;
    m_authDeadline = QDateTime::currentMSecsSinceEpoch() + 20000;
    QTimer::singleShot(20000, this, [this, generation] {
        if (m_authAction.isEmpty() || generation != m_authGeneration
            || QDateTime::currentMSecsSinceEpoch() < m_authDeadline) {
            return;
        }
        const QString action = m_authAction;
        m_authAction.clear();
        setStatus(QStringLiteral("error"), QStringLiteral("Fightcade sign-in timed out."));
        emit loginFailed(QStringLiteral("network-unavailable"),
                         action == QStringLiteral("autologin")
                             ? QStringLiteral("Session restore timed out. Sign in again.")
                             : QStringLiteral("Fightcade sign-in timed out."));
        if (action == QStringLiteral("autologin")) {
            m_sessionCookie.clear();
        }
        m_socket.close();
    });
}

void FightcadeClient::login(const QString &username, const QString &password)
{
    if (!m_authAction.isEmpty()) {
        emit loginFailed(QStringLiteral("unknown"), QStringLiteral("A sign-in is already in progress."));
        return;
    }

    setStatus(QStringLiteral("connecting"), QStringLiteral("Contacting Fightcade..."));
    requestLocation([this, username, password](bool ok, const QString &locationOrError) {
        if (!ok) {
            setStatus(QStringLiteral("error"), locationOrError);
            emit loginFailed(QStringLiteral("network-unavailable"), locationOrError);
            return;
        }

        withSocket([this, username, password, locationOrError] {
            setStatus(QStringLiteral("authenticating"), QStringLiteral("Signing in..."));
            m_authFallbackCookie.clear();
            sendAuth(QStringLiteral("login"), QJsonObject{
                {"req", "login"},
                {"username", username},
                {"userpass", password},
                {"location", locationOrError},
                {"requestIdx", -1}
            });
        }, [this](const QString &message) {
            setStatus(QStringLiteral("error"), message);
            emit loginFailed(QStringLiteral("network-unavailable"), message);
        });
    });
}

void FightcadeClient::autologin(const QString &cookie)
{
    if (!m_authAction.isEmpty()) {
        return;
    }

    setStatus(QStringLiteral("connecting"), QStringLiteral("Restoring session..."));
    requestLocation([this, cookie](bool ok, const QString &locationOrError) {
        if (!ok) {
            setStatus(QStringLiteral("error"), locationOrError);
            emit loginFailed(QStringLiteral("network-unavailable"), locationOrError);
            return;
        }

        withSocket([this, cookie, locationOrError] {
            setStatus(QStringLiteral("authenticating"), QStringLiteral("Restoring session..."));
            m_authFallbackCookie = cookie;
            sendAuth(QStringLiteral("autologin"), QJsonObject{
                {"req", "autologin"},
                {"cookie", cookie},
                {"location", locationOrError},
                {"requestIdx", -1}
            });
        }, [this](const QString &message) {
            setStatus(QStringLiteral("error"), message);
            emit loginFailed(QStringLiteral("network-unavailable"), message);
        });
    });
}

void FightcadeClient::handleAuthResponse(const QJsonObject &payload)
{
    if (m_authAction.isEmpty()) {
        return;
    }
    const QString action = m_authAction;
    m_authAction.clear();

    const int result = intValue(payload.value(QStringLiteral("result")));
    if (result != 200) {
        const QString serverMessage = errorString(payload);
        const QString code = authErrorCode(serverMessage);
        const QString message = authErrorMessage(serverMessage);
        setStatus(QStringLiteral("error"), message);
        emit loginFailed(code, message);
        if (action == QStringLiteral("autologin")) {
            m_sessionCookie.clear();
        }
        m_socket.close();
        return;
    }

    QVariantMap user = normalizeSessionUser(payload.value(QStringLiteral("user")).toObject());
    if (user.isEmpty()) {
        setStatus(QStringLiteral("error"), QStringLiteral("Fightcade did not return a user."));
        emit loginFailed(QStringLiteral("unknown"), QStringLiteral("Fightcade did not return a user."));
        m_socket.close();
        return;
    }

    const QString token = FightcadeProtocol::findLauncherToken(payload);
    if (!token.isEmpty()) {
        user.insert(QStringLiteral("token"), token);
    }
    const QString cookie = stringValue(payload.value(QStringLiteral("cookie")));
    if (!cookie.isEmpty()) {
        m_sessionCookie = cookie;
    } else if (!m_authFallbackCookie.isEmpty()) {
        m_sessionCookie = m_authFallbackCookie;
    }

    m_user = user;
    m_appInfo = payload.value(QStringLiteral("app_info")).toObject().toVariantMap();
    setStatus(QStringLiteral("connected"), QStringLiteral("Connected as %1").arg(user.value(QStringLiteral("displayName")).toString()));
    m_lastPong = QDateTime::currentMSecsSinceEpoch();
    emit userChanged();

    // The official launcher registers its machine/status with the lobby before
    // the web client starts joining channels. Macade does the same, and the
    // server otherwise keeps reporting a "broken or incomplete" installation
    // and rejects challenges.
    if (!token.isEmpty()) {
        reportLauncherStatus(token, [this](bool ok, const QString &message) {
            if (!ok) {
                qWarning().noquote() << "Fightcade launcher status report failed:" << message;
            }
            emit loggedIn(m_user, m_appInfo);
        });
    } else {
        qWarning() << "Fightcade autologin did not include a launcher token";
        emit loggedIn(m_user, m_appInfo);
    }
}

void FightcadeClient::reportLauncherStatus(const QString &token,
                                           const std::function<void(bool, const QString &)> &callback)
{
    const QByteArray body = QJsonDocument(FightcadeUserStatus::payload(token)).toJson(QJsonDocument::Compact);
    const QString curl = QStandardPaths::findExecutable(QStringLiteral("curl"));
    if (curl.isEmpty()) {
        qWarning() << "curl is not available; cannot report Fightcade launcher status";
        if (callback) callback(false, QStringLiteral("curl is not installed"));
        return;
    }

    auto *process = new QProcess(this);
    auto finished = std::make_shared<bool>(false);
    auto complete = [this, process, callback, finished](bool ok, const QString &message) {
        if (*finished) return;
        *finished = true;
        emit launcherStatusReported(ok, message);
        if (callback) callback(ok, message);
        process->deleteLater();
    };

    connect(process, &QProcess::errorOccurred, this, [process, complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            complete(false, QStringLiteral("Could not start curl"));
        }
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [process, complete](int exitCode, QProcess::ExitStatus status) {
                const QByteArray output = process->readAllStandardOutput().trimmed();
                const QByteArray error = process->readAllStandardError().trimmed();
                if (status != QProcess::NormalExit || exitCode != 0) {
                    complete(false, QString::fromUtf8(error.isEmpty() ? output : error).left(200));
                    return;
                }
                if (!output.contains("\"OK\"")) {
                    complete(false, QStringLiteral("Unexpected status response: %1")
                                       .arg(QString::fromUtf8(output).left(200)));
                    return;
                }
                complete(true, QStringLiteral("launcher status registered"));
            });
    QTimer::singleShot(12000, process, [process, complete] {
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            complete(false, QStringLiteral("Launcher status request timed out"));
        }
    });

    process->start(curl, {
        QStringLiteral("-sS"),
        QStringLiteral("--max-time"),
        QStringLiteral("10"),
        QStringLiteral("-A"),
        QStringLiteral("fcade"),
        QStringLiteral("-H"),
        QStringLiteral("Content-Type: application/json"),
        QStringLiteral("--data-binary"),
        QStringLiteral("@-"),
        QStringLiteral("https://web.fightcade.com/fc2status/api/")
    });
    process->write(body);
    process->closeWriteChannel();
}

void FightcadeClient::logout()
{
    m_intentionalClose = true;
    m_authAction.clear();
    m_sessionCookie.clear();
    m_user.clear();
    m_appInfo.clear();
    m_socket.close();
    m_socketReady = {};
    m_socketError = {};
    failPending(QStringLiteral("Logged out."));
    setStatus(QStringLiteral("disconnected"), QStringLiteral("Signed out."));
    emit userChanged();
    emit loggedOut();
}

void FightcadeClient::onSocketConnected()
{
    m_lastPong = QDateTime::currentMSecsSinceEpoch();
    if (m_socketReady) {
        const auto ready = std::move(m_socketReady);
        m_socketReady = {};
        m_socketError = {};
        ready();
    }
}

void FightcadeClient::onSocketDisconnected()
{
    if (m_intentionalClose) {
        m_intentionalClose = false;
        return;
    }

    const bool wasActive = m_status == QStringLiteral("connected")
        || m_status == QStringLiteral("authenticating");
    m_authAction.clear();
    failPending(QStringLiteral("Fightcade connection closed."));
    setStatus(QStringLiteral("disconnected"), QStringLiteral("Connection closed."));
    m_user.clear();
    emit userChanged();
    if (wasActive) {
        emit connectionLost(QStringLiteral("Connection to Fightcade was lost."));
    }
}

void FightcadeClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    const QString message = m_socket.errorString();
    if (m_socketError) {
        const auto callback = std::move(m_socketError);
        m_socketError = {};
        m_socketReady = {};
        callback(message);
    }
    if (m_status == QStringLiteral("connected")) {
        emit notice(QStringLiteral("error"), QStringLiteral("Lobby connection: %1").arg(message));
    }
}

void FightcadeClient::onTextMessage(const QString &message)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }
    const QJsonObject payload = document.object();
    const QString request = stringValue(payload.value(QStringLiteral("req")));

    if ((request == QStringLiteral("login") || request == QStringLiteral("autologin"))
        && payload.contains(QStringLiteral("result")) && !m_authAction.isEmpty()) {
        handleAuthResponse(payload);
        return;
    }

    const int requestIndex = intValue(payload.value(QStringLiteral("requestIdx")));
    if (requestIndex >= 0 && payload.contains(QStringLiteral("result")) && m_pending.contains(requestIndex)) {
        const PendingRequest pending = m_pending.take(requestIndex);
        if (intValue(payload.value(QStringLiteral("result"))) == 429) {
            noteRateLimit();
        }
        const bool ok = intValue(payload.value(QStringLiteral("result"))) == 200;
        pending.callback(ok, payload, ok ? QString() : errorString(payload));
        return;
    }

    if (!payload.contains(QStringLiteral("result"))) {
        handleEvent(payload);
    }
}

void FightcadeClient::heartbeat()
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastPong > 0 && now - m_lastPong > 60000) {
        emit notice(QStringLiteral("error"), QStringLiteral("Fightcade lobby stopped responding."));
        m_socket.close();
        return;
    }
    m_socket.ping();
}

void FightcadeClient::failPending(const QString &message)
{
    const auto pending = m_pending;
    m_pending.clear();
    for (const PendingRequest &request : pending) {
        request.callback(false, {}, message);
    }
}

void FightcadeClient::sendRequest(const QJsonObject &payload, const QString &context,
                                  RequestCallback callback)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        callback(false, {}, QStringLiteral("Not connected to Fightcade."));
        return;
    }

    const int index = m_requestIndex++;
    QJsonObject request = payload;
    request.insert(QStringLiteral("requestIdx"), index);
    m_pending.insert(index, PendingRequest{std::move(callback), context});
    m_socket.sendTextMessage(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));

    QTimer::singleShot(20000, this, [this, index, context] {
        if (!m_pending.contains(index)) {
            return;
        }
        const PendingRequest pending = m_pending.take(index);
        pending.callback(false, {}, QStringLiteral("Fightcade request timed out."));
        Q_UNUSED(context)
    });
}

void FightcadeClient::sendOneWay(const QJsonObject &payload)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        return;
    }
    QJsonObject request = payload;
    request.insert(QStringLiteral("requestIdx"), -1);
    m_socket.sendTextMessage(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
}

void FightcadeClient::listWelcome()
{
    sendRequest(QJsonObject{{"req", "welcome"}}, QStringLiteral("welcome"),
                [this](bool ok, const QJsonObject &payload, const QString &error) {
                    if (!ok) {
                        emit requestFailed(QStringLiteral("welcome"), error);
                        return;
                    }
                    emit welcomeLoaded(normalizeWelcome(payload));
                });
}

void FightcadeClient::listAllChannels()
{
    sendRequest(QJsonObject{{"req", "channels"}, {"all", true}}, QStringLiteral("channels"),
                [this](bool ok, const QJsonObject &payload, const QString &error) {
                    if (!ok) {
                        emit requestFailed(QStringLiteral("channels"), error);
                        return;
                    }
                    emit allChannelsLoaded(normalizeChannelList(payload));
                });
}

void FightcadeClient::listChannels(const QVariantMap &filters)
{
    const QString text = filters.value(QStringLiteral("text")).toString().trimmed();
    QJsonObject request{
        {"req", "channels"},
        {"filter", text.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(text)},
        {"genre", filters.value(QStringLiteral("genre")).toString().isEmpty()
                      ? QJsonValue(QJsonValue::Null) : QJsonValue(filters.value(QStringLiteral("genre")).toString())},
        {"year", filters.value(QStringLiteral("year")).toString().isEmpty()
                     ? QJsonValue(QJsonValue::Null) : QJsonValue(filters.value(QStringLiteral("year")).toString())},
        {"system", filters.value(QStringLiteral("system")).toString().isEmpty()
                       ? QJsonValue(QJsonValue::Null) : QJsonValue(filters.value(QStringLiteral("system")).toString())},
        {"ranked", filters.contains(QStringLiteral("ranked"))
                       ? QJsonValue(filters.value(QStringLiteral("ranked")).toBool())
                       : QJsonValue(QJsonValue::Null)},
        {"favorites", filters.value(QStringLiteral("favorites"), false).toBool()},
        {"paginated", true},
        {"page", filters.value(QStringLiteral("page"), 0).toInt()}
    };

    sendRequest(request, QStringLiteral("channels"),
                [this](bool ok, const QJsonObject &payload, const QString &error) {
                    if (!ok) {
                        emit requestFailed(QStringLiteral("channels"), error);
                        return;
                    }
                    emit channelsLoaded(normalizeChannelList(payload),
                                        intValue(payload.value(QStringLiteral("page"))),
                                        intValue(payload.value(QStringLiteral("totalChannels"))));
                });
}

void FightcadeClient::listFilters()
{
    sendRequest(QJsonObject{{"req", "filteroptions"}}, QStringLiteral("filters"),
                [this](bool ok, const QJsonObject &payload, const QString &error) {
                    if (!ok) {
                        emit requestFailed(QStringLiteral("filters"), error);
                        return;
                    }
                    emit filtersLoaded(normalizeFilters(payload));
                });
}

void FightcadeClient::joinChannel(const QString &channelName, bool away)
{
    const QJsonObject request{
        {"req", "join"},
        {"channelname", channelName},
        {"status", "available"},
        {"away", away},
        {"idx", -1}
    };

    sendRequest(request, QStringLiteral("join"),
                [this, channelName](bool ok, const QJsonObject &payload, const QString &error) {
                    if (!ok) {
                        emit channelJoinFailed(channelName, error);
                        return;
                    }
                    const QVariantMap channel = normalizeJoinedChannel(payload);
                    if (channel.isEmpty()) {
                        emit channelJoinFailed(channelName, QStringLiteral("Fightcade returned an invalid channel."));
                        return;
                    }
                    emit channelJoined(channel);
                });
}

void FightcadeClient::leaveChannel(const QString &channelName)
{
    sendRequest(QJsonObject{{"req", "leave"}, {"channelname", channelName}}, QStringLiteral("leave"),
                [this, channelName](bool ok, const QJsonObject &, const QString &error) {
                    if (!ok) {
                        emit requestFailed(QStringLiteral("leave"), error);
                        return;
                    }
                    emit channelLeft(channelName);
                });
}

void FightcadeClient::sendChat(const QString &channelName, const QString &message)
{
    sendRequest(QJsonObject{{"req", "chat"}, {"channelname", channelName}, {"chat", message}},
                QStringLiteral("chat"),
                [this, channelName](bool ok, const QJsonObject &payload, const QString &error) {
                    if (!ok || boolValue(payload.value(QStringLiteral("isspam")))) {
                        const QString message = boolValue(payload.value(QStringLiteral("isspam")))
                            ? QStringLiteral("Message not sent. Slow down a little.")
                            : error;
                        qWarning().noquote() << "Fightcade chat rejected in" << channelName << ":" << message;
                        emit chatFailed(channelName, message);
                        return;
                    }
                    emit chatSent(channelName);
                });
}

void FightcadeClient::favChannel(const QString &channelName, bool favorite)
{
    sendOneWay(QJsonObject{{"req", "favchannel"}, {"channelname", channelName}, {"fav", favorite}});
    QVariantList favorites = m_user.value(QStringLiteral("favoritesChannels")).toList();
    if (favorite) {
        if (!favorites.contains(channelName)) favorites.append(channelName);
    } else {
        favorites.removeAll(channelName);
    }
    m_user.insert(QStringLiteral("favoritesChannels"), favorites);
    emit userChanged();
}

void FightcadeClient::setAway(bool away, const QString &channelName)
{
    if (channelName.isEmpty()) {
        sendOneWay(QJsonObject{{"req", away ? "staway" : "stnoaway"}});
        m_user.insert(QStringLiteral("away"), away);
        emit userChanged();
    } else {
        sendOneWay(QJsonObject{{"req", away ? "chaway" : "chnoaway"}, {"channelname", channelName}});
    }
}

void FightcadeClient::updateUser(const QVariantMap &patch)
{
    QJsonObject request{{"req", "updateuser"}};
    for (auto it = patch.constBegin(); it != patch.constEnd(); ++it) {
        request.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }

    sendRequest(request, QStringLiteral("updateuser"),
                [this, patch](bool ok, const QJsonObject &, const QString &error) {
                    if (!ok) {
                        emit requestFailed(QStringLiteral("updateuser"), error);
                        return;
                    }
                    for (auto it = patch.constBegin(); it != patch.constEnd(); ++it) {
                        m_user.insert(it.key(), it.value());
                    }
                    emit userChanged();
                });
}

QVariantMap FightcadeClient::challengeMap(const QJsonObject &payload, const QString &direction) const
{
    QString challenger = stringValue(payload.value(QStringLiteral("username")));
    if (challenger.isEmpty()) {
        challenger = stringValue(payload.value(QStringLiteral("user")).toObject().value(QStringLiteral("name")));
    }
    return {
        {"channelName", stringValue(payload.value(QStringLiteral("channelname")))},
        {"challenger", challenger},
        {"challengeId", intValue(payload.value(QStringLiteral("challengeid")))},
        {"ranked", intValue(payload.value(QStringLiteral("ranked")))},
        {"direction", direction}
    };
}

void FightcadeClient::challenge(const QString &username, const QString &channelName,
                                int challengeId, int ranked)
{
    QJsonObject request{
        {"req", "challenge"},
        {"username", username},
        {"channelname", channelName},
        {"challengeid", challengeId}
    };
    // The official web client omits ranked for a casual set. Sending 0 is not
    // equivalent: the server rejects the challenge with "cant challenge user".
    if (ranked > 0) request.insert(QStringLiteral("ranked"), ranked);
    sendRequest(request,
                QStringLiteral("challenge"),
                [this, username, channelName, challengeId, ranked](bool ok, const QJsonObject &payload,
                                                                   const QString &error) {
                    if (!ok) {
                        emit challengeFailed(channelName, error);
                        return;
                    }
                    Q_UNUSED(payload)
                    emit challengeSent(QVariantMap{
                        {"channelName", channelName},
                        {"challenger", username},
                        {"challengeId", challengeId},
                        {"ranked", ranked},
                        {"direction", "outgoing"}
                    });
                });
}

void FightcadeClient::acceptChallenge(const QString &username, const QString &channelName,
                                      int challengeId, int ranked)
{
    QJsonObject request{
        {"req", "accept"},
        {"username", username},
        {"channelname", channelName},
        {"challengeid", challengeId}
    };
    if (ranked > 0) request.insert(QStringLiteral("ranked"), ranked);
    sendRequest(request,
                QStringLiteral("accept"),
                [this, channelName](bool ok, const QJsonObject &, const QString &error) {
                    if (!ok) {
                        emit challengeFailed(channelName, error);
                    }
                });
}

void FightcadeClient::rejectChallenge(const QString &username, const QString &channelName,
                                      int challengeId)
{
    sendOneWay(QJsonObject{
        {"req", "reject"},
        {"username", username},
        {"channelname", channelName},
        {"challengeid", challengeId}
    });
    emit challengeCancelled(QVariantMap{
        {"channelName", channelName},
        {"challenger", username},
        {"challengeId", challengeId},
        {"ranked", 0},
        {"direction", "incoming"}
    });
}

void FightcadeClient::cancelChallenge(const QString &username, const QString &channelName,
                                      int challengeId)
{
    sendOneWay(QJsonObject{
        {"req", "cancel"},
        {"username", username},
        {"channelname", channelName},
        {"challengeid", challengeId}
    });
    emit challengeCancelled(QVariantMap{
        {"channelName", channelName},
        {"challenger", username},
        {"challengeId", challengeId},
        {"ranked", 0},
        {"direction", "outgoing"}
    });
}

void FightcadeClient::handleChat(const QJsonObject &payload, bool system)
{
    const QString channelName = stringValue(payload.value(QStringLiteral("channelname")));
    QString body = stringValue(payload.value(QStringLiteral("chat")));
    if (body.isEmpty()) body = stringValue(payload.value(QStringLiteral("msg")));
    if (body.isEmpty()) body = stringValue(payload.value(QStringLiteral("message")));
    if (body.isEmpty() || channelName.isEmpty()) {
        return;
    }
    QString username = system ? QStringLiteral("system")
                              : stringValue(payload.value(QStringLiteral("username")));
    if (username.isEmpty()) {
        username = QStringLiteral("Fightcade");
    }
    emit chatReceived(QVariantMap{
        {"channelName", channelName},
        {"username", username},
        {"text", body},
        {"kind", system ? QStringLiteral("system") : QStringLiteral("chat")},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    });
}

void FightcadeClient::handleEvent(const QJsonObject &payload)
{
    const QString request = stringValue(payload.value(QStringLiteral("req")));
    const QString channelName = stringValue(payload.value(QStringLiteral("channelname")));

    if (request == QStringLiteral("chat")) {
        handleChat(payload, false);
        return;
    }
    if (request == QStringLiteral("broken")) {
        QString message = stringValue(payload.value(QStringLiteral("msg")));
        if (message.isEmpty()) message = stringValue(payload.value(QStringLiteral("message")));
        if (message.isEmpty()) message = stringValue(payload.value(QStringLiteral("chat")));
        if (!message.isEmpty()) {
            qWarning().noquote() << "Fightcade server notice:" << message;
            // The lobby can push a "broken installation" warning while a
            // challenge is pending. Treat it as the challenge failure instead
            // of waiting for the 20-second request timeout.
            if (message.contains(QStringLiteral("challenge"), Qt::CaseInsensitive)
                || message.contains(QStringLiteral("broken or incomplete"), Qt::CaseInsensitive)) {
                for (auto it = m_pending.begin(); it != m_pending.end();) {
                    if (it->context == QStringLiteral("challenge") || it->context == QStringLiteral("accept")) {
                        const PendingRequest pending = it.value();
                        it = m_pending.erase(it);
                        pending.callback(false, payload, message);
                    } else {
                        ++it;
                    }
                }
            }
            // A broken message with a channel is a channel system message and
            // is rendered in the chat log below. Only surface notices that
            // have no channel as a toast.
            if (channelName.isEmpty()) emit serverNotice(message);
        }
        handleChat(payload, true);
        return;
    }
    if (request == QStringLiteral("join")) {
        const QVariantMap user = normalizeUser(payload.value(QStringLiteral("user")).toObject(), channelName);
        if (!user.isEmpty() && !channelName.isEmpty()) {
            emit userJoined(channelName, user);
        }
        return;
    }
    if (request == QStringLiteral("leave")) {
        QString username = stringValue(payload.value(QStringLiteral("username")));
        if (username.isEmpty()) {
            username = stringValue(payload.value(QStringLiteral("user")).toObject().value(QStringLiteral("name")));
        }
        if (!username.isEmpty() && !channelName.isEmpty()) {
            emit userLeft(channelName, username);
        }
        return;
    }
    if (request == QStringLiteral("kick")) {
        if (!channelName.isEmpty()) emit userKicked(channelName);
        return;
    }
    if (request == QStringLiteral("challenge")) {
        emit challengeReceived(challengeMap(payload, QStringLiteral("incoming")));
        return;
    }
    if (request == QStringLiteral("cancel")) {
        emit challengeCancelled(challengeMap(payload, QStringLiteral("incoming")));
        return;
    }
    if (request == QStringLiteral("reject")) {
        emit challengeRejected(challengeMap(payload, QStringLiteral("outgoing")));
        return;
    }
    if (request == QStringLiteral("accept")) {
        emit challengeAccepted(challengeMap(payload, QStringLiteral("outgoing")));
        return;
    }
    if (request == QStringLiteral("start")) {
        const QVariantMap match = normalizeMatchStart(payload);
        if (!match.isEmpty()) emit matchStarted(match);
        return;
    }
    if (request == QStringLiteral("staway") || request == QStringLiteral("stnoaway")
        || request == QStringLiteral("chaway") || request == QStringLiteral("chnoaway")) {
        const QString username = stringValue(payload.value(QStringLiteral("username")));
        const bool channelScoped = request == QStringLiteral("chaway") || request == QStringLiteral("chnoaway");
        const bool away = request == QStringLiteral("staway") || request == QStringLiteral("chaway");
        if (!username.isEmpty()) {
            emit userUpdated(QVariantMap{
                {"username", username},
                {"channelName", channelScoped ? channelName : QString()},
                {"away", away},
                {"channelAway", channelScoped ? away : QVariant()}
            });
        }
        return;
    }
    if (request == QStringLiteral("stwlan") || request == QStringLiteral("stcable")) {
        const QString username = stringValue(payload.value(QStringLiteral("username")));
        if (!username.isEmpty()) {
            emit userUpdated(QVariantMap{{"username", username},
                                         {"wifi", request == QStringLiteral("stwlan")}});
        }
        return;
    }
    if (request == QStringLiteral("updaterank")) {
        const QString username = stringValue(payload.value(QStringLiteral("username")));
        if (!username.isEmpty()) {
            emit userUpdated(QVariantMap{{"username", username},
                                         {"rank", intValue(payload.value(QStringLiteral("ranked")))}});
        }
        return;
    }
    if (request == QStringLiteral("stplaying") || request == QStringLiteral("stnoplaying")) {
        const QString username = stringValue(payload.value(QStringLiteral("username")));
        if (username.isEmpty()) return;
        QVariantMap playing;
        if (request == QStringLiteral("stplaying")) {
            const QJsonObject game = payload.value(QStringLiteral("game")).toObject();
            playing = QVariantMap{
                {"channelName", stringValue(game.value(QStringLiteral("channelname")))},
                {"gameId", stringValue(game.value(QStringLiteral("gameid")))},
                {"quarkId", stringValue(game.value(QStringLiteral("quarkid")))},
                {"port", intValue(game.value(QStringLiteral("port")))}
            };
        }
        emit userUpdated(QVariantMap{{"username", username}, {"playing", playing}});
        return;
    }
    if (request == QStringLiteral("quarkstats")) {
        const QJsonObject quark = payload.value(QStringLiteral("quark")).toObject();
        const QString quarkId = stringValue(quark.value(QStringLiteral("quarkid")));
        if (!channelName.isEmpty() && !quarkId.isEmpty()) {
            emit quarkStats(channelName, quarkId, intValue(quark.value(QStringLiteral("spectators"))));
        }
        return;
    }
}

void FightcadeClient::noteRateLimit()
{
    m_rateLimitedUntil = QDateTime::currentMSecsSinceEpoch() + 30000;
    emit rateLimited(m_rateLimitedUntil);
    emit notice(QStringLiteral("warning"), QStringLiteral("Fightcade is rate limiting requests. Cooling down for 30 seconds."));
}
