#pragma once

#include <QObject>
#include <QString>

class SettingsStore;
class FightcadeLauncher;
class QGuiApplication;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool isSteamDeck READ isSteamDeck CONSTANT)
    Q_PROPERTY(bool fullscreen READ fullscreen WRITE setFullscreen NOTIFY fullscreenChanged)

public:
    AppController(SettingsStore *settings, FightcadeLauncher *launcher,
                  QGuiApplication *application, QObject *parent = nullptr);

    QString version() const;
    bool isSteamDeck() const;
    bool fullscreen() const;
    void setFullscreen(bool value);

    Q_INVOKABLE void openExternal(const QString &url);
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void toggleFullscreen();
    Q_INVOKABLE void quit();
    Q_INVOKABLE void setWindowVisible(bool visible);
    Q_INVOKABLE void setWindowMinimized(bool minimized);

signals:
    void fullscreenChanged();

private:
    SettingsStore *m_settings = nullptr;
    FightcadeLauncher *m_launcher = nullptr;
    QGuiApplication *m_application = nullptr;
};
