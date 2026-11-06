#include "protocol.h"

#include <QRegularExpression>
#include <QUrl>

namespace FightcadeProtocol {
namespace {

QString firstString(const QJsonObject &object, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QString value = stringValue(object.value(QLatin1String(key)));
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

int firstInt(const QJsonObject &object, std::initializer_list<const char *> keys, int fallback = 0)
{
    for (const char *key : keys) {
        const QJsonValue value = object.value(QLatin1String(key));
        if (!value.isUndefined() && !value.isNull()) {
            return intValue(value);
        }
    }
    return fallback;
}

bool firstBool(const QJsonObject &object, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        if (object.contains(QLatin1String(key))) {
            return boolValue(object.value(QLatin1String(key)));
        }
    }
    return false;
}

QVariantMap countryValue(const QJsonValue &value)
{
    if (value.isString()) {
        const QString code = value.toString().trimmed().toLower();
        return code.isEmpty() ? QVariantMap{} : QVariantMap{{"code", code}};
    }

    const QJsonObject object = value.toObject();
    if (object.isEmpty()) {
        return {};
    }

    QVariantMap country;
    const QString code = firstString(object, {"iso_code", "isoCode", "code"}).toLower();
    if (!code.isEmpty()) {
        country.insert("code", code);
    }
    const QString name = firstString(object, {"name"});
    if (!name.isEmpty()) {
        country.insert("name", name);
    }
    return country;
}

QVariantMap normalizePlaying(const QJsonObject &game, const QString &channelName)
{
    if (game.isEmpty()) {
        return {};
    }

    const QString channel = firstString(game, {"channelname", "channel", "channelId"});
    const QString quarkId = firstString(game, {"quarkid", "quarkId"});
    const int port = firstInt(game, {"port"}, -1);
    if (quarkId.isEmpty() || port < 0) {
        return {};
    }

    return {
        {"channelName", channel.isEmpty() ? channelName : channel},
        {"gameId", firstString(game, {"gameid", "gameId"})},
        {"quarkId", quarkId},
        {"port", port}
    };
}

void collectChannels(const QJsonValue &value, QJsonArray &target)
{
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue &entry : array) {
            collectChannels(entry, target);
        }
        return;
    }

    const QJsonObject object = value.toObject();
    if (object.isEmpty()) {
        return;
    }

    static const QStringList markers = {
        QStringLiteral("channelname"), QStringLiteral("gameid"), QStringLiteral("gamename"),
        QStringLiteral("emulator"), QStringLiteral("ranked"), QStringLiteral("clients")
    };
    bool looksLikeChannel = false;
    for (const QString &marker : markers) {
        if (object.contains(marker)) {
            looksLikeChannel = true;
            break;
        }
    }
    if (looksLikeChannel) {
        target.append(object);
    }
    collectChannels(object.value(QStringLiteral("channels")), target);
    collectChannels(object.value(QStringLiteral("results")), target);
}

} // namespace

int intValue(const QJsonValue &value)
{
    if (value.isDouble()) {
        return value.toInt();
    }
    if (value.isString()) {
        bool ok = false;
        const int parsed = value.toString().toInt(&ok);
        return ok ? parsed : 0;
    }
    if (value.isBool()) {
        return value.toBool() ? 1 : 0;
    }
    return 0;
}

bool boolValue(const QJsonValue &value)
{
    if (value.isBool()) {
        return value.toBool();
    }
    if (value.isDouble()) {
        return value.toInt() != 0;
    }
    if (value.isString()) {
        const QString normalized = value.toString().trimmed().toLower();
        return normalized == QStringLiteral("1") || normalized == QStringLiteral("true")
            || normalized == QStringLiteral("yes") || normalized == QStringLiteral("on");
    }
    return false;
}

QString stringValue(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toInt());
    }
    return {};
}

