// Drives the real QML with D-pad moves and lobby updates, without a display.
// Covers the focus rules and the "list jumps to the top" regressions.
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

#include <QEventLoop>
#include <QGuiApplication>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>

namespace {
int failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++failures;
}

void spin(int ms = 200)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

QVariantMap player(const QString &name)
{
    return {{QStringLiteral("name"), name},
            {QStringLiteral("rank"), 1},
            {QStringLiteral("ping"), 30},
            {QStringLiteral("country"), QVariantMap{{QStringLiteral("code"), QStringLiteral("us")}}}};
}

QVariantMap channel(const QString &name, int players, const QVariantList &users = {})
{
    return {{QStringLiteral("name"), name},
            {QStringLiteral("title"), name.toUpper()},
            {QStringLiteral("system"), QStringLiteral("CPS2")},
            {QStringLiteral("gameId"), name},
            {QStringLiteral("emulator"), QStringLiteral("fbneo")},
            {QStringLiteral("playerCount"), players},
            {QStringLiteral("users"), users}};
}

QString playerName(QQuickItem *item)
{
    return item ? item->property("player").toMap().value(QStringLiteral("name")).toString() : QString();
}

qreal sceneY(QQuickItem *item)
{
    return item ? item->mapToScene(QPointF(0, 0)).y() : -1;
}

qreal contentY(QObject *view)
{
    return view->property("contentY").toReal();
}

void systemMessage(QObject *window, const QString &room, int number)
{
    QMetaObject::invokeMethod(window, "appendSystemMessage",
                              Q_ARG(QVariant, QVariant(room)),
                              Q_ARG(QVariant, QVariant(QStringLiteral("message %1").arg(number))));
}
} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // SettingsStore always opens deckade/deckade.ini; keep tests off the real one.
    QTemporaryDir settingsDir;
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());

    SettingsStore settings;
    settings.setLastChannels({QStringLiteral("kof98")});
    settings.setFullscreen(false); // The default would snap to the virtual screen.
    Theme theme;
    GamepadManager gamepad;
    FocusController focus;
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
    context->setContextProperty(QStringLiteral("focusController"), &focus);
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
    window->findChild<QObject *>(QStringLiteral("shell"))->setProperty("visible", true);
    window->findChild<QObject *>(QStringLiteral("loginScreen"))->setProperty("visible", false);
    window->show();
    window->requestActivate();
    spin();

    QVariantList users;
    for (int i = 0; i < 30; ++i) users.append(player(QStringLiteral("player%1").arg(i, 2, 10, QChar('0'))));
    const QVariantMap room = channel(QStringLiteral("kof98"), 500, users);

    QVariantList lobby{room};
    for (const char *name : {"garou", "vsav", "mvsc", "kof2002", "sf2ce", "dino", "sfa3", "xmen"}) {
        lobby.append(channel(QString::fromLatin1(name), 120));
    }

    window->setProperty("user", QVariantMap{{QStringLiteral("name"), QStringLiteral("someone")}});
    window->setProperty("joinedChannels", QVariantList{room});
    window->setProperty("allChannels", lobby);
    window->setProperty("currentChannel", room);

    auto setRoster = [&](const QVariantList &roster) {
        QVariantMap next = room;
        next.insert(QStringLiteral("users"), roster);
        window->setProperty("currentChannel", next);
        spin();
    };
    auto openScreen = [&](const QString &name) {
        QMetaObject::invokeMethod(window, "switchScreen", Q_ARG(QVariant, QVariant(name)));
        spin();
    };

    // The featured banner: sideways moves stay in its row on a wide screen.
    window->resize(1920, 1080);
    spin();
    check(window->width() == 1920, "the window really is wide for the banner checks");
    const qreal bannerBottom = 340;
    openScreen(QStringLiteral("home"));
    focus.move(1, 0);
    check(sceneY(window->activeFocusItem()) < bannerBottom, "Right from the banner stays beside it");
    openScreen(QStringLiteral("home"));
    focus.move(-1, 0);
    check(sceneY(window->activeFocusItem()) < bannerBottom, "Left from the banner stays beside it");

    window->resize(1280, 800);
    spin();
    openScreen(QStringLiteral("room"));
    auto *list = window->findChild<QObject *>(QStringLiteral("playerList"));
    auto *chat = window->findChild<QObject *>(QStringLiteral("chatList"));

    for (int i = 0; i < 12; ++i) {
        focus.move(0, 1);
        spin(120); // A real press is never faster than the repeat rate.
    }
    QPointer<QQuickItem> row = window->activeFocusItem();
    check(playerName(row) == QStringLiteral("player12"), "D-pad walks down the roster");

    // Live roster traffic must not move the list or the selection.
    qreal lowest = contentY(list);
    const qreal before = contentY(list);
    const qreal rowY = sceneY(row);
    QVariantMap update{{QStringLiteral("username"), QStringLiteral("player20")}, {QStringLiteral("rank"), 4}};
    QMetaObject::invokeMethod(window, "updateUserInChannels", Q_ARG(QVariant, QVariant(update)));
    lowest = qMin(lowest, contentY(list));
    check(lowest == before, "a player update does not reset the list, even briefly");
    spin();
    check(window->activeFocusItem() == row && contentY(list) == before, "a player update keeps row and scroll");

    QVariantList joined = users;
    joined.prepend(player(QStringLiteral("aaa_new")));
    setRoster(joined);
    check(window->activeFocusItem() == row, "a join above keeps the selected row");
    check(qAbs(sceneY(row) - rowY) < 1.0, "a join above does not shift what is on screen");

    QVariantList left = joined;
    left.removeAt(20);
    setRoster(left);
    check(window->activeFocusItem() == row, "a leave elsewhere keeps the selected row");

    QVariantList withoutSelected;
    for (const QVariant &entry : left) {
        if (entry.toMap().value(QStringLiteral("name")) != QStringLiteral("player12")) withoutSelected.append(entry);
    }
    setRoster(withoutSelected);
    const QString neighbour = playerName(window->activeFocusItem());
    check(neighbour == QStringLiteral("player13"), "when the selected player leaves, the next row is selected");

    // A row scrolled out of view: the next press stays where the player is looking.
    list->setProperty("contentY", 0);
    spin();
    focus.move(0, 1);
    spin();
    check(contentY(list) < 5.0, "Down after scrolling away does not drag the list back");

    // Chat: reading history holds position, and the 400-message cap can't reset it.
    const QString roomName = room.value(QStringLiteral("name")).toString();
    for (int i = 0; i < 405; ++i) systemMessage(window, roomName, i);
    spin(400);
    check(chat->property("count").toInt() == 400, "chat is capped at 400 rows");
    check(chat->property("atYEnd").toBool(), "chat follows new messages at the bottom");

    systemMessage(window, roomName, 1000);
    spin();
    check(chat->property("atYEnd").toBool(), "chat at the cap stays at the bottom");

    const qreal reading = contentY(chat) - 900;
    chat->setProperty("contentY", reading);
    spin();
    systemMessage(window, roomName, 1001);
    spin();
    check(qAbs(contentY(chat) - reading) < 80.0, "reading chat history holds position at the cap");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
