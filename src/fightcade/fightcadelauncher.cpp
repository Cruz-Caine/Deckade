#include "fightcadelauncher.h"

#include "../core/settingsstore.h"
#include "protocol.h"
#include "netplayproxy.h"
#include "flycastlaunch.h"
#include "controllerprofile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QTextStream>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>
#include <QElapsedTimer>
#include <QDateTime>
#include <QUrl>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <sys/stat.h>

namespace {

QString cleanPath(const QString &path)
{
    return QDir::cleanPath(QDir(path).absolutePath());
}

bool directoryExists(const QString &path)
{
    return !path.isEmpty() && QFileInfo(path).isDir();
}

bool executableExists(const QString &path)
{
    const QFileInfo info(path);
    return info.isFile() && info.isExecutable();
}

QString romRootForEmulator(const QString &configured, const QString &emulator)
{
    QStringList candidates;
    if (!configured.isEmpty()) {
        candidates << configured
                   << QDir(configured).filePath(emulator);
    }
    candidates << QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                      .filePath(QStringLiteral("deckade/roms/%1").arg(emulator));

    for (const QString &candidate : std::as_const(candidates)) {
        if (candidate.isEmpty()) continue;
        const QString path = cleanPath(candidate);
        if (directoryExists(path)) return path;
    }
    return {};
}

QString discoverRuntimeRoot()
{
    if (qEnvironmentVariableIsSet("DECKADE_RUNTIME_DIR")) {
        const QString configured = qEnvironmentVariable("DECKADE_RUNTIME_DIR");
        if (directoryExists(configured)) {
            return cleanPath(configured);
        }
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("runtime")),
        QDir(appDir).filePath(QStringLiteral("../runtime")),
        QDir(appDir).filePath(QStringLiteral("../share/deckade/runtime")),
        QDir(home).filePath(QStringLiteral(".local/share/deckade/runtime")),
        QStringLiteral("/usr/lib/deckade/runtime"),
        QStringLiteral("/usr/share/deckade/runtime")
    };
    for (const QString &candidate : candidates) {
        if (directoryExists(candidate)) {
            return cleanPath(candidate);
        }
    }
    return {};
}

QString uriForMatch(const QVariantMap &match)
{
    return FightcadeProtocol::quarkServed(
        match.value(QStringLiteral("gameId")).toString(),
        match.value(QStringLiteral("quarkId")).toString(),
        match.value(QStringLiteral("playerId")).toInt(),
        match.value(QStringLiteral("port")).toInt(),
        match.value(QStringLiteral("delay")).toInt(),
        match.value(QStringLiteral("ranked")).toInt());
}

} // namespace

FightcadeLauncher::FightcadeLauncher(SettingsStore *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    connect(m_settings, &SettingsStore::romPathChanged, this, &FightcadeLauncher::refreshInstall);
    refreshInstall();
}

