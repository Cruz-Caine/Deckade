// Renders Deckade's screens with sample data and saves them as PNGs.
//
//   QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software deckade_ui_preview <out-dir>
#include "core/appcontroller.h"
#include "core/focuscontroller.h"
#include "core/friendsstore.h"
#include "core/gamepadmanager.h"
#include "core/matchhistory.h"
#include "core/settingsstore.h"
#include "core/theme.h"
#include "fightcade/controllerprofile.h"
#include "fightcade/fightcadeclient.h"
#include "fightcade/fightcadelauncher.h"
#include "fightcade/romlibrary.h"

#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

static const char *kFixture = R"({
 "user":{"name":"couchplayer","displayName":"Couch Player","favoritesChannels":["sfiii3nr1"]},
 "rooms":[
  {"name":"kof98","title":"The King of Fighters '98","system":"Neo Geo","gameId":"kof98","emulator":"fbneo","playerCount":142,"isRanked":true,"supportsTraining":true,
   "previewUrl":"https://web.fightcade.com/static/previews/kof98.png",
   "users":[{"name":"couchplayer","rank":3,"ping":12,"country":{"code":"us"}},{"name":"Iori_main","rank":5,"ping":48,"country":{"code":"jp"}},{"name":"terry_b","rank":2,"ping":88,"country":{"code":"br"}},{"name":"away_guy","rank":1,"ping":40,"away":true,"country":{"code":"de"}}],
   "quarks":[{"quarkId":"1","players":["Iori_main","terry_b"],"spectators":4}]},
  {"name":"sfiii3nr1","title":"Street Fighter III: 3rd Strike","system":"CPS3","gameId":"sfiii3nr1","emulator":"fbneo","playerCount":388,"isRanked":true,"previewUrl":"https://web.fightcade.com/static/previews/sfiii3nr1.png","users":[]},
  {"name":"ssf2xjr1","title":"Super Street Fighter II X","system":"CPS2","gameId":"ssf2xjr1","emulator":"fbneo","playerCount":71,"previewUrl":"https://web.fightcade.com/static/previews/ssf2xjr1.png","users":[]}
 ],
 "more":[
  {"name":"garou","title":"Garou: Mark of the Wolves","system":"Neo Geo","gameId":"garou","emulator":"fbneo","playerCount":96,"isRanked":true,"previewUrl":"https://web.fightcade.com/static/previews/garou.png"},
  {"name":"vsav","title":"Vampire Savior","system":"CPS2","gameId":"vsav","emulator":"fbneo","playerCount":54,"previewUrl":"https://web.fightcade.com/static/previews/vsav.png"},
  {"name":"mvsc","title":"Marvel vs. Capcom","system":"CPS2","gameId":"mvsc","emulator":"fbneo","playerCount":33,"previewUrl":"https://web.fightcade.com/static/previews/mvsc.png"},
  {"name":"kof2002","title":"The King of Fighters 2002","system":"Neo Geo","gameId":"kof2002","emulator":"fbneo","playerCount":120,"previewUrl":"https://web.fightcade.com/static/previews/kof2002.png"}
 ]
})";

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("DeckadePreview"));
    QCoreApplication::setApplicationName(QStringLiteral("DeckadePreview"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    const QString outDir = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral(".");
    QDir().mkpath(outDir);

    SettingsStore settings;
    // Preview screenshots must never sign in with a developer's remembered
    // session or touch Fightcade; the mock data below is all they need.
    settings.clearSession();
    settings.setRememberLogin(false);
    Theme theme;
    GamepadManager gamepad;
    FocusController focusController;
    FightcadeClient client;
    FightcadeLauncher launcher(&settings);
    RomLibrary roms(&launcher, &settings);
    ControllerProfile controllerProfile(&launcher, &settings);
    MatchHistory matchHistory;
    FriendsStore friendsStore;
    AppController appController(&settings, &launcher, &application);

    QQmlApplicationEngine engine;
    QQmlContext *context = engine.rootContext();
    context->setContextProperty(QStringLiteral("settings"), &settings);
    context->setContextProperty(QStringLiteral("theme"), &theme);
    context->setContextProperty(QStringLiteral("gamepad"), &gamepad);
    context->setContextProperty(QStringLiteral("focusController"), &focusController);
    context->setContextProperty(QStringLiteral("client"), &client);
    context->setContextProperty(QStringLiteral("launcher"), &launcher);
    context->setContextProperty(QStringLiteral("roms"), &roms);
    context->setContextProperty(QStringLiteral("controllerProfile"), &controllerProfile);
    context->setContextProperty(QStringLiteral("appController"), &appController);
    context->setContextProperty(QStringLiteral("matchHistory"), &matchHistory);
    context->setContextProperty(QStringLiteral("friendsStore"), &friendsStore);
    context->setContextProperty(QStringLiteral("startWindowed"), true);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    bool widthOk = false;
    bool heightOk = false;
    const int previewWidth = qEnvironmentVariableIntValue("DECKADE_PREVIEW_WIDTH", &widthOk);
    const int previewHeight = qEnvironmentVariableIntValue("DECKADE_PREVIEW_HEIGHT", &heightOk);
    window->resize(widthOk && previewWidth > 0 ? previewWidth : 1280,
                   heightOk && previewHeight > 0 ? previewHeight : 800);
    QObject *shell = window->findChild<QObject *>(QStringLiteral("shell"));
    QObject *login = window->findChild<QObject *>(QStringLiteral("loginScreen"));
    QObject *challengeDialog = window->findChild<QObject *>(QStringLiteral("challengeDialog"));

    const QVariantMap fixture = QJsonDocument::fromJson(kFixture).toVariant().toMap();
    const QVariantList rooms2 = fixture.value(QStringLiteral("rooms")).toList();
    QVariantList all = rooms2;
    all += fixture.value(QStringLiteral("more")).toList();

    struct Shot { QString name; QString screen; QString inputMode; };
    const QList<Shot> shots = {
        {QStringLiteral("login"), QString(), QStringLiteral("controller")},
        {QStringLiteral("restoring"), QString(), QStringLiteral("controller")},
        {QStringLiteral("home"), QStringLiteral("home"), QStringLiteral("controller")},
        {QStringLiteral("browse"), QStringLiteral("browse"), QStringLiteral("keyboard")},
        {QStringLiteral("room"), QStringLiteral("room"), QStringLiteral("controller")},
        {QStringLiteral("challenge"), QStringLiteral("room"), QStringLiteral("controller")},
        {QStringLiteral("controls"), QStringLiteral("controls"), QStringLiteral("controller")},
        {QStringLiteral("settings"), QStringLiteral("settings"), QStringLiteral("controller")},
    };

    auto index = std::make_shared<int>(0);
    auto *timer = new QTimer(&application);
    timer->setInterval(2500);
    auto apply = [=] {
        const Shot &shot = shots.at(*index);
        if (shot.screen.isEmpty()) {
            // Authentication screens: show the login page (or its restoring
            // state) instead of leaving it hidden behind the shell.
            shell->setProperty("visible", false);
            login->setProperty("visible", true);
            window->setProperty("loginBusy", shot.name == QStringLiteral("restoring"));
            window->setProperty("loginBusyText", QStringLiteral("Restoring session"));
            window->setProperty("restoringName", QStringLiteral("couchplayer"));
            window->setProperty("loginError", QString());
            return;
        }
        login->setProperty("visible", false);
        shell->setProperty("visible", true);
        window->setProperty("user", fixture.value(QStringLiteral("user")));
        window->setProperty("joinedChannels", rooms2);
        window->setProperty("allChannels", all);
        window->setProperty("browseChannels", all);
        window->setProperty("browseTotal", all.size());
        window->setProperty("unread", QVariantMap{{QStringLiteral("sfiii3nr1"), 3}});
        window->setProperty("currentChannel", rooms2.first());
        window->setProperty("inputMode", shot.inputMode);
        window->setProperty("screen", shot.screen);
        if (shot.name == QStringLiteral("challenge")) {
            const QVariantMap room = rooms2.first().toMap();
            const QVariantMap player = room.value(QStringLiteral("users")).toList().first().toMap();
            const QVariantMap challenge = {
                {QStringLiteral("challenger"), player.value(QStringLiteral("name"))},
                {QStringLiteral("channelName"), room.value(QStringLiteral("name"))},
                {QStringLiteral("channelTitle"), room.value(QStringLiteral("title"))},
                {QStringLiteral("gameId"), room.value(QStringLiteral("gameId"))},
                {QStringLiteral("challengeId"), 1},
                {QStringLiteral("ranked"), 3}
            };
            window->setProperty("challengeIncoming", true);
            window->setProperty("challengeDialogVisible", true);
            window->setProperty("challengeTarget", player);
            window->setProperty("challengeData", challenge);
            QMetaObject::invokeMethod(challengeDialog, "open",
                Q_ARG(QVariant, true), Q_ARG(QVariant, player),
                Q_ARG(QVariant, challenge), Q_ARG(QVariant, 3));
        } else if (challengeDialog && challengeDialog->property("visible").toBool()) {
            QMetaObject::invokeMethod(challengeDialog, "close");
        }
    };
    QObject::connect(timer, &QTimer::timeout, &application, [=, &application] {
        const Shot &shot = shots.at(*index);
        window->grabWindow().save(QDir(outDir).filePath(shot.name + QStringLiteral(".png")));
        if (++*index >= shots.size()) {
            application.exit(0);
            return;
        }
        apply();
    });
    apply();
    timer->start();
    return application.exec();
}
