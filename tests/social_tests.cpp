#include "core/matchhistory.h"
#include "core/friendsstore.h"
#include "fightcade/netplaypreflight.h"

#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QSettings>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { qCritical() << "FAIL" << __LINE__ << #cond; ++failures; } } while (0)

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication app(argc, argv);
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("deckade"), QStringLiteral("deckade"));
        settings.clear();
        settings.sync();
    }

    MatchHistory history;
    history.clear();
    history.recordMatch({{"channelName", "sfiii3nr1"}, {"gameId", "sfiii3nr1"},
                         {"opponent", "testingtop"}, {"ranked", 3}});
    CHECK(history.matches().size() == 1);
    CHECK(history.matches().first().toMap().value("opponent").toString() == QStringLiteral("testingtop"));
    history.recordMatch({{"channelName", "kof98"}, {"gameId", "kof98"},
                         {"opponent", "other"}, {"ranked", 0}});
    CHECK(history.matches().size() == 2);
    CHECK(history.matches().first().toMap().value("channelName").toString() == QStringLiteral("kof98"));
    history.clear();
    CHECK(history.matches().isEmpty());

    FriendsStore friends;
    friends.addFriend("testingtop");
    CHECK(friends.isFriend("TestingTop"));
    friends.toggleFriend("testingtop");
    CHECK(!friends.isFriend("testingtop"));

    const QStringList warnings = NetplayPreflight::warnings(QStringLiteral("127.0.0.1"), 1, 7001);
    CHECK(warnings.size() >= 1);

    if (failures) { qCritical() << failures << "social test(s) failed"; return 1; }
    qInfo() << "Match history, friends, and preflight tests passed.";
    return 0;
}
