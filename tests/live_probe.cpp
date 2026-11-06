#include "fightcade/fightcadeclient.h"
#include "core/settingsstore.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString username = qEnvironmentVariable("FIGHTCADE_TEST_USER");
    const QString password = qEnvironmentVariable("FIGHTCADE_TEST_PASS");
    const bool autoLogin = qEnvironmentVariable("DECKADE_PROBE_AUTOLOGIN") == QStringLiteral("1");
    if (!autoLogin && (username.isEmpty() || password.isEmpty())) {
        qCritical() << "FIGHTCADE_TEST_USER / FIGHTCADE_TEST_PASS are not set";
        return 2;
    }

    FightcadeClient client;
    SettingsStore settings;
    const bool seedSession = qEnvironmentVariable("DECKADE_SEED_SESSION") == QStringLiteral("1");
    const bool chatProbe = qEnvironmentVariableIsSet("DECKADE_PROBE_CHAT");
    const bool challengeProbe = qEnvironmentVariableIsSet("DECKADE_PROBE_CHALLENGE");
    const QString challengeUser = qEnvironmentVariable("DECKADE_PROBE_CHALLENGE_USER");
    const int holdMs = qEnvironmentVariableIntValue("DECKADE_PROBE_HOLD_MS");
    const int challengeDelay = qEnvironmentVariableIsSet("DECKADE_PROBE_CHALLENGE_DELAY_MS")
        ? qEnvironmentVariableIntValue("DECKADE_PROBE_CHALLENGE_DELAY_MS") : 2500;
    const int challengeRanked = qEnvironmentVariableIsSet("DECKADE_PROBE_RANKED")
        ? qEnvironmentVariableIntValue("DECKADE_PROBE_RANKED") : 3;
    bool finished = false;

    auto finish = [&](int code) {
        if (finished) return;
        finished = true;
        client.logout();
        QTimer::singleShot(250, &app, [&app, code] { app.exit(code); });
    };

    QObject::connect(&client, &FightcadeClient::statusChanged, [&] {
        qInfo().noquote() << "STATUS" << client.status() << client.statusMessage();
    });
    QObject::connect(&client, &FightcadeClient::loginFailed, [&](const QString &code, const QString &message) {
        qWarning().noquote() << "LOGIN_FAILED" << code << message;
        finish(1);
    });
    QObject::connect(&client, &FightcadeClient::launcherStatusReported, [&](bool ok, const QString &message) {
        qInfo().noquote() << "LAUNCHER_STATUS" << (ok ? "OK" : "FAILED") << message;
    });
    QObject::connect(&client, &FightcadeClient::loggedIn, [&](const QVariantMap &user, const QVariantMap &) {
        qInfo().noquote() << "LOGGED_IN" << user.value("name").toString()
                          << "display=" << user.value("displayName").toString()
                          << "rank=" << user.value("rank").toInt()
                          << "away=" << user.value("away").toBool()
                          << "settings=" << user.value("settings").toMap();
        if (seedSession) {
            settings.saveSession(client.sessionCookie(),
                                 user.value("name").toString(),
                                 user.value("displayName").toString());
            qInfo() << "SESSION_SEEDED";
        }
        client.listWelcome();
        client.listAllChannels();
        client.listFilters();
    });
    QObject::connect(&client, &FightcadeClient::welcomeLoaded, [&](const QVariantList &sections) {
        qInfo().noquote() << "WELCOME_SECTIONS" << sections.size();
        for (const QVariant &section : sections) {
            const QVariantMap map = section.toMap();
            qInfo().noquote() << "  SECTION" << map.value("title").toString()
                              << "channels=" << map.value("channels").toList().size()
                              << "events=" << map.value("events").toList().size();
        }
    });
    QString probeChannel;
    QObject::connect(&client, &FightcadeClient::allChannelsLoaded, [&](const QVariantList &channels) {
        qInfo().noquote() << "ALL_CHANNELS" << channels.size();
        QStringList names;
        for (int i = 0; i < channels.size() && i < 8; ++i) {
            const QVariantMap channel = channels.at(i).toMap();
            names << channel.value("name").toString() + "(" + QString::number(channel.value("playerCount").toInt()) + ")";
        }
        qInfo().noquote() << "  TOP" << names.join(", ");
        for (const QVariant &entry : channels) {
            const QVariantMap sample = entry.toMap();
            const QString emulator = sample.value("emulator").toString();
            if (emulator == "flycast" || emulator == "snes9x" || emulator == "ggpofba" || emulator == "fc1") {
                qInfo().noquote() << "  NONFBNEO" << emulator
                                  << "gameId=" << sample.value("gameId").toString()
                                  << "name=" << sample.value("name").toString();
            }
        }
        const QString wanted = qEnvironmentVariable("DECKADE_PROBE_MATCH");
        for (const QVariant &entry : channels) {
            const QVariantMap channel = entry.toMap();
            const QString name = channel.value("name").toString();
            if (!wanted.isEmpty() && name.contains(wanted, Qt::CaseInsensitive)
                && channel.value("emulator").toString() == QStringLiteral("fbneo")) {
                probeChannel = name;
                break;
            }
        }
        if (probeChannel.isEmpty()) {
            for (const QVariant &entry : channels) {
                const QVariantMap channel = entry.toMap();
                if (chatProbe && channel.value("playerCount").toInt() == 0
                    && channel.value("emulator").toString() == QStringLiteral("fbneo")) {
                    probeChannel = channel.value("name").toString();
                    break;
                }
                if (!chatProbe && channel.value("playerCount").toInt() > 0
                    && channel.value("emulator").toString() == QStringLiteral("fbneo")) {
                    probeChannel = channel.value("name").toString();
                    break;
                }
            }
        }
        if (chatProbe && probeChannel.isEmpty() && !channels.isEmpty()) {
            probeChannel = channels.first().toMap().value("name").toString();
        }
        if (probeChannel.isEmpty() && !channels.isEmpty()) {
            probeChannel = channels.first().toMap().value("name").toString();
        }
        qInfo().noquote() << "PROBE_CHANNEL" << probeChannel;
        if (!probeChannel.isEmpty()) client.joinChannel(probeChannel, false);
        else finish(0);
    });
    QObject::connect(&client, &FightcadeClient::filtersLoaded, [&](const QVariantMap &filters) {
        qInfo().noquote() << "FILTERS genres=" << filters.value("genres").toList().size()
                          << "years=" << filters.value("years").toList().size()
                          << "systems=" << filters.value("systems").toList().size();
    });
    QObject::connect(&client, &FightcadeClient::channelJoined, [&](const QVariantMap &channel) {
        int playingCount = 0;
        int shown = 0;
        int challengeCandidates = 0;
        for (const QVariant &entry : channel.value("users").toList()) {
            const QVariantMap user = entry.toMap();
            const QString state = user.value("playing").toMap().isEmpty() ? "idle" : "playing";
            if (state == "playing") ++playingCount;
            if (state == "idle" && !user.value("channelAway").toBool()) ++challengeCandidates;
            if (shown++ < 20) {
                qInfo().noquote() << "  USER" << user.value("name").toString()
                                  << "rank=" << user.value("rank").toInt()
                                  << state
                                  << "channelAway=" << user.value("channelAway").toBool()
                          << "ranked=" << user.value("settings").toMap().value("ranked").toBool();
            }
        }
        qInfo().noquote() << "JOINED" << channel.value("name").toString()
                          << "users=" << channel.value("users").toList().size()
                          << "playing=" << playingCount
                          << "challengeCandidates=" << challengeCandidates
                          << "quarks=" << channel.value("quarks").toList().size()
                          << "ranked=" << channel.value("isRanked").toBool()
                          << "emulator=" << channel.value("emulator").toString();
        if (chatProbe) {
            client.sendChat(channel.value("name").toString(), QStringLiteral("hello"));
        } else if (challengeProbe) {
            const QString channelName = channel.value("name").toString();
            QString target = challengeUser;
            if (target.isEmpty()) {
                for (const QVariant &entry : channel.value("users").toList()) {
                    const QVariantMap user = entry.toMap();
                    const QString name = user.value("name").toString();
                    if (name == username) continue;
                    if (name.contains("Dummy", Qt::CaseInsensitive)) continue;
                    if (user.value("away").toBool()) continue;
                    if (user.value("channelAway").toBool()) continue;
                    if (!user.value("playing").toMap().isEmpty()) continue;
                    target = name;
                    break;
                }
            }
            if (target.isEmpty()) {
                qWarning() << "No idle challenge target found";
                client.leaveChannel(channelName);
            } else {
                qInfo().noquote() << "CHALLENGING" << target << "in" << channelName;
                QTimer::singleShot(challengeDelay, &client, [&client, target, channelName, challengeRanked] {
                    client.challenge(target, channelName, 1, challengeRanked);
                });
            }
        } else if (holdMs > 0) {
            qInfo().noquote() << "HOLD" << holdMs << "ms in" << channel.value("name").toString();
            QTimer::singleShot(holdMs, &client, [&client, &finish, channel] {
                client.leaveChannel(channel.value("name").toString());
                QTimer::singleShot(500, &client, [&finish] { finish(0); });
            });
        } else {
            client.leaveChannel(channel.value("name").toString());
        }
    });
    QObject::connect(&client, &FightcadeClient::channelJoinFailed, [&](const QString &channel, const QString &message) {
        qWarning().noquote() << "JOIN_FAILED" << channel << message;
        finish(0);
    });
    QObject::connect(&client, &FightcadeClient::channelLeft, [&](const QString &channel) {
        qInfo().noquote() << "LEFT" << channel;
        finish(0);
    });
    QObject::connect(&client, &FightcadeClient::chatSent, [&](const QString &channel) {
        qInfo().noquote() << "CHAT_SENT" << channel;
        client.leaveChannel(channel);
    });
    QObject::connect(&client, &FightcadeClient::challengeSent, [&](const QVariantMap &challenge) {
        qInfo().noquote() << "CHALLENGE_SENT" << challenge.value("challenger").toString()
                          << "id=" << challenge.value("challengeId").toInt();
    });
    QObject::connect(&client, &FightcadeClient::challengeAccepted, [&](const QVariantMap &challenge) {
        qInfo().noquote() << "CHALLENGE_ACCEPTED" << challenge.value("challenger").toString();
    });
    QObject::connect(&client, &FightcadeClient::challengeRejected, [&](const QVariantMap &challenge) {
        qWarning().noquote() << "CHALLENGE_REJECTED" << challenge.value("challenger").toString();
        finish(0);
    });
    QObject::connect(&client, &FightcadeClient::challengeFailed, [&](const QString &, const QString &message) {
        qWarning().noquote() << "CHALLENGE_FAILED" << message;
        finish(1);
    });
    QObject::connect(&client, &FightcadeClient::matchStarted, [&](const QVariantMap &match) {
        qInfo().noquote() << "MATCH_STARTED" << match.value("gameId").toString()
                          << "quark=" << match.value("quarkId").toString()
                          << "port=" << match.value("port").toInt()
                          << "player=" << match.value("playerId").toInt()
                          << "ranked=" << match.value("ranked").toInt()
                          << "delay=" << match.value("delay").toInt()
                          << "opponent=" << match.value("opponent").toString();
        finish(0);
    });
    QObject::connect(&client, &FightcadeClient::chatFailed, [&](const QString &channel, const QString &message) {
        qWarning().noquote() << "CHAT_FAILED" << channel << message;
        client.leaveChannel(channel);
    });
    QObject::connect(&client, &FightcadeClient::requestFailed, [&](const QString &context, const QString &message) {
        qWarning().noquote() << "REQUEST_FAILED" << context << message;
    });

    QTimer::singleShot(45000, &app, [&] {
        qWarning() << "TIMEOUT";
        finish(3);
    });

    if (autoLogin) {
        qInfo() << "AUTOLOGIN";
        client.autologin(settings.sessionCookie());
    } else {
        client.login(username, password);
    }
    return app.exec();
}
