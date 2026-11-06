#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace FightcadeProtocol {

inline constexpr auto ApiUrl = "https://web.fightcade.com/api/";
inline constexpr auto WebUrl = "https://web.fightcade.com";
inline constexpr auto WebSocketUrl = "wss://ggs.fightcade.com/ws/";
inline constexpr auto UserAgent = "Fightcade2-WIN32-v2.1.45";

QString previewUrl(const QString &gameId, const QString &channelName);
QString findLauncherToken(const QJsonValue &value);
bool isAutologinResponse(const QJsonObject &payload);
QVariantMap normalizeMatchStart(const QJsonObject &payload);
bool isChallengeWarning(const QJsonObject &payload, const QString &request);
QString challengeWarningMessage(const QJsonObject &payload);

QString quarkServed(const QString &gameId, const QString &quarkId, int playerId,
                    int port, int delay, int ranked);
QString quarkStream(const QString &gameId, const QString &quarkId, int port);
QString quarkTraining(const QString &gameId, const QString &quarkId, int playerId,
                      int port, int delay);
QString quarkDirect(const QString &gameId, int localPort, const QString &host,
                    int remotePort, int playerId, int delay, int ranked);
QString avatarUrl(const QString &gravatarHash);

QVariantMap normalizeChannel(const QJsonObject &object);
QVariantList normalizeChannelList(const QJsonObject &payload);
QVariantList normalizeWelcome(const QJsonObject &payload);
QVariantMap normalizeFilters(const QJsonObject &payload);
QVariantMap normalizeJoinedChannel(const QJsonObject &payload);
QVariantMap normalizeSessionUser(const QJsonObject &object);
QVariantMap normalizeUser(const QJsonObject &object, const QString &channelName = {});
QVariantList normalizeUsers(const QJsonArray &array, const QString &channelName);
QVariantMap normalizeQuark(const QJsonObject &object);
QVariantList normalizeQuarks(const QJsonArray &array);
QVariantMap normalizeEvent(const QJsonObject &object);
QVariantList normalizeEvents(const QJsonArray &array);

QString plainText(QString value);
QString errorString(const QJsonObject &payload);
QString authErrorCode(const QString &serverMessage);
QString authErrorMessage(const QString &serverMessage);

int intValue(const QJsonValue &value);
bool boolValue(const QJsonValue &value);
QString stringValue(const QJsonValue &value);

} // namespace FightcadeProtocol
