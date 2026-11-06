#include "fightcade/protocol.h"
#include "fightcade/userstatus.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

using namespace FightcadeProtocol;

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { qCritical().noquote() << "FAIL line" << __LINE__ << #cond; ++failures; } \
} while (0)

#define EQ(actual, expected) do { \
    const auto deckadeActual = (actual); \
    const auto deckadeExpected = (expected); \
    if (!(deckadeActual == deckadeExpected)) { \
        qCritical().noquote() << "FAIL line" << __LINE__ << #actual \
            << "actual=" << deckadeActual << "expected=" << deckadeExpected; \
        ++failures; \
    } \
} while (0)

static QJsonObject obj(const char *json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

static void launcherTokenReadsNestedUserToken()
{
    const QJsonObject payload = obj(R"({"user":{"name":"player","token":"live-token"}})");
    EQ(findLauncherToken(payload), QStringLiteral("live-token"));
}

static void autologinRecognitionRequiresEnvelope()
{
    const QJsonObject login = obj(R"({"req":"autologin","requestIdx":-1,"result":200,"user":{"name":"player","token":"token"}})");
    const QJsonObject unsolicited = obj(R"({"req":"join","user":{"name":"opponent"}})");
    CHECK(isAutologinResponse(login));
    CHECK(!isAutologinResponse(unsolicited));
}

static void startEventParsesNullRankedAsUnranked()
{
    const QJsonObject payload = obj(R"JSON({
        "req":"start",
        "channelname":"Street Fighter III 3rd Strike: Fight for the Future (Japan 990512, NO CD)",
        "delay":1,"gameid":"sfiii3nr1","playerid":1,"port":7001,
        "quarkid":"1785013981484-4901","ranked":null,"token":"live-token"
    })JSON");
    const QVariantMap match = normalizeMatchStart(payload);
    EQ(match.value("channelName").toString(), QStringLiteral("Street Fighter III 3rd Strike: Fight for the Future (Japan 990512, NO CD)"));
    EQ(match.value("gameId").toString(), QStringLiteral("sfiii3nr1"));
    EQ(match.value("quarkId").toString(), QStringLiteral("1785013981484-4901"));
    EQ(match.value("playerId").toInt(), 1);
    EQ(match.value("port").toInt(), 7001);
    EQ(match.value("ranked").toInt(), 0);
    EQ(match.value("delay").toInt(), 1);
    EQ(match.value("token").toString(), QStringLiteral("live-token"));
}

static void usersParsePlayingFalseAsChallengeableState()
{
    const QJsonObject payload = obj(R"({"users":[{
        "name":"opponent","away":false,"playing":false,"wlan":true,"vping":37,
        "proxy":true,"ranked":3,"matches":21,"region":"Region Free",
        "gameid":"sfiii3n","quarkid":"1785073365625-9431","port":7001,
        "prevent_bad_challenges":true,"prevent_wifi_challenges":true
    }]})");
    const QVariantList users = normalizeUsers(payload.value("users").toArray(), QString());
    EQ(users.size(), 1);
    const QVariantMap user = users.first().toMap();
    EQ(user.value("away").toBool(), false);
    CHECK(user.value("playing").toMap().isEmpty());
    EQ(user.value("isWifi").toBool(), true);
    EQ(user.value("virtualPing").toInt(), 37);
    EQ(user.value("isProxy").toBool(), true);
    EQ(user.value("rankedSetting").toInt(), 3);
    EQ(user.value("matchCount").toInt(), 21);
    EQ(user.value("region").toString(), QStringLiteral("Region Free"));
    EQ(user.value("preventsBadChallenges").toBool(), true);
    EQ(user.value("preventsWifiChallenges").toBool(), true);
}

static void usersAttachStreamsFromJoinQuarks()
{
    const QJsonObject payload = obj(R"({
        "gameid":"kof98",
        "users":[{"name":"Mahaloz","playing":false},{"name":"idle","playing":false}],
        "quarks":[{"players":["Mahaloz","yulanxl"],"port":7001,"quarkid":"1785073071864-8625"}]
    })");
    const QVariantMap channel = normalizeJoinedChannel(payload);
    const QVariantList users = channel.value("users").toList();
    EQ(users.size(), 2);
    const QVariantMap first = users.at(0).toMap();
    EQ(first.value("playing").toMap().value("quarkId").toString(), QStringLiteral("1785073071864-8625"));
    EQ(first.value("stream").toMap().value("gameID").toString(), QStringLiteral("kof98"));
    EQ(first.value("stream").toMap().value("port").toInt(), 7001);
    const QVariantMap second = users.at(1).toMap();
    CHECK(second.value("playing").toMap().isEmpty());
    CHECK(second.value("stream").toMap().isEmpty());
}

