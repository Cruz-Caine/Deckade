#include "core/settingsstore.h"
#include "fightcade/fightcadelauncher.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    SettingsStore settings;
    FightcadeLauncher launcher(&settings);
    launcher.refreshInstall();

    qInfo().noquote() << "INSTALL" << launcher.installMethod() << launcher.installDetail();
    qInfo().noquote() << "ROMS" << launcher.romPath();
    qInfo().noquote() << "RUNTIME_ROOT" << launcher.runtimeRoot();
    for (const QVariant &entry : launcher.runtimeSummary()) {
        const QVariantMap runtime = entry.toMap();
        qInfo().noquote() << "RUNTIME" << runtime.value("name").toString()
                          << runtime.value("available").toBool()
                          << runtime.value("binary").toString();
    }

    qInfo() << "HAS_FLYCAST_AUSFACHE" << launcher.hasLocalRom(QStringLiteral("flycast"), QStringLiteral("flycast_ausfache"));
    if (qEnvironmentVariableIsSet("DECKADE_CHECK_ONLY")) return 0;
    const QString game = qEnvironmentVariable("DECKADE_TEST_GAME", "kof98");
    const QString mode = qEnvironmentVariable("DECKADE_TEST_MODE", "solo");
    const QString emulator = qEnvironmentVariable("DECKADE_TEST_EMULATOR", "fbneo");
    const bool started = mode == QStringLiteral("training")
        ? launcher.launchTraining(emulator, game)
        : launcher.launchSolo(emulator, game);
    qInfo() << "LAUNCH_STARTED" << started << "mode=" << mode << "emulator=" << emulator << "game=" << game << launcher.lastError();

    QTimer::singleShot(4000, &app, [&] {
        qInfo() << "AFTER_WAIT running=" << launcher.running() << "error=" << launcher.lastError();
        launcher.killEmulator();
        QTimer::singleShot(800, &app, [&] { app.exit(0); });
    });
    return app.exec();
}