QString previewUrl(const QString &gameId, const QString &channelName)
{
    QString source = gameId.trimmed();
    if (source.isEmpty()) {
        source = channelName.trimmed();
    }
    if (source.startsWith(QStringLiteral("fc1_"))) {
        source.remove(0, 4);
    }
    return QStringLiteral("%1/static/previews/%2.png").arg(QLatin1String(WebUrl), QUrl::toPercentEncoding(source));
}

QString avatarUrl(const QString &gravatarHash)
{
    if (gravatarHash.trimmed().isEmpty()) {
        return {};
    }
    return QStringLiteral("https://www.gravatar.com/avatar/%1?s=96&d=retro&r=g")
        .arg(QUrl::toPercentEncoding(gravatarHash.trimmed()));
}

QVariantMap normalizeChannel(const QJsonObject &object)
{
    const QString name = firstString(object, {"channelname", "name", "id", "gameid"});
    if (name.isEmpty()) {
        return {};
    }

    const QString gameId = firstString(object, {"gameid", "gameId"}).isEmpty()
        ? name
        : firstString(object, {"gameid", "gameId"});
    const QString title = firstString(object, {"gamename", "title", "description", "longname", "channelname", "name"});
    const int availableFor = firstInt(object, {"available_for", "availableFor"}, 0);

    return {
        {"id", name},
        {"name", name},
        {"title", title.isEmpty() ? name : title},
        {"gameId", gameId},
        {"system", firstString(object, {"system", "platform", "console"})},
        {"emulator", firstString(object, {"emulator", "emu"})},
        {"playerCount", firstInt(object, {"clients", "users", "players", "numplayers", "online", "nplayers"}, 0)},
        {"spectatorCount", firstInt(object, {"spectators", "streams", "watching", "numspectators"}, 0)},
        {"isRanked", firstBool(object, {"ranked"})},
        {"isFavorite", firstBool(object, {"fav", "favorite", "isFavorite"})},
        {"supportsTraining", firstBool(object, {"training"})},
        {"previewUrl", previewUrl(gameId, name)},
        {"availableFor", availableFor}
    };
}

QVariantList normalizeChannelList(const QJsonObject &payload)
{
    QJsonArray channels;
    collectChannels(payload.value(QStringLiteral("channels")), channels);
    collectChannels(payload.value(QStringLiteral("results")), channels);

    QVariantList result;
    QSet<QString> seen;
    for (const QJsonValue &value : std::as_const(channels)) {
        const QVariantMap channel = normalizeChannel(value.toObject());
        const QString name = channel.value(QStringLiteral("name")).toString();
        if (name.isEmpty() || seen.contains(name)) {
            continue;
        }
        seen.insert(name);
        result.append(channel);
    }
    return result;
}

QVariantList normalizeWelcome(const QJsonObject &payload)
{
    QVariantList sections;
    const QJsonArray results = payload.value(QStringLiteral("results")).toArray();
    for (const QJsonValue &value : results) {
        const QJsonObject section = value.toObject();
        const QString title = firstString(section, {"title", "name"});
        if (title.isEmpty()) {
            continue;
        }

        QVariantList channels;
        const QJsonArray rawChannels = section.value(QStringLiteral("channels")).toArray();
        for (const QJsonValue &channelValue : rawChannels) {
            const QVariantMap channel = normalizeChannel(channelValue.toObject());
            if (!channel.isEmpty()) {
                channels.append(channel);
            }
        }

        QVariantList categories;
        const QJsonArray rawCategories = section.value(QStringLiteral("categories")).toArray();
        for (const QJsonValue &category : rawCategories) {
            if (category.isString()) {
                categories.append(category.toString());
            }
        }

        sections.append(QVariantMap{
            {"title", title},
            {"channels", channels},
            {"categories", categories},
            {"events", normalizeEvents(section.value(QStringLiteral("events")).toArray())}
        });
    }
    return sections;
}