static void usersInferPlayingFromNestedGameStream()
{
    const QJsonObject payload = obj(R"({"users":[{
        "name":"opponent","game":{"gameid":"sfiii3n","quarkid":"1785073365625-9431","port":7001}
    }]})");
    const QVariantList users = normalizeUsers(payload.value("users").toArray(), QString());
    EQ(users.size(), 1);
    EQ(users.first().toMap().value("playing").toMap().value("quarkId").toString(),
       QStringLiteral("1785073365625-9431"));
}

static void challengeFailureParsesRestrictionWarning()
{
    const QJsonObject payload = obj(R"({
        "req":"challenge","username":"opponent","channelname":"sfiii3n",
        "challengeid":7,"result":403,"response":"Cannot challenge this user because of ping restrictions."
    })");
    CHECK(isChallengeWarning(payload, "challenge"));
    EQ(challengeWarningMessage(payload), QStringLiteral("Cannot challenge this user because of ping restrictions."));
}

static void brokenChallengeWarningParsesRestrictionWarning()
{
    const QJsonObject payload = obj(R"({
        "req":"broken","channelname":"sfiii3n","chat":"Cannot challenge this user from your region."
    })");
    CHECK(isChallengeWarning(payload, "broken"));
    EQ(challengeWarningMessage(payload), QStringLiteral("Cannot challenge this user from your region."));
}

static void channelParserSupportsDirectAndSectionedResultsOnly()
{
    const QJsonObject direct = obj(R"({"results":[{"name":"KOF 98","gameid":"kof98","clients":291,"emulator":"fbneo"}]})");
    const QJsonObject sectioned = obj(R"({"results":[{
        "title":"Popular",
        "channels":[{"name":"3rd Strike","gameid":"sfiii3n","clients":100,"emulator":"fbneo"}],
        "events":[{"name":"Not a channel","gameid":"event-game"}]
    }]})");
    EQ(normalizeChannelList(direct).size(), 1);
    EQ(normalizeChannelList(direct).first().toMap().value("gameId").toString(), QStringLiteral("kof98"));
    const QVariantList channels = normalizeChannelList(sectioned);
    EQ(channels.size(), 1);
    EQ(channels.first().toMap().value("gameId").toString(), QStringLiteral("sfiii3n"));
}

static void welcomePreservesOrderTypesAndFavorites()
{
    const QJsonObject payload = obj(R"({"results":[
        {"title":"Popular games","channels":[{"name":"KOF 98","gameid":"kof98","clients":291,"system":"Arcade FC2","emulator":"fbneo","ranked":true,"training":true}]},
        {"title":"Browse categories","categories":["Popular","My Favorites","Fighter","NAOMI"]},
        {"title":"Upcoming Events","events":[{"name":"Tournament","author":"organizer","date":1785610800000,"gameid":"sf2rb4","image":"https://i.imgur.com/1cojbCV.png","link":"https://www.start.gg/example","region":"Region Free","stream":"https://www.twitch.tv/example"}]},
        {"title":"Favorites","channels":[{"name":"3rd Strike","gameid":"sfiii3nr1","system":"Arcade FC2","emulator":"fbneo"}]}
    ]})");
    const QVariantList sections = normalizeWelcome(payload);
    EQ(sections.size(), 4);
    EQ(sections.at(0).toMap().value("title").toString(), QStringLiteral("Popular games"));
    EQ(sections.at(0).toMap().value("channels").toList().first().toMap().value("gameId").toString(), QStringLiteral("kof98"));
    EQ(sections.at(1).toMap().value("categories").toList().size(), 4);
    EQ(sections.at(2).toMap().value("events").toList().first().toMap().value("imageUrl").toString(), QStringLiteral("https://i.imgur.com/1cojbCV.png"));
    EQ(sections.at(3).toMap().value("channels").toList().first().toMap().value("isFavorite").toBool(), true);
}

static void previewUrlsUseCurrentAssetsAndStripFc1Prefix()
{
    EQ(previewUrl("kof98", "KOF 98"), QStringLiteral("https://web.fightcade.com/static/previews/kof98.png"));
    EQ(previewUrl("fc1_sfiii3n", "3rd Strike"), QStringLiteral("https://web.fightcade.com/static/previews/sfiii3n.png"));
}

