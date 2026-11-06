#include "core/settingsstore.h"
#include "fightcade/fightcadelauncher.h"
#include "fightcade/romlibrary.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication app(argc, argv);

    const QString root = QStringLiteral("/tmp/deckade-rom-probe");
    QDir(root).removeRecursively();
    QDir().mkpath(root);

    SettingsStore settings;
    settings.setRomPath(root);
    settings.setAutoDownloadRoms(true);
    FightcadeLauncher launcher(&settings);
    launcher.refreshInstall();
    RomLibrary roms(&launcher, &settings);

    QObject::connect(&roms, &RomLibrary::stateChanged, [&] {
        qInfo().noquote() << "STATE" << roms.busy() << roms.status() << roms.activeGameId();
    });
    QObject::connect(&roms, &RomLibrary::progressChanged, [&] {
        qInfo().noquote() << "PROGRESS" << roms.progress();
    });
    QObject::connect(&roms, &RomLibrary::romReady, [&](const QString &emulator, const QString &gameId, const QString &path) {
        qInfo().noquote() << "READY" << emulator << gameId << path;
        app.exit(0);
    });
    QObject::connect(&roms, &RomLibrary::romFailed, [&](const QString &emulator, const QString &gameId, const QString &message) {
        qWarning().noquote() << "FAILED" << emulator << gameId << message;
        if (!message.contains("cancelled", Qt::CaseInsensitive)) app.exit(1);
    });

    qInfo() << "ROM_ROOT" << launcher.romDirectoryFor(QStringLiteral("fbneo"));
    roms.ensureRom(QStringLiteral("fbneo"), QStringLiteral("88games"));

    QTimer::singleShot(1200, &app, [&] {
        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString file = it.next();
            qInfo().noquote() << "PARTIAL" << file << QFileInfo(file).size();
        }
        qInfo() << "CANCELLING after partial download";
        roms.cancel();
    });
    QTimer::singleShot(1800, &app, [&] {
        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        int files = 0;
        while (it.hasNext()) { qInfo().noquote() << "LEFTOVER" << it.next(); ++files; }
        qInfo() << "LEFTOVER_COUNT" << files;
        app.exit(files == 0 ? 0 : 2);
    });
    return app.exec();
}
