#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <SDL_gamecontroller.h>

class GamepadManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString controllerName READ controllerName NOTIFY connectedChanged)
    Q_PROPERTY(int controllerIndex READ controllerIndex NOTIFY connectedChanged)

public:
    explicit GamepadManager(QObject *parent = nullptr);
    ~GamepadManager() override;

    bool connected() const { return m_controller != nullptr; }
    QString controllerName() const { return m_controllerName; }
    int controllerIndex() const { return m_controllerIndex; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectController(int index);
    Q_INVOKABLE QStringList controllerNames() const;

signals:
    void connectedChanged();
    void navigate(int dx, int dy);
    void acceptPressed();
    void backPressed();
    void secondaryPressed();
    void keyboardPressed();
    void menuPressed();
    void tabLeftPressed();
    void tabRightPressed();
    void triggerLeftPressed();
    void triggerRightPressed();

private slots:
    void poll();

private:
    void closeController();
    bool openController();
    bool buttonPressed(int button) const;
    bool edgePressed(int button, bool &previous);
    void emitTriggers();

    SDL_GameController *m_controller = nullptr;
    QString m_controllerName;
    int m_selectedIndex = -1;
    int m_controllerIndex = -1;
    QTimer m_timer;
    QElapsedTimer m_navClock;
    QPoint m_lastNav;
    bool m_navRepeating = false;
    bool m_prevA = false;
    bool m_prevB = false;
    bool m_prevX = false;
    bool m_prevY = false;
    bool m_prevBack = false;
    bool m_prevStart = false;
    bool m_prevLeftShoulder = false;
    bool m_prevRightShoulder = false;
    bool m_prevLeftTrigger = false;
    bool m_prevRightTrigger = false;
};