bool FightcadeLauncher::isSteamDeck() const
{
    if (qEnvironmentVariableIsSet("SteamDeck") || qEnvironmentVariableIsSet("STEAMOS")) {
        return true;
    }
    if (QFile::exists(QStringLiteral("/etc/steam-release"))) {
        return true;
    }
    QFile file(QStringLiteral("/etc/os-release"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QByteArray contents = file.readAll().toLower();
        return contents.contains("steamdeck") || contents.contains("steamos");
    }
    return false;
}

void FightcadeLauncher::refreshInstall()
{
    inspectInstall();
    inspectRuntimes();
    emit installChanged();
}

void FightcadeLauncher::inspectInstall()
{
    m_romPath = romPathFor(QStringLiteral("fbneo"));
}

void FightcadeLauncher::inspectRuntimes()
{
    m_runtimeRoot = discoverRuntimeRoot();

    struct RuntimeDefinition {
        QString id;
        QString name;
        QString binary;
        QStringList systems;
        QString note;
    };

    const QList<RuntimeDefinition> definitions = {
        {QStringLiteral("fbneo"), QStringLiteral("FBNeo"), QStringLiteral("deckade-fbneo"),
         {QStringLiteral("Arcade"), QStringLiteral("Neo Geo"), QStringLiteral("CPS1/2/3"),
          QStringLiteral("Sega"), QStringLiteral("NES"), QStringLiteral("SNES"), QStringLiteral("PC Engine")},
         QStringLiteral("Native GGPO / rollback runtime")},
        {QStringLiteral("snes9x"), QStringLiteral("Snes9x"), QStringLiteral("deckade-snes9x"),
         {QStringLiteral("SNES")}, QStringLiteral("Native Snes9x runner (experimental)")},
        {QStringLiteral("flycast"), QStringLiteral("Flycast"), QStringLiteral("deckade-flycast"),
         {QStringLiteral("Dreamcast"), QStringLiteral("Naomi"), QStringLiteral("Atomiswave")},
         QStringLiteral("Flycast Dojo 6.46")},
        {QStringLiteral("ggpofba"), QStringLiteral("GGPOFBA"), QStringLiteral("deckade-ggpofba"),
         {QStringLiteral("Fightcade 1 legacy")}, QStringLiteral("Native Snes9x runner (experimental)")}
    };

    m_runtimeSummary.clear();
    for (const RuntimeDefinition &definition : definitions) {
        const QString executable = QDir(m_runtimeRoot).filePath(QStringLiteral("bin/%1").arg(definition.binary));
        const bool nativeAvailable = executableExists(executable);
        const bool available = nativeAvailable;
        QString note = definition.note;
        if (!available) {
            note = QStringLiteral("Not installed");
        }
        m_runtimeSummary.append(QVariantMap{
            {"id", definition.id},
            {"name", definition.name},
            {"available", available},
            {"binary", executable},
            {"systems", definition.systems},
            {"note", note}
        });
    }
}

QString FightcadeLauncher::normalizedEmulator(const QString &emulator) const
{
    const QString normalized = emulator.trimmed().toLower();
    if (normalized == QStringLiteral("fc1")) {
        return QStringLiteral("ggpofba");
    }
    if (normalized == QStringLiteral("nulldc")) {
        return QStringLiteral("flycast");
    }
    if (normalized == QStringLiteral("fcadefbneo") || normalized == QStringLiteral("fightcade-fbneo")) {
        return QStringLiteral("fbneo");
    }
    if (normalized == QStringLiteral("fcadesnes9x") || normalized == QStringLiteral("snes")) {
        return QStringLiteral("snes9x");
    }
    return normalized;
}

QString FightcadeLauncher::runtimeName(const QString &emulator) const
{
    if (emulator == QStringLiteral("fbneo")) return QStringLiteral("FBNeo");
    if (emulator == QStringLiteral("snes9x")) return QStringLiteral("Snes9x");
    if (emulator == QStringLiteral("flycast")) return QStringLiteral("Flycast");
    if (emulator == QStringLiteral("ggpofba")) return QStringLiteral("GGPOFBA");
    return emulator;
}

QString FightcadeLauncher::runtimeExecutable(const QString &emulator) const
{
    if (m_runtimeRoot.isEmpty()) {
        return {};
    }
    if (emulator == QStringLiteral("fbneo")) return QDir(m_runtimeRoot).filePath(QStringLiteral("bin/deckade-fbneo"));
    if (emulator == QStringLiteral("snes9x")) return QDir(m_runtimeRoot).filePath(QStringLiteral("bin/deckade-snes9x"));
    if (emulator == QStringLiteral("flycast")) return QDir(m_runtimeRoot).filePath(QStringLiteral("bin/deckade-flycast"));
    if (emulator == QStringLiteral("ggpofba")) return QDir(m_runtimeRoot).filePath(QStringLiteral("bin/deckade-ggpofba"));
    return {};
}

QString FightcadeLauncher::romPathFor(const QString &emulator) const
{
    const QString discovered = romRootForEmulator(m_settings->romPath(), emulator);
    if (!discovered.isEmpty()) {
        return discovered;
    }
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("roms/%1").arg(emulator));
}

QString FightcadeLauncher::romDirectoryFor(const QString &emulator) const
{
    const QString path = romPathFor(normalizedEmulator(emulator));
    QDir().mkpath(path);
    return path;
}

QString FightcadeLauncher::runtimeBinaryFor(const QString &emulator) const
{
    return runtimeExecutable(normalizedEmulator(emulator));
}

