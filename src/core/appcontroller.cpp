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
#include <QtGlobal>

#include <initializer_list>

AppController::AppController(SettingsStore *settings, FightcadeLauncher *launcher,
                             QGuiApplication *application, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_launcher(launcher)
    , m_application(application)
{
    // Optional unattended sign-in. Environment credentials are read once at
    // startup, are never written to settings or logs, and are cleared as soon
    // as QML takes them for the login request.
    const auto credential = [](std::initializer_list<const char *> names) {
        for (const char *name : names) {
            const QByteArray value = qgetenv(name).trimmed();
            if (!value.isEmpty()) return QString::fromUtf8(value);
        }
        return QString();
    };
    m_envUsername = credential({"DECKADE_USERNAME", "FIGHTCADE_USERNAME", "FIGHTCADE_USER"});
    m_envPassword = credential({"DECKADE_PASSWORD", "FIGHTCADE_PASSWORD", "FIGHTCADE_PASS"});
    if (m_envUsername.isEmpty() || m_envPassword.isEmpty()) {
        m_envUsername.clear();
        m_envPassword.clear();
    }

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

bool AppController::hasEnvCredentials() const
{
    return !m_envUsername.isEmpty() && !m_envPassword.isEmpty();
}

QString AppController::envUsername() const
{
    return m_envUsername;
}

QString AppController::takeEnvPassword()
{
    const QString password = m_envPassword;
    m_envPassword.clear();
    return password;
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