QVariantMap normalizeFilters(const QJsonObject &payload)
{
    auto strings = [](const QJsonArray &array) {
        QVariantList values;
        for (const QJsonValue &value : array) {
            const QString text = stringValue(value);
            if (!text.isEmpty()) {
                values.append(text);
            }
        }
        return values;
    };

    return {
        {"genres", strings(payload.value(QStringLiteral("genres")).toArray())},
        {"years", strings(payload.value(QStringLiteral("years")).toArray())},
        {"systems", strings(payload.value(QStringLiteral("systems")).toArray())}
    };
}

QVariantMap normalizeJoinedChannel(const QJsonObject &payload)
{
    QJsonObject channelObject = payload;
    if (!channelObject.contains(QStringLiteral("channelname"))) {
        const QString name = firstString(payload, {"channel", "name", "gameid"});
        if (!name.isEmpty()) {
            channelObject.insert(QStringLiteral("channelname"), name);
        }
    }

    QVariantMap channel = normalizeChannel(channelObject);
    if (channel.isEmpty()) {
        return {};
    }

    const QVariantList users = normalizeUsers(payload.value(QStringLiteral("users")).toArray(),
                                               channel.value(QStringLiteral("name")).toString());
    if (!users.isEmpty() && channel.value(QStringLiteral("playerCount")).toInt() == 0) {
        channel.insert(QStringLiteral("playerCount"), users.size());
    }

    channel.insert(QStringLiteral("users"), users);
    channel.insert(QStringLiteral("quarks"), normalizeQuarks(payload.value(QStringLiteral("quarks")).toArray()));
    channel.insert(QStringLiteral("motd"), plainText(firstString(payload, {"motd", "welcome", "message", "html"})));
    channel.insert(QStringLiteral("events"), normalizeEvents(payload.value(QStringLiteral("events")).toArray()));
    return channel;
}

QVariantMap normalizeSessionUser(const QJsonObject &object)
{
    const QString name = firstString(object, {"name", "username", "id"});
    if (name.isEmpty()) {
        return {};
    }

    QVariantList channels;
    const QJsonArray channelArray = object.value(QStringLiteral("channels")).toArray();
    for (const QJsonValue &value : channelArray) {
        if (value.isString()) {
            channels.append(value.toString());
        } else {
            const QString channelName = firstString(value.toObject(), {"channelname", "channel", "name", "id"});
            if (!channelName.isEmpty()) channels.append(channelName);
        }
    }

    QVariantList activeChannels;
    const QJsonArray activeArray = object.value(QStringLiteral("active_channels")).toArray();
    for (const QJsonValue &value : activeArray) {
        const QJsonObject active = value.toObject();
        const QString channelName = firstString(active, {"channelname", "channel", "name"});
        if (!channelName.isEmpty()) {
            activeChannels.append(QVariantMap{
                {"channelName", channelName},
                {"away", boolValue(active.value(QStringLiteral("away")))}
            });
        }
    }

    auto stringList = [](const QJsonArray &array) {
        QVariantList result;
        for (const QJsonValue &value : array) {
            if (value.isString()) result.append(value.toString());
        }
        return result;
    };

    const QJsonObject settingsObject = object.value(QStringLiteral("settings")).toObject();
    const QJsonObject perksObject = object.value(QStringLiteral("perks")).toObject();
    const QString sort = firstString(object, {"users_sort_by"});
    const QString normalizedSort = sort == QStringLiteral("name") || sort == QStringLiteral("rank")
        ? sort : QStringLiteral("ping");

    return {
        {"name", name},
        {"displayName", firstString(object, {"displayName", "display_name", "name", "username"})},
        {"gravatar", firstString(object, {"gravatar"})},
        {"country", countryValue(object.value(QStringLiteral("country")))},
        {"role", firstInt(object, {"role"}, 0)},
        {"patreon", firstBool(object, {"patreon"})},
        {"dev", firstBool(object, {"dev"})},
        {"token", firstString(object, {"token"})},
        {"email", firstString(object, {"email"})},
        {"settings", QVariantMap{
            {"ranked", settingsObject.isEmpty() ? firstBool(object, {"ranked"}) : boolValue(settingsObject.value(QStringLiteral("ranked")))},
            {"delay", firstInt(object, {"delay"}, -1)},
            {"autoAfk", object.value(QStringLiteral("auto_afk")).isUndefined() ? true : boolValue(object.value(QStringLiteral("auto_afk")))},
            {"chatMuted", firstBool(object, {"chat_muted"})},
            {"preventBadChallenges", firstBool(object, {"prevent_bad_challenges"})},
            {"preventWifiChallenges", firstBool(object, {"prevent_wifi_challenges"})},
            {"disableSpectators", firstBool(object, {"disable_spectators"})},
            {"usersSortBy", normalizedSort}
        }},
        {"channels", channels},
        {"activeChannels", activeChannels},
        {"favoritesChannels", stringList(object.value(QStringLiteral("favorites_channels")).toArray())},
        {"lobbyChannels", stringList(object.value(QStringLiteral("lobby_channels")).toArray())},
        {"ignores", stringList(object.value(QStringLiteral("ignores")).toArray())},
        {"perks", QVariantMap{
            {"maxNumChannels", firstInt(perksObject, {"maxNumChannels"}, 5)}
        }}
    };
}