QString FightcadeLauncher::findRomFile(const QString &gameId, const QString &romRoot) const
{
    if (romRoot.isEmpty() || gameId.isEmpty()) {
        return {};
    }

    QStringList stems{gameId};
    if (gameId.startsWith(QStringLiteral("snes9x_"))) stems << gameId.mid(7);
    if (gameId.startsWith(QStringLiteral("snes_"))) stems << gameId.mid(5);
    if (gameId.startsWith(QStringLiteral("flycast_"))) stems << gameId.mid(8);
    if (gameId.startsWith(QStringLiteral("nulldc_"))) stems << gameId.mid(7);
    if (gameId.startsWith(QStringLiteral("fc1_"))) stems << gameId.mid(4);

    QStringList names;
    for (const QString &stem : std::as_const(stems)) {
        names << stem + QStringLiteral(".zip") << stem + QStringLiteral(".7z")
              << stem + QStringLiteral(".sfc") << stem + QStringLiteral(".smc")
              << stem + QStringLiteral(".bin") << stem + QStringLiteral(".cue")
              << stem + QStringLiteral(".gdi") << stem + QStringLiteral(".cdi")
              << stem + QStringLiteral(".chd") << stem + QStringLiteral(".dat");
    }

    QDirIterator iterator(romRoot, names, QDir::Files, QDirIterator::Subdirectories);
    if (iterator.hasNext()) {
        return iterator.next();
    }
    return {};
}

bool FightcadeLauncher::hasLocalRom(const QString &emulator, const QString &gameId) const
{
    return !findRomFile(gameId, romPathFor(normalizedEmulator(emulator))).isEmpty();
}

void normalizeControllerProfile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    file.close();
    QStringList output;
    bool changed = false;
    for (const QString &line : lines) {
        const QStringList fields = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (fields.size() < 8 || fields.at(0) == QStringLiteral("MACADE_GAMEPADS")) {
            output.append(line);
            continue;
        }
        QStringList next = fields;
        const QString info = QString::fromLatin1(QByteArray::fromHex(fields.at(4).toLatin1())).toLower();
        int direction = fields.at(7).toInt();
        if (fields.at(5) == QStringLiteral("h")) {
            if (info.contains(QStringLiteral("up"))) direction = 1;
            else if (info.contains(QStringLiteral("down"))) direction = 4;
            else if (info.contains(QStringLiteral("left"))) direction = 8;
            else if (info.contains(QStringLiteral("right"))) direction = 2;
        }
        if (next.at(7).toInt() != direction) {
            next[7] = QString::number(direction);
            changed = true;
        }
        output.append(next.join(QLatin1Char(' ')));
    }
    if (!changed) return;
    QFile out(path);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        out.write(output.join(QLatin1Char('\n')).toUtf8());
        out.write("\n");
    }
}

bool copyTrainingMode(const QString &source, const QString &target)
{
    QDir().mkpath(target);
    QDirIterator it(source, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString relative = QDir(source).relativeFilePath(path);
        const QString destination = QDir(target).filePath(relative);
        if (QFileInfo(path).isDir()) {
            QDir().mkpath(destination);
            continue;
        }
        QDir().mkpath(QFileInfo(destination).absolutePath());
        QFile::copy(path, destination);
        if (destination.endsWith(QStringLiteral(".lua"), Qt::CaseInsensitive)) {
            QFile file(destination);
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray data = file.readAll();
                file.close();
                // The Windows training-mode scripts use backslash separators
                // in dofile/resource paths. Normalize only path prefixes; a
                // blanket backslash replacement corrupts Lua escapes.
                bool changed = false;
                for (const QString &prefix : {QStringLiteral("inputs"), QStringLiteral("resources"),
                                              QStringLiteral("hitboxes"), QStringLiteral("games"),
                                              QStringLiteral("addon")}) {
                    const QByteArray from = prefix.toUtf8() + "\\\\";
                    const QByteArray to = prefix.toUtf8() + "/";
                    if (data.contains(from)) {
                        data.replace(from, to);
                        changed = true;
                    }
                }
                if (destination.endsWith(QStringLiteral("fbneo-training-mode.lua"))) {
                    const QByteArray disabled = data;
                    data.replace("DISABLE_SCROLLING_INPUT = false", "DISABLE_SCROLLING_INPUT = true");
                    // The training script calls gd.* directly; Lua's require()
                    // does not create that global on all Lua 5.1 builds.
                    data.replace("require \"gd\"", "gd = require \"gd\"");
                    // Enable the usual training HUD/damage readout by default.
                    data.replace("comboenabled = false", "comboenabled = true");
                    data.replace("p1healthenabled = false", "p1healthenabled = true");
                    data.replace("p2healthenabled = false", "p2healthenabled = true");
                    data.replace("p1meterenabled = false", "p1meterenabled = true");
                    data.replace("p2meterenabled = false", "p2meterenabled = true");
                    // KOF and several drivers ship incompatible hitbox maps that
                    // render as a blue/white grid. Keep hitboxes opt-in via the
                    // training menu instead of enabling them by default.
                    data.replace("hitbox = {\n\t\tenabled = true", "hitbox = {\n\t\tenabled = false");
                    changed = changed || data != disabled;
                }
                if (changed && file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    file.write(data);
                }
            }
        }
    }
    return QFileInfo(target).isDir();
}

