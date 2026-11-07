#include "controllerprofile.h"

#include "fightcadelauncher.h"
#include "../core/settingsstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>


namespace {
QString hexString(const QByteArray &text)
{
    return QString::fromLatin1(text.toHex());
}

QString directionName(int bit)
{
    // These are literal SDL_HAT_* bits: UP=1, RIGHT=2, DOWN=4, LEFT=8.
    if (bit == SDL_HAT_UP) return QStringLiteral("D-pad Up");
    if (bit == SDL_HAT_RIGHT) return QStringLiteral("D-pad Right");
    if (bit == SDL_HAT_DOWN) return QStringLiteral("D-pad Down");
    if (bit == SDL_HAT_LEFT) return QStringLiteral("D-pad Left");
    return QStringLiteral("Hat");
}
} // namespace

ControllerProfile::ControllerProfile(FightcadeLauncher *launcher, SettingsStore *settings,
                                     QObject *parent)
    : QObject(parent)
    , m_launcher(launcher)
    , m_settings(settings)
{
}

ControllerProfile::~ControllerProfile()
{
    stopCapture();
    closeController();
}

QString ControllerProfile::currentIdentity() const
{
    return m_identity;
}

QString ControllerProfile::bindingText(const Action &action) const
{
    if (action.kind == QStringLiteral("b")) return QStringLiteral("Button %1").arg(action.controlIndex);
    if (action.kind == QStringLiteral("h")) return directionName(action.direction);
    if (action.kind == QStringLiteral("x")) return QStringLiteral("Axis %1").arg(action.controlIndex);
    if (action.kind == QStringLiteral("a")) {
        return QStringLiteral("Axis %1 %2")
            .arg(action.controlIndex)
            .arg(action.direction < 0 ? QStringLiteral("−") : QStringLiteral("+"));
    }
    return QStringLiteral("Unbound");
}

QVariantList ControllerProfile::actions() const
{
    QVariantList result;
    for (const Action &action : m_actionList) {
        result.append(QVariantMap{
            {"index", action.index},
            {"title", action.title},
            {"info", action.info},
            {"analog", action.analog},
            {"kind", action.kind},
            {"controlIndex", action.controlIndex},
            {"direction", action.direction},
            {"binding", bindingText(action)}
        });
    }
    return result;
}

void ControllerProfile::setStatus(const QString &status)
{
    if (m_status == status) return;
    m_status = status;
    emit stateChanged();
}

void ControllerProfile::loadForGame(const QString &gameId)
{
    if (m_gameId == gameId && !m_actionList.isEmpty()) {
        return;
    }
    stopCapture();
    m_gameId = gameId;
    m_status.clear();
    m_actionList.clear();
    emit stateChanged();
    emit actionsChanged();
    runHelper();
    parseProfile();
    setStatus(QStringLiteral("%1 P1 actions").arg(m_actionList.size()));
    emit actionsChanged();
}

void ControllerProfile::runHelper()
{
    if (!m_launcher) return;
    const QString binary = m_launcher->runtimeBinaryFor(QStringLiteral("fbneo"));
    if (binary.isEmpty() || !QFileInfo::exists(binary)) {
        setStatus(QStringLiteral("FBNeo runtime not found."));
        return;
    }

    QProcess process;
    process.setProgram(binary);
    process.setArguments({QStringLiteral("--macade-gamepad-inputs"), m_gameId});
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForFinished(8000)) {
        process.kill();
        setStatus(QStringLiteral("Could not read the FBNeo input list."));
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput());
    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("player")).toInt() != 1) continue;
        Action action;
        action.index = object.value(QStringLiteral("index")).toInt();
        action.title = object.value(QStringLiteral("title")).toString();
        action.info = object.value(QStringLiteral("info")).toString();
        action.analog = object.value(QStringLiteral("analog")).toBool();
        m_actionList.append(action);
    }

    std::sort(m_actionList.begin(), m_actionList.end(), [](const Action &a, const Action &b) {
        return a.index < b.index;
    });
}