QVariantMap normalizeUser(const QJsonObject &object, const QString &channelName)
{
    const QString name = firstString(object, {"name", "username", "id"});
    if (name.isEmpty()) {
        return {};
    }

    const QJsonObject settings = object.value(QStringLiteral("settings")).toObject();
    const bool ranked = settings.isEmpty() ? boolValue(object.value(QStringLiteral("ranked")))
                                           : boolValue(settings.value(QStringLiteral("ranked")));

    const QJsonValue playingValue = object.value(QStringLiteral("playing"));
    const bool isPlaying = playingValue.isBool()
        ? playingValue.toBool()
        : playingValue.isObject()
          ? !playingValue.toObject().isEmpty()
          : boolValue(playingValue);
    const QVariantMap playing = isPlaying
        ? normalizePlaying(object.value(QStringLiteral("game")).toObject(), channelName)
        : QVariantMap{};

    QVariantMap user{
        {"name", name},
        {"gravatar", firstString(object, {"gravatar"})},
        {"country", countryValue(object.value(QStringLiteral("country")))},
        {"ping", firstInt(object, {"ping"}, -1)},
        {"virtualPing", firstInt(object, {"vping", "virtualPing"}, -1)},
        {"rank", firstInt(object, {"rank"}, 0)},
        {"away", firstBool(object, {"away"})},
        {"isWifi", firstBool(object, {"wlan"})},
        {"isProxy", firstBool(object, {"proxy"})},
        {"settings", QVariantMap{{"ranked", ranked}}},
        {"playing", playing}
    };

    if (!channelName.isEmpty()) {
        user.insert(QStringLiteral("channelAway"), firstBool(object, {"channel_away"}));
    }
    return user;
}

QVariantList normalizeUsers(const QJsonArray &array, const QString &channelName)
{
    QVariantList users;
    for (const QJsonValue &value : array) {
        const QVariantMap user = normalizeUser(value.toObject(), channelName);
        if (!user.isEmpty()) {
            users.append(user);
        }
    }
    return users;
}

QVariantMap normalizeQuark(const QJsonObject &object)
{
    const QString quarkId = firstString(object, {"quarkid", "quarkId"});
    const int port = firstInt(object, {"port"}, -1);
    if (quarkId.isEmpty() || port < 0) {
        return {};
    }

    QVariantList players;
    const QJsonArray rawPlayers = object.value(QStringLiteral("players")).toArray();
    for (const QJsonValue &player : rawPlayers) {
        if (player.isString()) {
            players.append(player.toString());
        } else {
            const QString name = firstString(player.toObject(), {"name", "username"});
            if (!name.isEmpty()) {
                players.append(name);
            }
        }
    }

    return {
        {"quarkId", quarkId},
        {"port", port},
        {"ranked", firstInt(object, {"ranked"}, 0)},
        {"spectators", firstInt(object, {"spectators"}, 0)},
        {"players", players}
    };
}

