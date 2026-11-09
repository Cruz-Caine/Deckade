#pragma once

#include <QDir>
#include <QList>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class SettingsStore;
class FightcadeLauncher;
typedef struct _SDL_Joystick SDL_Joystick;

inline QString controllerProfilePath(const QString &gameId)
{
    QString safe = gameId;
    safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));
    if (safe.isEmpty()) safe = QStringLiteral("unknown");
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("controller-profiles/%1.txt").arg(safe));
}

class ControllerProfile : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString gameId READ gameId NOTIFY stateChanged)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY actionsChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(bool capturing READ capturing NOTIFY stateChanged)
    Q_PROPERTY(int captureInputIndex READ captureInputIndex NOTIFY stateChanged)
    Q_PROPERTY(QString controllerName READ controllerName NOTIFY stateChanged)
    Q_PROPERTY(int controllerIndex READ controllerIndex NOTIFY stateChanged)

public:
    ControllerProfile(FightcadeLauncher *launcher, SettingsStore *settings,
                      QObject *parent = nullptr);
    ~ControllerProfile() override;

    QString gameId() const { return m_gameId; }
    QVariantList actions() const;
    QString status() const { return m_status; }
    bool capturing() const { return m_capturing; }
    int captureInputIndex() const { return m_captureInputIndex; }
    QString controllerName() const { return m_controllerName; }
    int controllerIndex() const { return m_controllerIndex; }

    Q_INVOKABLE void loadForGame(const QString &gameId);
    // Snes9x and Flycast share one profile each; only FBNeo varies by game.
    Q_INVOKABLE void loadFor(const QString &emulator, const QString &gameId);
    Q_INVOKABLE void prepareCapture(int inputIndex);
    Q_INVOKABLE void cancelCapture();
    Q_INVOKABLE void clearBinding(int inputIndex);
    Q_INVOKABLE void resetDefaults();

signals:
    void stateChanged();
    void actionsChanged();
    void bindingCaptured(int inputIndex, const QString &kind, int controlIndex, int direction);

private:
    struct Action {
        int index = 0;
        QString title;
        QString info;
        bool analog = false;
        QString kind;
        int controlIndex = 0;
        int direction = 0;
        QString flycastName;
    };

    void runHelper();
    void buildStaticActions();
    void seedFlycastDefaults();
    void writeFlycastMappings() const;
    QString flycastMappingBase() const;
    void parseProfile();
    bool ensureController();
    void closeController();
    void stopCapture();
    void saveProfile();
    void setStatus(const QString &status);
    Action *actionByIndex(int inputIndex);
    const Action *actionByIndex(int inputIndex) const;
    QString bindingText(const Action &action) const;
    QString currentIdentity() const;

    FightcadeLauncher *m_launcher = nullptr;
    SettingsStore *m_settings = nullptr;
    QString m_gameId;
    QString m_emulator = QStringLiteral("fbneo");
    QString m_status;
    QList<Action> m_actionList;
    bool m_capturing = false;
    int m_captureInputIndex = -1;
    SDL_Joystick *m_joystick = nullptr;
    int m_controllerIndex = -1;
    QString m_controllerName;
    QString m_identity;
    QVector<bool> m_initialButtons;
    int m_initialHat = 0;
    QVector<int> m_initialAxes;
    qint64 m_captureArmedAt = 0;
};