bool FightcadeLauncher::prepareWorkDirectory(const QString &emulator, const QString &gameId)
{
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString workDir = QDir(dataRoot).filePath(QStringLiteral("runtime-work/%1").arg(emulator));
    if (!QDir().mkpath(workDir)) {
        setError(QStringLiteral("Could not create the emulator work directory."));
        return false;
    }

    const QString romRoot = romPathFor(emulator);
    if (romRoot.isEmpty() || !directoryExists(romRoot)) {
        setError(QStringLiteral("No ROM folder found for %1. Install Fightcade or set a ROM folder in Settings.").arg(emulator));
        return false;
    }

    const QString dataDir = QDir(workDir).filePath(QStringLiteral("data"));
    QDir().mkpath(dataDir);
    QDir().mkpath(QDir(dataDir).filePath(QStringLiteral("fbneo/config")));
    QDir().mkpath(QDir(workDir).filePath(QStringLiteral("savestates")));

    if (emulator == QStringLiteral("fbneo")) {
        const QString trainingSource = QDir(m_runtimeRoot).filePath(QStringLiteral("fbneo/fbneo-training-mode"));
        const QString trainingLink = QDir(workDir).filePath(QStringLiteral("fbneo-training-mode"));
        if (QFileInfo(trainingSource).isDir()) {
            if (QFileInfo(trainingLink).isSymLink()) {
                QFile::remove(trainingLink);
            }
            if (QFileInfo(trainingLink).isDir()) {
                QDir(trainingLink).removeRecursively();
            }
            if (!copyTrainingMode(trainingSource, trainingLink)) {
                qWarning() << "Could not copy Fightcade training mode; falling back to source link";
                QFile::link(trainingSource, trainingLink);
            }
        }

        const QString savestateSource = QDir(m_runtimeRoot).filePath(QStringLiteral("fbneo/savestates"));
        const QString savestateLink = QDir(workDir).filePath(QStringLiteral("savestates"));
        if (QFileInfo(savestateSource).isDir()) {
            const QFileInfo linkInfo(savestateLink);
            if (linkInfo.isSymLink() || QFile::exists(savestateLink)) {
                if (linkInfo.isSymLink() || QFileInfo(savestateLink).isFile()) {
                    QFile::remove(savestateLink);
                } else if (QDir(savestateLink).isEmpty()) {
                    QDir().rmdir(savestateLink);
                }
            }
            if (!QFileInfo::exists(savestateLink)) {
                QFile::link(savestateSource, savestateLink);
            }
        }

        // Overlay art and per-game score detection, read relative to the work
        // directory. Relink on each launch because AppImage mount paths change.
        for (const QString &bundled : {QStringLiteral("ui"), QStringLiteral("detector")}) {
            const QString source = QDir(m_runtimeRoot).filePath(QStringLiteral("fbneo/%1").arg(bundled));
            const QString link = QDir(workDir).filePath(bundled);
            if (QFileInfo(link).isSymLink()) {
                QFile::remove(link);
            }
            if (QFileInfo(source).isDir() && !QFileInfo::exists(link)) {
                QFile::link(source, link);
            }
        }

        const QString configDir = QDir(dataDir).filePath(QStringLiteral("fbneo/config"));
        const QString configPath = QDir(configDir).filePath(QStringLiteral("fbneo.ini"));
        const QString markerPath = QDir(workDir).filePath(QStringLiteral("deckade-rom-root"));
        QString previousRoot;
        QFile marker(markerPath);
        if (marker.open(QIODevice::ReadOnly | QIODevice::Text)) {
            previousRoot = QString::fromUtf8(marker.readAll()).trimmed();
        }
        marker.close();

        if (!QFile::exists(configPath) || previousRoot != romRoot) {
            QStringList romPaths{QDir(romRoot).absolutePath() + QStringLiteral("/")};
            QDirIterator directories(romRoot, QDir::Dirs | QDir::NoDotAndDotDot,
                                     QDirIterator::Subdirectories);
            while (directories.hasNext() && romPaths.size() < 20) {
                romPaths << directories.next() + QStringLiteral("/");
            }

            QFile config(configPath);
            if (config.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                QTextStream stream(&config);
                stream << "nIniVersion 0x029744\n";
                for (int i = 0; i < romPaths.size(); ++i) {
                    stream << "szAppRomPaths[" << i << "] " << romPaths.at(i) << "\n";
                }
                config.close();
            }

            if (marker.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                marker.write(romRoot.toUtf8());
                marker.close();
            }
        }
    }

    Q_UNUSED(gameId)
    return true;
}