QVariantList normalizeQuarks(const QJsonArray &array)
{
    QVariantList quarks;
    for (const QJsonValue &value : array) {
        const QVariantMap quark = normalizeQuark(value.toObject());
        if (!quark.isEmpty()) {
            quarks.append(quark);
        }
    }
    return quarks;
}

QVariantMap normalizeEvent(const QJsonObject &object)
{
    const QString name = firstString(object, {"name", "title"});
    const QString gameId = firstString(object, {"gameid", "gameId"});
    if (name.isEmpty() || gameId.isEmpty()) {
        return {};
    }

    double date = 0;
    const QJsonValue dateValue = object.value(QStringLiteral("date"));
    if (dateValue.isDouble()) {
        date = dateValue.toDouble();
    } else if (dateValue.isString()) {
        date = dateValue.toString().toDouble();
    }
    if (date > 0 && date < 100000000000.0) {
        date *= 1000.0;
    }

    return {
        {"name", name},
        {"author", firstString(object, {"author"})},
        {"date", date},
        {"gameId", gameId},
        {"imageUrl", firstString(object, {"image", "thumbnail"})},
        {"link", firstString(object, {"link", "url"})},
        {"region", firstString(object, {"region"})},
        {"stream", firstString(object, {"stream"})}
    };
}

QVariantList normalizeEvents(const QJsonArray &array)
{
    QVariantList events;
    for (const QJsonValue &value : array) {
        const QVariantMap event = normalizeEvent(value.toObject());
        if (!event.isEmpty()) {
            events.append(event);
        }
    }
    return events;
}

QString plainText(QString value)
{
    if (value.isEmpty()) {
        return {};
    }
    value.replace(QRegularExpression(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption),
                  QStringLiteral("\n"));
    value.replace(QRegularExpression(QStringLiteral("</p>"), QRegularExpression::CaseInsensitiveOption),
                  QStringLiteral("\n"));
    value.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    value.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    value.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    value.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    value.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    value.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    value.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    value.replace(QRegularExpression(QStringLiteral("\\n{3,}")), QStringLiteral("\n\n"));
    return value.trimmed();
}

QString errorString(const QJsonObject &payload)
{
    const QString message = firstString(payload, {"error", "msg", "message", "response"});
    return message.isEmpty() ? QStringLiteral("Fightcade request failed.") : message;
}

QString authErrorCode(const QString &serverMessage)
{
    static const QHash<QString, QString> codes = {
        {"user wrong password", "wrong-password"},
        {"too many failed login", "too-many-failed-login"},
        {"login locked", "login-locked"},
        {"user not found", "user-not-found"},
        {"user locked", "user-locked"},
        {"user newaccount muted", "user-newaccount-muted"},
        {"user banned", "user-banned"},
        {"server locked", "server-locked"},
        {"old version", "old-version"}
    };
    return codes.value(serverMessage.toLower(), QStringLiteral("unknown"));
}

QString authErrorMessage(const QString &serverMessage)
{
    static const QHash<QString, QString> messages = {
        {"user wrong password", "Wrong username or password."},
        {"too many failed login", "Too many failed attempts. Wait a few minutes and try again."},
        {"login locked", "Login is temporarily locked."},
        {"user not found", "That Fightcade account does not exist."},
        {"user locked", "That account is locked."},
        {"user newaccount muted", "New accounts cannot chat yet."},
        {"user banned", "That account is banned."},
        {"server locked", "Fightcade is in maintenance mode."},
        {"old version", "Fightcade is too old. Install the latest Linux client or Flatpak."}
    };
    return messages.value(serverMessage.toLower(), serverMessage.isEmpty()
        ? QStringLiteral("Fightcade could not sign you in.")
        : serverMessage);
}

} // namespace FightcadeProtocol
