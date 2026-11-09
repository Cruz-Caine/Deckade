#include "fightcade/flycastlaunch.h"

#include <QCoreApplication>
#include <QDebug>

using namespace FlycastLaunch;

static int failures = 0;

static void expectOptions(const char *label, const QStringList &actual, const QStringList &options)
{
    QStringList expected;
    for (const QString &option : options) {
        expected << QStringLiteral("-config") << option;
    }
    if (actual != expected) {
        qCritical().noquote() << "FAIL" << label << "\n  got " << actual.join(' ')
                              << "\n  want" << expected.join(' ');
        ++failures;
    }
}

static MatchRoute route(bool hosting, int ranked)
{
    MatchRoute value;
    value.game = QStringLiteral("flycast_ausfache");
    value.quarkId = QStringLiteral("1785-4901");
    value.hosting = hosting;
    value.delay = 2;
    value.ranked = ranked;
    value.server = QStringLiteral("ggpo.fightcade.com");
    value.spectatorPort = 7001;
    value.localPort = 6006;
    value.remotePort = 41000;
    return value;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    expectOptions("local", localArguments(QStringLiteral("flycast_ausfache")),
                  {"dojo:GameEntry=ausfache", "dojo:TestGame=yes", "dojo:Enable=no", "dojo:EnableMatchCode=no"});

    expectOptions("host unranked", matchArguments(route(true, 0)),
                  {"dojo:EnableMatchCode=no", "dojo:Enable=yes", "network:ActAsServer=yes", "network:GGPO=yes",
                   "network:server=0.0.0.0", "network:GGPORemotePort=41000", "network:GGPOPort=6006",
                   "dojo:Quark=1785-4901", "dojo:Transmitting=yes", "dojo:SpectatorIP=ggpo.fightcade.com",
                   "dojo:SpectatorPort=7001", "network:GGPODelay=2", "dojo:GameEntry=ausfache",
                   "dojo:HostJoinSelect=no"});

    expectOptions("guest ranked", matchArguments(route(false, 3)),
                  {"dojo:EnableMatchCode=no", "dojo:Enable=yes", "network:ActAsServer=no", "network:GGPO=yes",
                   "network:server=127.0.0.1", "network:GGPORemotePort=41000", "network:GGPOPort=6006",
                   "dojo:Quark=1785-4901", "dojo:SpectatorIP=ggpo.fightcade.com", "dojo:SpectatorPort=7001",
                   "network:GGPODelay=2", "dojo:TransmitScore=yes", "dojo:FirstTo=3", "dojo:GameEntry=ausfache",
                   "dojo:HostJoinSelect=no"});

    MatchRoute negative = route(false, 0);
    negative.delay = -1;
    if (!matchArguments(negative).contains(QStringLiteral("network:GGPODelay=0"))) {
        qCritical() << "FAIL negative delay must clamp to 0";
        ++failures;
    }

    expectOptions("spectate",
                  spectateArguments(QStringLiteral("flycast_ausfache"), QStringLiteral("1785-4901"),
                                    QStringLiteral("ggpo.fightcade.com"), 7001),
                  {"dojo:Receiving=yes", "dojo:ActAsServer=no", "dojo:SpectatorIP=ggpo.fightcade.com",
                   "dojo:SpectatorPort=7001", "dojo:Quark=1785-4901", "dojo:GameEntry=ausfache", "dojo:Enable=yes",
                   "dojo:EnableMatchCode=no", "dojo:RxFrameBuffer=180"});

    qInfo() << "flycast launch:" << failures << "failure(s)";
    return failures ? 1 : 0;
}