static void quarkCommandsMatchLauncherContract()
{
    EQ(quarkServed("sfiii3n", "1234567890-42", 0, 7000, 2, 3),
       QStringLiteral("quark:served,sfiii3n,1234567890-42.0,7000,2,3"));
    EQ(quarkStream("sfiii3n", "1234567890-42.2", 7001),
       QStringLiteral("quark:stream,sfiii3n,1234567890-42.2,7001"));
    EQ(quarkTraining("sfiii3n", "1234567890-42", 1, 7000, 2),
       QStringLiteral("quark:training,sfiii3n,1234567890-42.1,7000,2"));
    EQ(quarkDirect("sfiii3n", 6000, "203.0.113.20", 6001, 0, 2, 0),
       QStringLiteral("quark:direct,sfiii3n,6000,203.0.113.20,6001,0,2,0"));
}

static void userStatusHashMatchesOfficialFormula()
{
    EQ(FightcadeUserStatus::hash("tok", "uuid", "guid", "stcable", "huid"),
       QStringLiteral("a766b975ff83d1c99d33dbefa38235e6"));
    const QJsonObject payload = FightcadeUserStatus::payload("tok");
    EQ(payload.value("req").toString(), QStringLiteral("userstatus"));
    EQ(payload.value("version").toInt(), 6);
    EQ(payload.value("userstatus").toString(), QStringLiteral("stcable"));
}


static void requestCorrelationUsesRequestIdxNotRosterIdx()
{
    const QJsonObject payload = obj(R"({"req":"join","requestIdx":2,"idx":-1})");
    EQ(intValue(payload.value("requestIdx")), 2);
    EQ(intValue(payload.value("idx")), -1);
}

static void quarkStatsParsesStreamDetails()
{
    const QJsonObject quark = obj(R"({"players":["Mahaloz","yulanxl"],"port":7001,"quarkid":"1785073071864-8625","spectators":2})");
    const QVariantMap stream = normalizeQuark(quark);
    EQ(stream.value("quarkId").toString(), QStringLiteral("1785073071864-8625"));
    EQ(stream.value("port").toInt(), 7001);
    EQ(stream.value("spectators").toInt(), 2);
    EQ(stream.value("players").toList().size(), 2);
}

static void sessionUserParsesPerksAndNestedToken()
{
    const QJsonObject payload = obj(R"({
        "name":"player","display_name":"Player","token":"nested-token",
        "active_channels":[{"channelname":"kof98","away":true}],
        "settings":{"ranked":true,"auto_afk":false},
        "perks":{"maxNumChannels":3}
    })");
    const QVariantMap user = normalizeSessionUser(payload);
    EQ(user.value("name").toString(), QStringLiteral("player"));
    EQ(user.value("token").toString(), QStringLiteral("nested-token"));
    EQ(user.value("activeChannels").toList().size(), 1);
    EQ(user.value("settings").toMap().value("autoAfk").toBool(), false);
    EQ(user.value("perks").toMap().value("maxNumChannels").toInt(), 3);
}

static void plainTextStripsHtmlAndEntities()
{
    EQ(plainText(QStringLiteral("<p>Hello<br>world &amp; friends</p>")),
       QStringLiteral("Hello\nworld & friends"));
}

static void romMetadataHasDependenciesForFightcadeGames()
{
    QFile file(QStringLiteral(DECKADE_SOURCE_DIR "/assets/rom-json/fbneo_roms.json"));
    CHECK(file.open(QIODevice::ReadOnly));
    const QJsonObject entries = QJsonDocument::fromJson(file.readAll()).object();
    CHECK(entries.contains("sfiii3n"));
    CHECK(entries.value("sfiii3n").toObject().value("require").toArray().contains("sfiii3"));
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    launcherTokenReadsNestedUserToken();
    autologinRecognitionRequiresEnvelope();
    startEventParsesNullRankedAsUnranked();
    usersParsePlayingFalseAsChallengeableState();
    usersAttachStreamsFromJoinQuarks();
    usersInferPlayingFromNestedGameStream();
    challengeFailureParsesRestrictionWarning();
    brokenChallengeWarningParsesRestrictionWarning();
    channelParserSupportsDirectAndSectionedResultsOnly();
    welcomePreservesOrderTypesAndFavorites();
    previewUrlsUseCurrentAssetsAndStripFc1Prefix();
    requestCorrelationUsesRequestIdxNotRosterIdx();
    quarkStatsParsesStreamDetails();
    sessionUserParsesPerksAndNestedToken();
    plainTextStripsHtmlAndEntities();
    quarkCommandsMatchLauncherContract();
    userStatusHashMatchesOfficialFormula();
    romMetadataHasDependenciesForFightcadeGames();
    if (failures == 0) {
        qInfo() << "All Deckade protocol tests passed.";
        return 0;
    }
    qCritical() << failures << "protocol test(s) failed.";
    return 1;
}
