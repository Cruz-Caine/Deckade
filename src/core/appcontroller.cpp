#include "appcontroller.h"

#include "settingsstore.h"
#include "../fightcade/fightcadelauncher.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QUrl>
#include <QWindow>

AppController::AppController(SettingsStore *settings, FightcadeLauncher *launcher,
                             QGuiApplication *application, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_launcher(launcher)
    , m_application(application)
{
    connect(m_settings, &SettingsStore::fullscreenChanged, this, &AppController::fullscreenChanged);
}

QString AppController::version() const
{
#ifdef DECKADE_VERSION
    return QStringLiteral(DECKADE_VERSION);
#else
    return QStringLiteral("0.1.0");
#endif
}

bool AppController::isSteamDeck() const
{
    return m_launcher && m_launcher->isSteamDeck();
}

bool AppController::fullscreen() const
{
    return m_settings->fullscreen();
}

void AppController::setFullscreen(bool value)
{
    m_settings->setFullscreen(value);
}

void AppController::openExternal(const QString &url)
{
    const QUrl parsed(url);
    if (!parsed.isValid()) {
        return;
    }
    const QString scheme = parsed.scheme().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
        return;
    }
    QDesktopServices::openUrl(parsed);
}

void AppController::copyToClipboard(const QString &text)
{
    if (auto *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}

void AppController::toggleFullscreen()
{
    setFullscreen(!fullscreen());
}

void AppController::quit()
{
    if (m_launcher) {
        m_launcher->killEmulator();
    }
    if (m_application) {
        m_application->quit();
    }
}

void AppController::setWindowVisible(bool visible)
{
    for (QWindow *window : QGuiApplication::topLevelWindows()) {
        if (!window) continue;
        if (visible) {
            window->show();
            window->raise();
            window->requestActivate();
        } else {
            window->hide();
        }
    }
}

void AppController::setWindowMinimized(bool minimized)
{
    for (QWindow *window : QGuiApplication::topLevelWindows()) {
        if (!window) continue;
        window->setVisibility(minimized ? QWindow::Minimized : QWindow::Windowed);
    }
}