void ControllerProfile::parseProfile()
{
    QFile file(controllerProfilePath(m_gameId));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    bool header = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (!header) {
            header = line == QStringLiteral("MACADE_GAMEPADS 1");
            continue;
        }
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        const QStringList fields = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (fields.size() < 8 || fields.at(1) != m_gameId) continue;
        const int inputIndex = fields.at(2).toInt();
        const QString kind = fields.at(5);
        const int controlIndex = fields.at(6).toInt();
        const int direction = fields.at(7).toInt();
        if (Action *action = actionByIndex(inputIndex)) {
            action->kind = kind;
            action->controlIndex = controlIndex;
            action->direction = direction;
            if (kind == QStringLiteral("h")) {
                const QString title = action->title.toLower();
                if (title.contains(QStringLiteral("up"))) action->direction = SDL_HAT_UP;
                else if (title.contains(QStringLiteral("down"))) action->direction = SDL_HAT_DOWN;
                else if (title.contains(QStringLiteral("left"))) action->direction = SDL_HAT_LEFT;
                else if (title.contains(QStringLiteral("right"))) action->direction = SDL_HAT_RIGHT;
            }
        }
    }
}

ControllerProfile::Action *ControllerProfile::actionByIndex(int inputIndex)
{
    for (Action &action : m_actionList) {
        if (action.index == inputIndex) return &action;
    }
    return nullptr;
}

const ControllerProfile::Action *ControllerProfile::actionByIndex(int inputIndex) const
{
    for (const Action &action : m_actionList) {
        if (action.index == inputIndex) return &action;
    }
    return nullptr;
}

bool ControllerProfile::ensureController()
{
    if (m_joystick) return true;
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) != 0) {
        setStatus(QStringLiteral("SDL controller initialization failed."));
        return false;
    }

    int index = m_settings ? m_settings->controllerIndex() : 0;
    if (index < 0 || index >= SDL_NumJoysticks()) index = 0;
    SDL_Joystick *joystick = SDL_JoystickOpen(index);
    if (!joystick) {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            joystick = SDL_JoystickOpen(i);
            if (joystick) {
                index = i;
                break;
            }
        }
    }
    if (!joystick) {
        setStatus(QStringLiteral("No controller detected."));
        return false;
    }

    m_joystick = joystick;
    m_controllerIndex = index;
    const char *name = SDL_JoystickName(joystick);
    m_controllerName = name ? QString::fromUtf8(name) : QStringLiteral("Controller");

    char guid[33] = {};
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick), guid, sizeof(guid));
    const char *serial = SDL_JoystickGetSerial(joystick);
    m_identity = QString::fromUtf8(guid) + QLatin1Char(':');
    if (serial && *serial) {
        m_identity += QLatin1Char('s') + hexString(QByteArray(serial));
    } else {
        m_identity += QStringLiteral("n0");
    }
    emit stateChanged();
    return true;
}

void ControllerProfile::closeController()
{
    if (m_joystick) {
        SDL_JoystickClose(m_joystick);
        m_joystick = nullptr;
    }
    m_controllerIndex = -1;
    m_controllerName.clear();
    m_identity.clear();
    emit stateChanged();
}