bool FightcadeLauncher::launchRuntime(const QString &emulator, const QStringList &arguments,
                                      const QString &gameId, bool singlePlayer,
                                      const QVariantMap &extraEnvironment)
{
    if (m_running) {
        setError(QStringLiteral("An emulator is already running."));
        return false;
    }

    const QString executable = runtimeExecutable(emulator);
    if (!executableExists(executable)) {
        setError(QStringLiteral("The native %1 runtime is not installed. Add it under %2/bin/.")
                     .arg(runtimeName(emulator), m_runtimeRoot.isEmpty() ? QStringLiteral("runtime") : m_runtimeRoot));
        return false;
    }
    const QString romRoot = romPathFor(emulator);
    if (findRomFile(gameId, romRoot).isEmpty()) {
        setError(QStringLiteral("No ROM found for %1 in %2. Add the game ROM, then try again.")
                     .arg(gameId, romRoot.isEmpty() ? QStringLiteral("the ROM folder") : romRoot));
        return false;
    }
    if (!prepareWorkDirectory(emulator, gameId)) {
        return false;
    }

    const QString workDir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                                .filePath(QStringLiteral("runtime-work/%1").arg(emulator));

    m_process = new QProcess(this);
    m_process->setProgram(executable);
    m_process->setArguments(arguments);
    m_process->setWorkingDirectory(workDir);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("MACADE_FIGHTCADE_RUNTIME"), workDir);
    environment.insert(QStringLiteral("MACADE_ROM_DIR"), romRoot);
    environment.insert(QStringLiteral("XDG_DATA_HOME"), QDir(workDir).filePath(QStringLiteral("data")));
    environment.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
    // SDL's fullscreen-desktop path is unreliable through XWayland/sway. The
    // window is deliberately windowed; i3 and Steam Deck Game Mode can make it
    // fullscreen, and X11 is the reliable SDL backend on both.
    if (!qEnvironmentVariableIsEmpty("DISPLAY")) {
        environment.insert(QStringLiteral("SDL_VIDEODRIVER"), QStringLiteral("x11"));
    }
    if (singlePlayer) {
        environment.insert(QStringLiteral("MACADE_SINGLE_PLAYER_INPUT"), QStringLiteral("1"));
    } else {
        environment.remove(QStringLiteral("MACADE_SINGLE_PLAYER_INPUT"));
    }
    // Keep native GGPO/Quark diagnostics when a match does not reach video.
    const QString logDir = QDir(workDir).filePath(QStringLiteral("logs"));
    QDir().mkpath(logDir);
    environment.insert(QStringLiteral("quark.log"), QStringLiteral("1"));
    environment.insert(QStringLiteral("quark.log.timestamps"), QStringLiteral("1"));
    environment.insert(QStringLiteral("MACADE_QUARK_LOG_DIR"), logDir);
    environment.insert(QStringLiteral("DECKADE_P1_JOYSTICK_INDEX"),
                       QString::number(m_settings->controllerIndex()));
    // FBNeo's SDL input layer owns in-game controller mapping. Do not inject
    // Deckade's legacy per-game profile here: having both mapping systems live
    // caused duplicate/overridden bindings. The app still uses its own gamepad
    // input for UI navigation.
    if (emulator != QStringLiteral("fbneo")) {
        const QString profilePath = controllerProfilePath(gameId);
        if (QFile::exists(profilePath)) {
            normalizeControllerProfile(profilePath);
            environment.insert(QStringLiteral("MACADE_GAMEPAD_PROFILE_PATH"), profilePath);
        }
    }
    for (auto it = extraEnvironment.constBegin(); it != extraEnvironment.constEnd(); ++it) {
        environment.insert(it.key(), it.value().toString());
    }
    m_process->setProcessEnvironment(environment);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        if (m_process) {
            const QByteArray output = m_process->readAllStandardOutput();
            if (!output.isEmpty()) {
                qInfo().noquote() << output.trimmed();
            }
        }
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
                qInfo() << "Emulator exited" << exitCode << status;
                if (m_netplayProxy) {
                    m_netplayProxy->stop();
                    m_netplayProxy->deleteLater();
                    m_netplayProxy = nullptr;
                }
                if (m_process) {
                    m_process->deleteLater();
                    m_process = nullptr;
                }
                setRunning(false);
            });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (m_process) {
            setError(QStringLiteral("Emulator failed to start: %1").arg(m_process->errorString()));
            m_process->deleteLater();
            m_process = nullptr;
        }
        if (m_netplayProxy) {
            m_netplayProxy->stop();
            m_netplayProxy->deleteLater();
            m_netplayProxy = nullptr;
        }
        Q_UNUSED(error)
        setRunning(false);
    });

    qInfo().noquote() << "Launching" << executable << arguments.join(' ');
    m_lastError.clear();
    m_process->start();
    if (!m_process->waitForStarted(2500)) {
        setError(QStringLiteral("Could not start %1: %2").arg(runtimeName(emulator), m_process->errorString()));
        m_process->deleteLater();
        m_process = nullptr;
        if (m_netplayProxy) {
            m_netplayProxy->stop();
            m_netplayProxy->deleteLater();
            m_netplayProxy = nullptr;
        }
        return false;
    }

    setRunning(true);
    return true;
}

