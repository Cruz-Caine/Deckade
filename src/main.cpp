#include "core/appcontroller.h"
#include "core/matchhistory.h"
#include "core/friendsstore.h"
#include "core/focuscontroller.h"
#include "core/gamepadmanager.h"
#include "core/settingsstore.h"
#include "core/theme.h"
#include "fightcade/fightcadeclient.h"
#include "fightcade/fightcadelauncher.h"
#include "fightcade/romlibrary.h"
#include "fightcade/controllerprofile.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Deckade"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("deckade.local"));
    QCoreApplication::setApplicationName(QStringLiteral("Deckade"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Deckade"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/icons/deckade.svg")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    SettingsStore settings;
    const bool startWindowed = application.arguments().contains(QStringLiteral("--windowed"));

    Theme theme;
    GamepadManager gamepad;
    gamepad.selectController(settings.controllerIndex());
    FocusController focusController;
    FightcadeClient client;
    FightcadeLauncher launcher(&settings);
    RomLibrary roms(&launcher, &settings);
    ControllerProfile controllerProfile(&launcher, &settings);
    MatchHistory matchHistory;
    FriendsStore friendsStore;
    AppController appController(&settings, &launcher, &application);

    QObject::connect(&client, &FightcadeClient::loggedIn, &settings,
                     [&settings, &client](const QVariantMap &user, const QVariantMap &) {
                         if (!settings.rememberLogin()) {
                             settings.clearSession();
                             return;
                         }
                         settings.saveSession(client.sessionCookie(),
                                              user.value(QStringLiteral("name")).toString(),
                                              user.value(QStringLiteral("displayName")).toString());
                     });
    QObject::connect(&application, &QGuiApplication::aboutToQuit, &launcher,
                     &FightcadeLauncher::killEmulator);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("settings"), &settings);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("gamepad"), &gamepad);
    engine.rootContext()->setContextProperty(QStringLiteral("focusController"), &focusController);
    engine.rootContext()->setContextProperty(QStringLiteral("client"), &client);
    engine.rootContext()->setContextProperty(QStringLiteral("launcher"), &launcher);
    engine.rootContext()->setContextProperty(QStringLiteral("roms"), &roms);
    engine.rootContext()->setContextProperty(QStringLiteral("controllerProfile"), &controllerProfile);
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &appController);
    engine.rootContext()->setContextProperty(QStringLiteral("matchHistory"), &matchHistory);
    engine.rootContext()->setContextProperty(QStringLiteral("friendsStore"), &friendsStore);
    engine.rootContext()->setContextProperty(QStringLiteral("startWindowed"), startWindowed);

    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return application.exec();
}
