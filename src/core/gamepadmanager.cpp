#include "gamepadmanager.h"

#include <SDL.h>

#include <algorithm>

namespace {
constexpr int kStickDeadzone = 16000;
constexpr int kInitialRepeatMs = 340;
constexpr int kRepeatMs = 105;
}

GamepadManager::GamepadManager(QObject *parent)
    : QObject(parent)
{
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "1");
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
    SDL_GameControllerEventState(SDL_ENABLE);

    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &GamepadManager::poll);
    m_timer.start();

    refresh();
    m_navClock.start();
}

GamepadManager::~GamepadManager()
{
    closeController();
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
}

void GamepadManager::refresh()
{
    if (m_controller && !SDL_GameControllerGetAttached(m_controller)) {
        closeController();
    }
    if (!m_controller) {
        openController();
    }
}

bool GamepadManager::openController()
{
    const int joysticks = SDL_NumJoysticks();
    auto tryOpen = [this](int index) -> bool {
        if (index < 0 || index >= SDL_NumJoysticks() || !SDL_IsGameController(index)) return false;
        SDL_GameController *controller = SDL_GameControllerOpen(index);
        if (!controller) return false;
        m_controller = controller;
        m_controllerIndex = index;
        const char *name = SDL_GameControllerName(controller);
        m_controllerName = name ? QString::fromUtf8(name) : QStringLiteral("Game controller");
        emit connectedChanged();
        return true;
    };

    if (m_selectedIndex >= 0 && tryOpen(m_selectedIndex)) {
        return true;
    }
    for (int index = 0; index < joysticks; ++index) {
        if (tryOpen(index)) {
            return true;
        }
    }
    return false;
}

void GamepadManager::selectController(int index)
{
    closeController();
    m_selectedIndex = index;
    refresh();
}

QStringList GamepadManager::controllerNames() const
{
    QStringList names;
    const int joysticks = SDL_NumJoysticks();
    for (int index = 0; index < joysticks; ++index) {
        if (!SDL_IsGameController(index)) continue;
        const char *name = SDL_GameControllerNameForIndex(index);
        names << QStringLiteral("%1: %2").arg(index).arg(name ? QString::fromUtf8(name) : QStringLiteral("Controller"));
    }
    return names;
}

void GamepadManager::closeController()
{
    if (!m_controller) {
        return;
    }
    SDL_GameControllerClose(m_controller);
    m_controller = nullptr;
    m_controllerName.clear();
    m_controllerIndex = -1;
    emit connectedChanged();
}

bool GamepadManager::buttonPressed(int button) const
{
    return m_controller && SDL_GameControllerGetButton(m_controller, static_cast<SDL_GameControllerButton>(button)) != 0;
}

bool GamepadManager::edgePressed(int button, bool &previous)
{
    const bool current = buttonPressed(button);
    const bool pressed = current && !previous;
    previous = current;
    return pressed;
}

void GamepadManager::emitTriggers()
{
    if (!m_controller) {
        m_prevLeftTrigger = false;
        m_prevRightTrigger = false;
        return;
    }

    const int left = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    const int right = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    const bool leftDown = left > 16000;
    const bool rightDown = right > 16000;

    if (leftDown && !m_prevLeftTrigger) emit triggerLeftPressed();
    if (rightDown && !m_prevRightTrigger) emit triggerRightPressed();
    m_prevLeftTrigger = leftDown;
    m_prevRightTrigger = rightDown;
}

void GamepadManager::poll()
{
    SDL_PumpEvents();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_CONTROLLERDEVICEADDED) {
            refresh();
        } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            refresh();
        }
    }

    if (!m_controller || !SDL_GameControllerGetAttached(m_controller)) {
        closeController();
        refresh();
        return;
    }

    if (edgePressed(SDL_CONTROLLER_BUTTON_A, m_prevA)) emit acceptPressed();
    if (edgePressed(SDL_CONTROLLER_BUTTON_B, m_prevB)) emit backPressed();
    if (edgePressed(SDL_CONTROLLER_BUTTON_X, m_prevX)) emit secondaryPressed();
    if (edgePressed(SDL_CONTROLLER_BUTTON_Y, m_prevY)) emit keyboardPressed();
    if (edgePressed(SDL_CONTROLLER_BUTTON_BACK, m_prevBack)) emit keyboardPressed();
    if (edgePressed(SDL_CONTROLLER_BUTTON_START, m_prevStart)) emit menuPressed();
    if (edgePressed(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, m_prevLeftShoulder)) emit tabLeftPressed();
    if (edgePressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, m_prevRightShoulder)) emit tabRightPressed();
    emitTriggers();

    int dx = 0;
    int dy = 0;
    if (buttonPressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) dx -= 1;
    if (buttonPressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) dx += 1;
    if (buttonPressed(SDL_CONTROLLER_BUTTON_DPAD_UP)) dy -= 1;
    if (buttonPressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) dy += 1;

    if (dx == 0 && dy == 0) {
        const int axisX = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTX);
        const int axisY = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTY);
        if (qAbs(axisX) > kStickDeadzone) dx = axisX > 0 ? 1 : -1;
        if (qAbs(axisY) > kStickDeadzone) dy = axisY > 0 ? 1 : -1;
    }

    if (dx == 0 && dy == 0) {
        m_lastNav = {};
        m_navRepeating = false;
        return;
    }

    if (dx != 0 && dy != 0) {
        const int axisX = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTX);
        const int axisY = SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTY);
        if (qAbs(axisX) >= qAbs(axisY)) {
            dy = 0;
        } else {
            dx = 0;
        }
    }

    const QPoint direction(dx, dy);
    if (direction != m_lastNav) {
        m_lastNav = direction;
        m_navRepeating = false;
        m_navClock.restart();
        emit navigate(dx, dy);
        return;
    }

    const int threshold = m_navRepeating ? kRepeatMs : kInitialRepeatMs;
    if (m_navClock.elapsed() >= threshold) {
        m_navRepeating = true;
        m_navClock.restart();
        emit navigate(dx, dy);
    }
}