bool FightcadeLauncher::flycastCanStart(const QString &gameId)
{
    if (!executableExists(runtimeExecutable(QStringLiteral("flycast")))) {
        setError(QStringLiteral("The Flycast runtime is not installed."));
        return false;
    }
    if (m_running) {
        setError(QStringLiteral("An emulator is already running."));
        return false;
    }
    const QString romRoot = romPathFor(QStringLiteral("flycast"));
    if (findRomFile(gameId, romRoot).isEmpty()) {
        setError(QStringLiteral("No ROM found for %1 in %2. Add the game ROM, then try again.")
                     .arg(gameId, romRoot.isEmpty() ? QStringLiteral("the ROM folder") : romRoot));
        return false;
    }
    return true;
}

bool FightcadeLauncher::startFlycast(const QStringList &arguments)
{
    // Flycast keeps its config, BIOS files, and a ROMs link under FLYCAST_ROOT.
    const QString workDir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                                .filePath(QStringLiteral("runtime-work/flycast"));
    QDir().mkpath(QDir(workDir).filePath(QStringLiteral("data")));
    const QString romLink = QDir(workDir).filePath(QStringLiteral("ROMs"));
    if (QFileInfo(romLink).isSymLink()) QFile::remove(romLink);
    if (!QFileInfo::exists(romLink)) QFile::link(romPathFor(QStringLiteral("flycast")), romLink);

    // Holds the settings official clients run with. Some affect netplay sync.
    const QString configPath = QDir(workDir).filePath(QStringLiteral("emu.cfg"));
    if (!QFileInfo::exists(configPath)) {
        QFile::copy(QDir(m_runtimeRoot).filePath(QStringLiteral("flycast-emu.default.cfg")), configPath);
        QFile::setPermissions(configPath, QFile::ReadOwner | QFile::WriteOwner);
    }

    m_process = new QProcess(this);
    m_process->setProgram(runtimeExecutable(QStringLiteral("flycast")));
    m_process->setArguments(arguments);
    m_process->setWorkingDirectory(workDir);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("FLYCAST_ROOT"), workDir);
    m_process->setProcessEnvironment(environment);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        if (m_process) {
            const QByteArray output = m_process->readAllStandardOutput();
            if (!output.isEmpty()) qInfo().noquote() << output.trimmed();
        }
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
                qInfo() << "Flycast exited" << exitCode << status;
                reportFlycastFinished();
                if (m_netplayProxy) {
                    m_netplayProxy->stop();
                    m_netplayProxy->deleteLater();
                    m_netplayProxy = nullptr;
                }
                if (m_process) {
                    m_process->deleteLater();
                    m_process = nullptr;
                }
                setRunning(false);
            });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_process) {
            setError(QStringLiteral("Flycast failed to start: %1").arg(m_process->errorString()));
        }
    });

    m_process->start();
    if (!m_process->waitForStarted(2500)) {
        setError(QStringLiteral("Could not start Flycast: %1").arg(m_process->errorString()));
        m_process->deleteLater();
        m_process = nullptr;
        return false;
    }
    setRunning(true);
    return true;
}