void ControllerProfile::prepareCapture(int inputIndex)
{
    const Action *action = actionByIndex(inputIndex);
    if (!action) {
        setStatus(QStringLiteral("Unknown FBNeo action."));
        return;
    }
    if (!ensureController()) return;

    stopCapture();
    m_captureInputIndex = inputIndex;
    m_initialButtons.resize(SDL_JoystickNumButtons(m_joystick));
    for (int i = 0; i < m_initialButtons.size(); ++i) {
        m_initialButtons[i] = SDL_JoystickGetButton(m_joystick, i) != 0;
    }
    m_initialHat = SDL_JoystickNumHats(m_joystick) > 0 ? SDL_JoystickGetHat(m_joystick, 0) : 0;
    m_initialAxes.resize(SDL_JoystickNumAxes(m_joystick));
    for (int i = 0; i < m_initialAxes.size(); ++i) {
        m_initialAxes[i] = SDL_JoystickGetAxis(m_joystick, i);
    }

    m_capturing = true;
    m_captureArmedAt = QDateTime::currentMSecsSinceEpoch() + 350;
    setStatus(QStringLiteral("Press a button, D-pad direction, or move a stick..."));
    emit stateChanged();

    QTimer *timer = new QTimer(this);
    timer->setInterval(16);
    connect(timer, &QTimer::timeout, this, [this, timer, inputIndex] {
        if (!m_capturing || !m_joystick || m_captureInputIndex != inputIndex) {
            timer->stop();
            timer->deleteLater();
            return;
        }
        if (QDateTime::currentMSecsSinceEpoch() < m_captureArmedAt) {
            return;
        }

        SDL_JoystickUpdate();
        Action *action = actionByIndex(inputIndex);
        if (!action) {
            timer->stop();
            timer->deleteLater();
            return;
        }

        QString kind;
        int controlIndex = 0;
        int direction = 0;

        for (int i = 0; i < m_initialButtons.size(); ++i) {
            const bool current = SDL_JoystickGetButton(m_joystick, i) != 0;
            if (m_initialButtons[i] && !current) {
                m_initialButtons[i] = false;
            } else if (!m_initialButtons[i] && current) {
                kind = QStringLiteral("b");
                controlIndex = i;
                break;
            }
        }

        if (kind.isEmpty() && SDL_JoystickNumHats(m_joystick) > 0) {
            const int hat = SDL_JoystickGetHat(m_joystick, 0);
            if (hat != m_initialHat) {
                if (hat != 0) {
                    kind = QStringLiteral("h");
                    controlIndex = 0;
                    if (hat & SDL_HAT_UP) direction = SDL_HAT_UP;
                    else if (hat & SDL_HAT_RIGHT) direction = SDL_HAT_RIGHT;
                    else if (hat & SDL_HAT_DOWN) direction = SDL_HAT_DOWN;
                    else if (hat & SDL_HAT_LEFT) direction = SDL_HAT_LEFT;
                }
                m_initialHat = hat;
            }
        }

        if (kind.isEmpty()) {
            for (int i = 0; i < m_initialAxes.size(); ++i) {
                const int current = SDL_JoystickGetAxis(m_joystick, i);
                const int delta = current - m_initialAxes[i];
                if (action->analog) {
                    if (std::abs(delta) > 16000) {
                        kind = QStringLiteral("x");
                        controlIndex = i;
                        direction = 0;
                        break;
                    }
                } else if (std::abs(delta) > 16000) {
                    kind = QStringLiteral("a");
                    controlIndex = i;
                    direction = delta > 0 ? 1 : -1;
                    break;
                }
            }
        }

        if (kind.isEmpty()) return;

        action->kind = kind;
        action->controlIndex = controlIndex;
        action->direction = direction;
        timer->stop();
        timer->deleteLater();
        stopCapture();
        saveProfile();
        setStatus(QStringLiteral("%1 → %2").arg(action->title, bindingText(*action)));
        emit actionsChanged();
        emit bindingCaptured(inputIndex, kind, controlIndex, direction);
    });
    timer->start();
}

void ControllerProfile::stopCapture()
{
    m_capturing = false;
    m_captureInputIndex = -1;
    m_initialButtons.clear();
    m_initialAxes.clear();
    emit stateChanged();
}

void ControllerProfile::cancelCapture()
{
    stopCapture();
    setStatus(QStringLiteral("Binding cancelled."));
}

void ControllerProfile::saveProfile()
{
    if (!ensureController()) return;
    QList<Action> bound;
    for (const Action &action : m_actionList) {
        if (!action.kind.isEmpty()) bound.append(action);
    }

    const QString path = controllerProfilePath(m_gameId);
    if (bound.isEmpty()) {
        QFile::remove(path);
        return;
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatus(QStringLiteral("Could not write controller profile."));
        return;
    }

    QTextStream stream(&file);
    stream << "MACADE_GAMEPADS 1\n";
    for (const Action &action : bound) {
        stream << m_identity << ' ' << m_gameId << ' ' << action.index << " 1 "
               << action.info << ' ' << action.kind << ' ' << action.controlIndex << ' '
               << action.direction << '\n';
    }
    file.commit();
}

void ControllerProfile::clearBinding(int inputIndex)
{
    Action *action = actionByIndex(inputIndex);
    if (!action) return;
    action->kind.clear();
    action->controlIndex = 0;
    action->direction = 0;
    saveProfile();
    emit actionsChanged();
    setStatus(QStringLiteral("%1 unbound").arg(action->title));
}

void ControllerProfile::resetDefaults()
{
    QFile::remove(controllerProfilePath(m_gameId));
    for (Action &action : m_actionList) {
        action.kind.clear();
        action.controlIndex = 0;
        action.direction = 0;
    }
    setStatus(QStringLiteral("Controller profile cleared; FBNeo defaults apply."));
    emit actionsChanged();
}
