#pragma once

#include <QString>
#include <QStringList>

// Command lines for Flycast Dojo, matching how official Fightcade starts it so
// Deckade players stay in sync with official clients.
namespace FlycastLaunch {

struct MatchRoute {
    QString game;
    QString quarkId;
    bool hosting = false;
    int delay = 0;
    int ranked = 0;
    QString server;
    int spectatorPort = 0;
    int localPort = 0;
    int remotePort = 0;
};

QString gameEntry(const QString &gameId);
QStringList localArguments(const QString &gameId);
QStringList matchArguments(const MatchRoute &route);
QStringList spectateArguments(const QString &gameId, const QString &quarkId,
                              const QString &server, int port);

} // namespace FlycastLaunch