bool FightcadeLauncher::launchFlycastMatch(const QVariantMap &match)
{
    const QString gameId = match.value(QStringLiteral("gameId")).toString();
    if (!flycastCanStart(gameId)) return false;

    if (m_netplayProxy) {
        m_netplayProxy->stop();
        m_netplayProxy->deleteLater();
    }
    m_netplayProxy = new NetplayProxy(match, this);
    m_netplayProxy->setPeerWaitMs(25000);
    if (!m_netplayProxy->prepare()) {
        m_netplayProxy->deleteLater();
        m_netplayProxy = nullptr;
        setError(QStringLiteral("Could not connect to your opponent."));
        return false;
    }

    FlycastLaunch::MatchRoute route;
    route.game = gameId;
    route.quarkId = match.value(QStringLiteral("quarkId")).toString();
    route.hosting = match.value(QStringLiteral("playerId")).toInt() == 0;
    route.delay = match.value(QStringLiteral("delay")).toInt();
    route.ranked = match.value(QStringLiteral("ranked")).toInt();
    route.server = m_netplayProxy->masterHost();
    route.spectatorPort = match.value(QStringLiteral("port")).toInt();
    if (route.hosting) {
        // The host's Flycast takes over the hole-punched port itself.
        route.remotePort = m_netplayProxy->peerPort();
        route.localPort = m_netplayProxy->releasePeerPort();
    } else {
        // The guest's Flycast talks to Deckade's relay on localhost.
        const int peerPort = m_netplayProxy->peerPort();
        route.localPort = peerPort > 1028 ? peerPort - 4 : 6004;
        route.remotePort = m_netplayProxy->proxyPort();
        m_netplayProxy->setEmulatorPort(static_cast<quint16>(route.localPort));
    }

    if (!startFlycast(FlycastLaunch::matchArguments(route))) {
        m_netplayProxy->stop();
        m_netplayProxy->deleteLater();
        m_netplayProxy = nullptr;
        return false;
    }
    m_flycastQuarkId = route.quarkId;
    m_flycastToken = match.value(QStringLiteral("token")).toString();
    return true;
}

