#include "flycastlaunch.h"

#include <QtGlobal>

namespace {

void addOption(QStringList &arguments, const QString &option)
{
    arguments << QStringLiteral("-config") << option;
}

} // namespace

namespace FlycastLaunch {

QString gameEntry(const QString &gameId)
{
    const QString prefix = QStringLiteral("flycast_");
    return gameId.startsWith(prefix) ? gameId.mid(prefix.size()) : gameId;
}

QStringList localArguments(const QString &gameId)
{
    QStringList arguments;
    addOption(arguments, QStringLiteral("dojo:GameEntry=%1").arg(gameEntry(gameId)));
    addOption(arguments, QStringLiteral("dojo:TestGame=yes"));
    addOption(arguments, QStringLiteral("dojo:Enable=no"));
    addOption(arguments, QStringLiteral("dojo:EnableMatchCode=no"));
    return arguments;
}

QStringList matchArguments(const MatchRoute &route)
{
    QStringList arguments;
    addOption(arguments, QStringLiteral("dojo:EnableMatchCode=no"));
    addOption(arguments, QStringLiteral("dojo:Enable=yes"));
    addOption(arguments, QStringLiteral("network:ActAsServer=%1")
                             .arg(route.hosting ? QStringLiteral("yes") : QStringLiteral("no")));
    addOption(arguments, QStringLiteral("network:GGPO=yes"));
    addOption(arguments, QStringLiteral("network:server=%1")
                             .arg(route.hosting ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1")));
    addOption(arguments, QStringLiteral("network:GGPORemotePort=%1").arg(route.remotePort));
    addOption(arguments, QStringLiteral("network:GGPOPort=%1").arg(route.localPort));
    addOption(arguments, QStringLiteral("dojo:Quark=%1").arg(route.quarkId));
    if (route.hosting) {
        addOption(arguments, QStringLiteral("dojo:Transmitting=yes"));
    }
    addOption(arguments, QStringLiteral("dojo:SpectatorIP=%1").arg(route.server));
    addOption(arguments, QStringLiteral("dojo:SpectatorPort=%1").arg(route.spectatorPort));
    addOption(arguments, QStringLiteral("network:GGPODelay=%1").arg(qMax(0, route.delay)));
    if (route.ranked > 0) {
        addOption(arguments, QStringLiteral("dojo:TransmitScore=yes"));
        addOption(arguments, QStringLiteral("dojo:FirstTo=%1").arg(route.ranked));
    }
    addOption(arguments, QStringLiteral("dojo:GameEntry=%1").arg(gameEntry(route.game)));
    addOption(arguments, QStringLiteral("dojo:HostJoinSelect=no"));
    return arguments;
}

QStringList spectateArguments(const QString &gameId, const QString &quarkId,
                              const QString &server, int port)
{
    QStringList arguments;
    addOption(arguments, QStringLiteral("dojo:Receiving=yes"));
    addOption(arguments, QStringLiteral("dojo:ActAsServer=no"));
    addOption(arguments, QStringLiteral("dojo:SpectatorIP=%1").arg(server));
    addOption(arguments, QStringLiteral("dojo:SpectatorPort=%1").arg(port));
    addOption(arguments, QStringLiteral("dojo:Quark=%1").arg(quarkId));
    addOption(arguments, QStringLiteral("dojo:GameEntry=%1").arg(gameEntry(gameId)));
    addOption(arguments, QStringLiteral("dojo:Enable=yes"));
    addOption(arguments, QStringLiteral("dojo:EnableMatchCode=no"));
    addOption(arguments, QStringLiteral("dojo:RxFrameBuffer=180"));
    return arguments;
}

} // namespace FlycastLaunch
