#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>

class SettingsStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString romPath READ romPath WRITE setRomPath NOTIFY romPathChanged)
    Q_PROPERTY(QString gameInputDevice READ gameInputDevice WRITE setGameInputDevice NOTIFY gameInputDeviceChanged)
    Q_PROPERTY(bool showControllerHints READ showControllerHints WRITE setShowControllerHints NOTIFY showControllerHintsChanged)
    Q_PROPERTY(bool autoJoinLastChannels READ autoJoinLastChannels WRITE setAutoJoinLastChannels NOTIFY autoJoinLastChannelsChanged)
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY reduceMotionChanged)
    Q_PROPERTY(bool fullscreen READ fullscreen WRITE setFullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(bool autoDownloadRoms READ autoDownloadRoms WRITE setAutoDownloadRoms NOTIFY autoDownloadRomsChanged)
    Q_PROPERTY(bool rememberLogin READ rememberLogin WRITE setRememberLogin NOTIFY rememberLoginChanged)
    Q_PROPERTY(qreal uiScale READ uiScale WRITE setUiScale NOTIFY uiScaleChanged)
    Q_PROPERTY(int controllerIndex READ controllerIndex WRITE setControllerIndex NOTIFY controllerIndexChanged)
    Q_PROPERTY(QStringList lastChannels READ lastChannels WRITE setLastChannels NOTIFY lastChannelsChanged)
    Q_PROPERTY(bool hasSession READ hasSession NOTIFY sessionChanged)
    Q_PROPERTY(QString sessionName READ sessionName NOTIFY sessionChanged)
    Q_PROPERTY(QString sessionDisplayName READ sessionDisplayName NOTIFY sessionChanged)
    Q_PROPERTY(QString sessionCookie READ sessionCookie NOTIFY sessionChanged)

public:
    explicit SettingsStore(QObject *parent = nullptr);

    QString romPath() const;
    bool showControllerHints() const;
    bool autoJoinLastChannels() const;
    bool reduceMotion() const;
    bool fullscreen() const;
    bool autoDownloadRoms() const;
    bool rememberLogin() const;
    qreal uiScale() const;
    int controllerIndex() const;
    QStringList lastChannels() const;
    bool hasSession() const;
    QString sessionName() const;
    QString sessionDisplayName() const;
    QString sessionCookie() const;

    void setRomPath(const QString &value);
    QString gameInputDevice() const { return m_gameInputDevice; }
    void setGameInputDevice(const QString &value);
    void setShowControllerHints(bool value);
    void setAutoJoinLastChannels(bool value);
    void setReduceMotion(bool value);
    void setFullscreen(bool value);
    void setAutoDownloadRoms(bool value);
    void setRememberLogin(bool value);
    void setUiScale(qreal value);
    void setControllerIndex(int value);
    void setLastChannels(const QStringList &value);

    Q_INVOKABLE void saveSession(const QString &cookie, const QString &name, const QString &displayName);
    Q_INVOKABLE void clearSession();
    Q_INVOKABLE void rememberChannel(const QString &channelName);
    Q_INVOKABLE void protectConfigFile();

signals:
    void romPathChanged();
    void gameInputDeviceChanged();
    void showControllerHintsChanged();
    void autoJoinLastChannelsChanged();
    void reduceMotionChanged();
    void fullscreenChanged();
    void autoDownloadRomsChanged();
    void rememberLoginChanged();
    void uiScaleChanged();
    void controllerIndexChanged();
    void lastChannelsChanged();
    void sessionChanged();

private:
    QSettings m_settings;
    QString m_romPath;
    QString m_gameInputDevice = QStringLiteral("controller");
    bool m_showControllerHints = true;
    bool m_autoJoinLastChannels = true;
    bool m_reduceMotion = false;
    bool m_fullscreen = true;
    bool m_autoDownloadRoms = true;
    bool m_rememberLogin = true;
    qreal m_uiScale = 1.0;
    int m_controllerIndex = 0;
    QStringList m_lastChannels;
    QString m_sessionCookie;
    QString m_sessionName;
    QString m_sessionDisplayName;

    void persist(const QString &key, const QVariant &value);
};