// Official clients tell Fightcade when a Flycast match ends.
void FightcadeLauncher::reportFlycastFinished()
{
    if (m_flycastQuarkId.isEmpty() || m_flycastToken.isEmpty()) return;
    if (!m_network) m_network = new QNetworkAccessManager(this);
    QNetworkRequest request{QUrl(QLatin1String(FightcadeProtocol::ApiUrl))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    const QJsonObject body{{QStringLiteral("req"), QStringLiteral("quarkstatus")},
                           {QStringLiteral("quarkstatus"), QStringLiteral("finish")},
                           {QStringLiteral("quarkid"), m_flycastQuarkId},
                           {QStringLiteral("token"), m_flycastToken}};
    QNetworkReply *reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    m_flycastQuarkId.clear();
    m_flycastToken.clear();
}

bool FightcadeLauncher::launchMatch(const QVariantMap &match)
{
    const QString emulator = normalizedEmulator(match.value(QStringLiteral("emulator")).toString());
    const QString gameId = match.value(QStringLiteral("gameId")).toString();
    if (emulator == QStringLiteral("flycast")) {
        return launchFlycastMatch(match);
    }
    QVariantMap proxyEnvironment;
    if (emulator == QStringLiteral("fbneo")) {
        if (m_netplayProxy) {
            m_netplayProxy->stop();
            m_netplayProxy->deleteLater();
            m_netplayProxy = nullptr;
        }
        m_netplayProxy = new NetplayProxy(match, this);
        if (m_netplayProxy->prepare()) {
            proxyEnvironment = m_netplayProxy->environment();
            qInfo().noquote() << "Netplay proxy ready, log:" << m_netplayProxy->logPath();
        } else {
            qWarning().noquote() << "Netplay proxy unavailable; using native direct route";
            m_netplayProxy->deleteLater();
            m_netplayProxy = nullptr;
        }
    }
    QStringList arguments;
    if (emulator == QStringLiteral("fbneo")) {
        arguments << QStringLiteral("-joy") << uriForMatch(match);
    } else if (emulator == QStringLiteral("snes9x")) {
        arguments << QStringLiteral("quark:served,%1,%2.%3,%4,%5,%6")
                         .arg(gameId, match.value(QStringLiteral("quarkId")).toString())
                         .arg(match.value(QStringLiteral("playerId")).toInt())
                         .arg(match.value(QStringLiteral("port")).toInt())
                         .arg(match.value(QStringLiteral("delay")).toInt())
                         .arg(match.value(QStringLiteral("ranked")).toInt());
    } else {
        setError(QStringLiteral("No native runtime is available for %1 yet.").arg(runtimeName(emulator)));
        return false;
    }
    return launchRuntime(emulator, arguments, gameId, false, proxyEnvironment);
}

bool FightcadeLauncher::launchSpectate(const QVariantMap &match)
{
    const QString emulator = normalizedEmulator(match.value(QStringLiteral("emulator")).toString());
    const QString gameId = match.value(QStringLiteral("gameId")).toString();
    if (emulator == QStringLiteral("flycast")) {
        if (!flycastCanStart(gameId)) return false;
        return startFlycast(FlycastLaunch::spectateArguments(
            gameId, match.value(QStringLiteral("quarkId")).toString(),
            qEnvironmentVariable("DECKADE_PROXY_MASTER_HOST", "ggpo.fightcade.com"),
            match.value(QStringLiteral("port")).toInt()));
    }
    const QString command = FightcadeProtocol::quarkStream(
        gameId,
        match.value(QStringLiteral("quarkId")).toString() + QStringLiteral(".2"),
        match.value(QStringLiteral("port")).toInt());
    QStringList arguments;
    if (emulator == QStringLiteral("fbneo")) {
        arguments << QStringLiteral("-joy") << command;
    } else if (emulator == QStringLiteral("snes9x")) {
        arguments << command;
    } else {
        setError(QStringLiteral("No native runtime is available for %1 yet.").arg(runtimeName(emulator)));
        return false;
    }
    return launchRuntime(emulator, arguments, gameId, false);
}

bool FightcadeLauncher::launchSolo(const QString &emulatorName, const QString &gameId)
{
    const QString emulator = normalizedEmulator(emulatorName);
    if (emulator == QStringLiteral("flycast")) {
        return flycastCanStart(gameId) && startFlycast(FlycastLaunch::localArguments(gameId));
    }
    QStringList arguments;
    if (emulator == QStringLiteral("fbneo")) {
        arguments << QStringLiteral("-joy") << gameId;
    } else if (emulator == QStringLiteral("snes9x")) {
        arguments << gameId;
    } else {
        setError(QStringLiteral("No native runtime is available for %1 yet.").arg(runtimeName(emulator)));
        return false;
    }
    return launchRuntime(emulator, arguments, gameId, true);
}

bool FightcadeLauncher::launchTraining(const QString &emulatorName, const QString &gameId)
{
    const QString emulator = normalizedEmulator(emulatorName);
    if (emulator == QStringLiteral("flycast")) {
        return flycastCanStart(gameId) && startFlycast(FlycastLaunch::localArguments(gameId));
    }
    const QString command = QStringLiteral("macade:training,%1").arg(gameId);
    QStringList arguments;
    if (emulator == QStringLiteral("fbneo")) {
        arguments << QStringLiteral("-joy") << command;
    } else if (emulator == QStringLiteral("snes9x")) {
        arguments << command;
    } else {
        setError(QStringLiteral("No native runtime is available for %1 yet.").arg(runtimeName(emulator)));
        return false;
    }
    return launchRuntime(emulator, arguments, gameId, true);
}

void FightcadeLauncher::killEmulator()
{
    if (m_netplayProxy) {
        m_netplayProxy->stop();
        m_netplayProxy->deleteLater();
        m_netplayProxy = nullptr;
    }
    if (!m_process) {
        setRunning(false);
        return;
    }
    m_process->terminate();
    QProcess *process = m_process;
    QTimer::singleShot(2500, this, [process] {
        if (process && process->state() != QProcess::NotRunning) {
            process->kill();
        }
    });
}

void FightcadeLauncher::clearRunning()
{
    if (!m_process) {
        setRunning(false);
    }
}

void FightcadeLauncher::openRomFolder()
{
    const QString path = romDirectoryFor(QStringLiteral("fbneo"));
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void FightcadeLauncher::reportUserStatus(const QString &token)
{
    Q_UNUSED(token)
    // Native runtimes are wired directly; no WLAN probe process is needed.
}

void FightcadeLauncher::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }
    m_running = running;
    emit stateChanged();
}

void FightcadeLauncher::setError(const QString &message)
{
    m_lastError = message;
    emit stateChanged();
    emit errorOccurred(message);
}
