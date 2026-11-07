#include "netplaypreflight.h"

#include <QHostInfo>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QThread>

namespace NetplayPreflight {
QStringList warnings(const QString &masterHost, quint16 masterPort, int proxyPort)
{
    QStringList result;
    bool active = false;
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        if (interface.flags().testFlag(QNetworkInterface::IsUp)
            && !interface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            active = true;
            break;
        }
    }
    if (!active) result << QStringLiteral("No active network interface was detected.");

    QTcpSocket socket;
    socket.connectToHost(masterHost, masterPort);
    if (!socket.waitForConnected(1500)) {
        result << QStringLiteral("Fightcade GGPO master %1:%2 was not reachable during preflight.")
                      .arg(masterHost).arg(masterPort);
    }
    socket.abort();
    if (proxyPort < 7001 || proxyPort > 7009) {
        result << QStringLiteral("Local UDP proxy selected an unexpected port: %1.").arg(proxyPort);
    }
    return result;
}
} // namespace NetplayPreflight
