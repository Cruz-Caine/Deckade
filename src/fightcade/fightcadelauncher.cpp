#include "fightcadelauncher.h"

#include "../core/settingsstore.h"
#include "protocol.h"
#include "netplayproxy.h"
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

QString readVersion(const QString &root)
{
    QFile file(QDir(root).filePath(QStringLiteral("VERSION.txt")));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString nativeFightcadeRoot(const QString &configured)
{
    QStringList candidates;
    if (!configured.isEmpty()) {
        candidates << configured;
    }
    if (qEnvironmentVariableIsSet("DECKADE_FIGHTCODE_DIR")) {
        candidates << qEnvironmentVariable("DECKADE_FIGHTCODE_DIR");
    }
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    candidates << QDir(home).filePath(QStringLiteral("Fightcade"))
               << QDir(home).filePath(QStringLiteral("Applications/Fightcade"))
               << QDir(home).filePath(QStringLiteral("Games/Fightcade"))
               << QDir(home).filePath(QStringLiteral("Downloads/Fightcade"))
               << QDir(home).filePath(QStringLiteral("Desktop/Fightcade"))
               << QDir(home).filePath(QStringLiteral(".local/share/Fightcade"))
               << QStringLiteral("/opt/Fightcade")
               << QStringLiteral("/usr/local/Fightcade");

    for (const QString &candidate : std::as_const(candidates)) {
        if (candidate.isEmpty()) {
            continue;
        }
        const QString root = cleanPath(candidate);
        if (QFileInfo(QDir(root).filePath(QStringLiteral("emulator/fcade"))).isFile()) {
            return root;
        }
    }
    return {};
}

bool flatpakInstalled()
{
    const QString flatpak = QStandardPaths::findExecutable(QStringLiteral("flatpak"));
    if (flatpak.isEmpty()) {
        return false;
    }
    QProcess process;
    process.start(flatpak, {QStringLiteral("info"), QStringLiteral("com.fightcade.Fightcade")});
    if (!process.waitForFinished(1500)) {
        process.kill();
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

QString flatpakRomRoot()
{
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return QDir(home).filePath(QStringLiteral(".var/app/com.fightcade.Fightcade/data/ROMs"));
}

QString discoverRomPath(const QString &fightcadeRoot, const QString &configured)
{
    const QStringList candidates = {
        configured,
        fightcadeRoot.isEmpty() ? QString() : QDir(fightcadeRoot).filePath(QStringLiteral("emulator/fbneo/ROMs")),
        fightcadeRoot.isEmpty() ? QString() : QDir(fightcadeRoot).filePath(QStringLiteral("ROMs")),
        fightcadeRoot.isEmpty() ? QString() : QDir(fightcadeRoot).filePath(QStringLiteral("emulator/fbneo/roms")),
        fightcadeRoot.isEmpty() ? QString() : QDir(fightcadeRoot).filePath(QStringLiteral("emulator/fbneo")),
        flatpakRomRoot() + QStringLiteral("/fbneo"),
        flatpakRomRoot(),
        QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath(QStringLiteral("deckade/roms/fbneo"))
    };

    for (const QString &candidate : candidates) {
        if (candidate.isEmpty()) {
            continue;
        }
        const QString path = cleanPath(candidate);
        if (directoryExists(path)) {
            return path;
        }
    }
    return {};
}

QString romRootForEmulator(const QString &fightcadeRoot, const QString &configured,
                           const QString &emulator)
{
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString flatpakRoot = QDir(home).filePath(QStringLiteral(".var/app/com.fightcade.Fightcade/data/ROMs"));
    QStringList candidates;
    if (!configured.isEmpty()) {
        candidates << configured
                   << QDir(configured).filePath(emulator);
    }
    if (!fightcadeRoot.isEmpty()) {
        candidates << QDir(fightcadeRoot).filePath(QStringLiteral("emulator/%1/ROMs").arg(emulator))
                   << QDir(fightcadeRoot).filePath(QStringLiteral("emulator/%1/roms").arg(emulator))
                   << QDir(fightcadeRoot).filePath(QStringLiteral("ROMs/%1").arg(emulator))
                   << QDir(fightcadeRoot).filePath(QStringLiteral("emulator/%1").arg(emulator))
                   << QDir(fightcadeRoot).filePath(QStringLiteral("ROMs"));
    }
    candidates << QDir(flatpakRoot).filePath(emulator)
               << flatpakRoot
               << QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
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
    connect(m_settings, &SettingsStore::installMethodChanged, this, &FightcadeLauncher::refreshInstall);
    connect(m_settings, &SettingsStore::fightcadePathChanged, this, &FightcadeLauncher::refreshInstall);
    connect(m_settings, &SettingsStore::romPathChanged, this, &FightcadeLauncher::refreshInstall);
    m_externalMonitor.setInterval(700);
    connect(&m_externalMonitor, &QTimer::timeout, this, &FightcadeLauncher::monitorExternalEmulator);
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

void FightcadeLauncher::setInstallMethod(const QString &method)
{
    m_settings->setInstallMethod(method);
}

void FightcadeLauncher::setInstallPath(const QString &path)
{
    m_settings->setFightcadePath(path);
}

void FightcadeLauncher::inspectInstall()
{
    const QString nativeRoot = nativeFightcadeRoot(m_settings->fightcadePath());
    const bool hasFlatpak = flatpakInstalled();
    const QString method = m_settings->installMethod();

    m_installMethod = QStringLiteral("missing");
    m_installPath.clear();
    m_installedVersion.clear();

    if (method == QStringLiteral("native")) {
        if (!nativeRoot.isEmpty()) {
            m_installMethod = QStringLiteral("native");
            m_installPath = nativeRoot;
            m_installedVersion = readVersion(nativeRoot);
        }
    } else if (method == QStringLiteral("flatpak")) {
        if (hasFlatpak) {
            m_installMethod = QStringLiteral("flatpak");
            m_installPath = QStringLiteral("com.fightcade.Fightcade");
        }
    } else {
        if (!nativeRoot.isEmpty()) {
            m_installMethod = QStringLiteral("native");
            m_installPath = nativeRoot;
            m_installedVersion = readVersion(nativeRoot);
        } else if (hasFlatpak) {
            m_installMethod = QStringLiteral("flatpak");
            m_installPath = QStringLiteral("com.fightcade.Fightcade");
        }
    }

    m_romPath = romRootForEmulator(nativeRoot, m_settings->romPath(), QStringLiteral("fbneo"));

    if (m_installMethod == QStringLiteral("native")) {
        m_installDetail = m_installedVersion.isEmpty()
            ? QStringLiteral("Native Fightcade at %1").arg(m_installPath)
            : QStringLiteral("Fightcade %1 at %2").arg(m_installedVersion, m_installPath);
    } else if (m_installMethod == QStringLiteral("flatpak")) {
        m_installDetail = QStringLiteral("Fightcade Flatpak (com.fightcade.Fightcade)");
    } else {
        m_installDetail = QStringLiteral("No Fightcade installation found. Set it in Settings.");
    }
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
         QStringLiteral("Official native Flycast through the Fightcade helper")},
        {QStringLiteral("ggpofba"), QStringLiteral("GGPOFBA"), QStringLiteral("deckade-ggpofba"),
         {QStringLiteral("Fightcade 1 legacy")}, QStringLiteral("Native Snes9x runner (experimental)")}
    };

    m_runtimeSummary.clear();
    for (const RuntimeDefinition &definition : definitions) {
        const QString executable = QDir(m_runtimeRoot).filePath(QStringLiteral("bin/%1").arg(definition.binary));
        const bool nativeAvailable = executableExists(executable);
        const bool helperAvailable = definition.id == QStringLiteral("flycast")
            && m_installMethod != QStringLiteral("missing");
        const bool available = nativeAvailable || helperAvailable;
        QString note = definition.note;
        if (!available) {
            note = QStringLiteral("Not installed");
        } else if (helperAvailable && !nativeAvailable) {
            note = QStringLiteral("Official native Flycast via Fightcade helper");
        }
        m_runtimeSummary.append(QVariantMap{
            {"id", definition.id},
            {"name", definition.name},
            {"available", available},
            {"helper", helperAvailable && !nativeAvailable},
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
    const QString discovered = romRootForEmulator(
        m_installMethod == QStringLiteral("native") ? m_installPath : QString(),
        m_settings->romPath(), emulator);
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

QString findSavestateDirectory(const QString &romRoot, const QString &installPath,
                               const QString &runtimeRoot)
{
    QStringList candidates;
    if (!runtimeRoot.isEmpty()) {
        candidates << QDir(runtimeRoot).filePath(QStringLiteral("fbneo/savestates"));
    }
    if (!romRoot.isEmpty()) {
        const QDir rom(romRoot);
        candidates << rom.filePath(QStringLiteral("../../config/fcadefbneo/savestates"))
                   << rom.filePath(QStringLiteral("../savestates"))
                   << rom.filePath(QStringLiteral("savestates"));
    }
    if (!installPath.isEmpty()) {
        const QDir install(installPath);
        candidates << install.filePath(QStringLiteral("data/config/fcadefbneo/savestates"))
                   << install.filePath(QStringLiteral("emulator/fbneo/savestates"))
                   << install.filePath(QStringLiteral("savestates"));
    }
    for (const QString &candidate : std::as_const(candidates)) {
        if (QFileInfo(candidate).isDir()) return QDir(candidate).absolutePath();
    }
    return {};
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
        QString trainingSource = QDir(m_runtimeRoot).filePath(QStringLiteral("fbneo/fbneo-training-mode"));
        if (!QFileInfo(trainingSource).isDir()) {
            trainingSource = QDir(romRoot).filePath(QStringLiteral("../../fbneo-training-mode"));
        }
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

        const QString savestateSource = findSavestateDirectory(
            romRoot, m_installMethod == QStringLiteral("native") ? m_installPath : QString(),
            m_runtimeRoot);
        const QString savestateLink = QDir(workDir).filePath(QStringLiteral("savestates"));
        if (!savestateSource.isEmpty()) {
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
                qInfo().noquote() << "Using Fightcade savestates:" << savestateSource;
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
    // FBNeo's training-mode Lua requires the Lua-GD module. The module is
    // loaded with a relative ./?.so path after FBNeo changes into the training
    // script directory, so expose the runtime workdir explicitly.
    const QString luaGd = QDir(m_runtimeRoot).filePath(QStringLiteral("fbneo/gd.so"));
    const QString luaGdTarget = QDir(workDir).filePath(QStringLiteral("gd.so"));
    if (QFileInfo::exists(luaGd) && !QFileInfo::exists(luaGdTarget)) {
        QFile::copy(luaGd, luaGdTarget);
    }
    environment.insert(QStringLiteral("LUA_CPATH"),
                       QDir(workDir).filePath(QStringLiteral("?.so")) + QLatin1Char(';'));
    // Keep native GGPO/Quark diagnostics when a match does not reach video.
    const QString logDir = QDir(workDir).filePath(QStringLiteral("logs"));
    QDir().mkpath(logDir);
    environment.insert(QStringLiteral("quark.log"), QStringLiteral("1"));
    environment.insert(QStringLiteral("quark.log.timestamps"), QStringLiteral("1"));
    environment.insert(QStringLiteral("MACADE_QUARK_LOG_DIR"), logDir);
    environment.insert(QStringLiteral("DECKADE_P1_JOYSTICK_INDEX"),
                       QString::number(m_settings->controllerIndex()));
    const QString profilePath = controllerProfilePath(gameId);
    if (QFile::exists(profilePath)) {
        normalizeControllerProfile(profilePath);
        environment.insert(QStringLiteral("MACADE_GAMEPAD_PROFILE_PATH"), profilePath);
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

bool FightcadeLauncher::isHelperEmulator(const QString &emulator) const
{
    return emulator == QStringLiteral("flycast")
        || emulator == QStringLiteral("duckstation")
        || emulator == QStringLiteral("custom");
}

QString FightcadeLauncher::officialMatchUri(const QVariantMap &match) const
{
    const QString emulator = normalizedEmulator(match.value(QStringLiteral("emulator")).toString());
    const QString gameId = match.value(QStringLiteral("gameId")).toString();
    const QString quarkId = match.value(QStringLiteral("quarkId")).toString();
    const int playerId = match.value(QStringLiteral("playerId")).toInt();
    const int port = match.value(QStringLiteral("port")).toInt();
    const int delay = match.value(QStringLiteral("delay")).toInt();
    const int ranked = match.value(QStringLiteral("ranked")).toInt();
    const QString token = match.value(QStringLiteral("token")).toString();

    if (isHelperEmulator(emulator)) {
        return QStringLiteral("fcade://launch/%1/%2/%3.%4,%5,%6,%7/%8")
            .arg(emulator, gameId, quarkId).arg(playerId).arg(port).arg(delay).arg(ranked).arg(token);
    }
    return QStringLiteral("fcade://served/%1/%2/%3.%4,%5,%6,%7")
        .arg(emulator, gameId, quarkId).arg(playerId).arg(port).arg(delay).arg(ranked);
}

QString FightcadeLauncher::officialSpectateUri(const QVariantMap &match) const
{
    const QString emulator = normalizedEmulator(match.value(QStringLiteral("emulator")).toString());
    return QStringLiteral("fcade://stream/%1/%2/%3.2,%4")
        .arg(emulator, match.value(QStringLiteral("gameId")).toString(),
             match.value(QStringLiteral("quarkId")).toString())
        .arg(match.value(QStringLiteral("port")).toInt());
}

bool FightcadeLauncher::launchDirectFlycast(const QString &gameId, bool training)
{
    if (m_installMethod == QStringLiteral("missing")) {
        setError(QStringLiteral("Flycast needs the official Fightcade installation."));
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

    QString entry = gameId;
    if (entry.startsWith(QStringLiteral("flycast_"))) entry = entry.mid(8);

    QStringList flycastArgs{
        QStringLiteral("-config"), QStringLiteral("dojo:GameEntry=%1").arg(entry),
        QStringLiteral("-config"), QStringLiteral("dojo:TestGame=no"),
        QStringLiteral("-config"), QStringLiteral("dojo:Enable=no"),
        QStringLiteral("-config"), QStringLiteral("dojo:EnableMatchCode=no")
    };
    if (training) {
        flycastArgs << QStringLiteral("-config") << QStringLiteral("dojo:Training=yes");
    }

    m_process = new QProcess(this);
    if (m_installMethod == QStringLiteral("flatpak")) {
        m_process->setProgram(QStringLiteral("flatpak"));
        QStringList args{
            QStringLiteral("run"),
            QStringLiteral("--command=/app/fightcade/Fightcade/emulator/flycast/flycast.elf"),
            QStringLiteral("com.fightcade.Fightcade")
        };
        args += flycastArgs;
        m_process->setArguments(args);
        m_process->setWorkingDirectory(QDir::homePath());
    } else {
        const QString flycast = QDir(m_installPath).filePath(QStringLiteral("emulator/flycast/flycast.elf"));
        if (!QFileInfo(flycast).isExecutable()) {
            setError(QStringLiteral("The official native Flycast binary was not found."));
            delete m_process;
            m_process = nullptr;
            return false;
        }
        m_process->setProgram(flycast);
        m_process->setArguments(flycastArgs);
        m_process->setWorkingDirectory(QDir(m_installPath).filePath(QStringLiteral("emulator/flycast")));
    }

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
    if (m_installMethod == QStringLiteral("native")) {
        environment.insert(QStringLiteral("FLYCAST_ROOT"),
                           QDir(m_installPath).filePath(QStringLiteral("emulator/flycast")));
    }
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
                if (m_process) {
                    m_process->deleteLater();
                    m_process = nullptr;
                }
                setRunning(false);
            });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_process) {
            setError(QStringLiteral("Flycast failed to start: %1").arg(m_process->errorString()));
            m_process->deleteLater();
            m_process = nullptr;
        }
        setRunning(false);
    });

    if (!m_process->waitForStarted(2500)) {
        setError(QStringLiteral("Could not start Flycast: %1").arg(m_process->errorString()));
        m_process->deleteLater();
        m_process = nullptr;
        return false;
    }
    setRunning(true);
    return true;
}

bool FightcadeLauncher::launchOfficialHelper(const QString &uri, const QString &emulator)
{
    if (m_installMethod == QStringLiteral("missing")) {
        setError(QStringLiteral("The official Fightcade helper is not available."));
        return false;
    }
    if (m_running) {
        setError(QStringLiteral("An emulator is already running."));
        return false;
    }

    m_helperProcess = new QProcess(this);
    if (m_installMethod == QStringLiteral("flatpak")) {
        m_helperProcess->setProgram(QStringLiteral("flatpak"));
        m_helperProcess->setArguments({QStringLiteral("run"), QStringLiteral("--command=fcade-quark"),
                                       QStringLiteral("com.fightcade.Fightcade"), uri});
        m_helperProcess->setWorkingDirectory(QDir::homePath());
    } else {
        m_helperProcess->setProgram(m_installPath + QStringLiteral("/emulator/fcade"));
        m_helperProcess->setArguments({uri});
        m_helperProcess->setWorkingDirectory(m_installPath + QStringLiteral("/emulator"));
    }
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
    m_helperProcess->setProcessEnvironment(environment);
    m_helperProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_helperProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus status) {
                qInfo() << "Fightcade helper exited" << code << status;
                if (m_helperProcess) {
                    m_helperProcess->deleteLater();
                    m_helperProcess = nullptr;
                }
                if (m_externalMarker.isEmpty()) {
                    setRunning(false);
                }
            });
    connect(m_helperProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_helperProcess) {
            setError(QStringLiteral("Fightcade helper failed: %1").arg(m_helperProcess->errorString()));
        }
    });

    m_helperProcess->start();
    if (!m_helperProcess->waitForStarted(2500)) {
        setError(QStringLiteral("Could not start the Fightcade helper."));
        m_helperProcess->deleteLater();
        m_helperProcess = nullptr;
        return false;
    }

    m_externalHelper = true;
    m_externalMarker = emulator == QStringLiteral("flycast") ? QStringLiteral("flycast")
        : emulator == QStringLiteral("duckstation") ? QStringLiteral("duckstation")
        : QString();
    m_externalStartedAt = QDateTime::currentMSecsSinceEpoch();
    setRunning(true);
    m_externalMonitor.start();
    return true;
}

bool FightcadeLauncher::externalEmulatorRunning() const
{
    if (m_externalMarker.isEmpty()) {
        return false;
    }
    QDir proc(QStringLiteral("/proc"));
    const QStringList entries = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool isPid = false;
        entry.toInt(&isPid);
        if (!isPid) continue;
        QFile file(QStringLiteral("/proc/%1/cmdline").arg(entry));
        if (!file.open(QIODevice::ReadOnly)) continue;
        const QString command = QString::fromUtf8(file.readAll()).replace(QChar('\0'), QChar(' '));
        if (!command.contains(m_externalMarker, Qt::CaseInsensitive)) continue;
        if (command.contains(QStringLiteral("fcade"), Qt::CaseInsensitive)
            || command.contains(QStringLiteral("deckade"), Qt::CaseInsensitive)
            || command.contains(QStringLiteral("pgrep"), Qt::CaseInsensitive)) {
            continue;
        }
        return true;
    }
    return false;
}

void FightcadeLauncher::monitorExternalEmulator()
{
    if (!m_externalHelper) {
        m_externalMonitor.stop();
        return;
    }
    if (externalEmulatorRunning()) {
        return;
    }
    const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_externalStartedAt;
    if (elapsed > 5000) {
        m_externalMonitor.stop();
        m_externalHelper = false;
        m_externalMarker.clear();
        setRunning(false);
    }
}

bool FightcadeLauncher::launchMatch(const QVariantMap &match)
{
    const QString emulator = normalizedEmulator(match.value(QStringLiteral("emulator")).toString());
    const QString gameId = match.value(QStringLiteral("gameId")).toString();
    if (isHelperEmulator(emulator) && !executableExists(runtimeExecutable(emulator))) {
        const QString helperRomRoot = romPathFor(emulator);
        if (findRomFile(gameId, helperRomRoot).isEmpty()) {
            setError(QStringLiteral("No ROM found for %1 in %2. Add the game ROM, then try again.")
                         .arg(gameId, helperRomRoot.isEmpty() ? QStringLiteral("the ROM folder") : helperRomRoot));
            return false;
        }
        return launchOfficialHelper(officialMatchUri(match), emulator);
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
    if (isHelperEmulator(emulator) && !executableExists(runtimeExecutable(emulator))) {
        const QString helperRomRoot = romPathFor(emulator);
        if (findRomFile(gameId, helperRomRoot).isEmpty()) {
            setError(QStringLiteral("No ROM found for %1 in %2. Add the game ROM, then try again.")
                         .arg(gameId, helperRomRoot.isEmpty() ? QStringLiteral("the ROM folder") : helperRomRoot));
            return false;
        }
        return launchOfficialHelper(officialSpectateUri(match), emulator);
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
        return launchDirectFlycast(gameId, false);
    }
    if (isHelperEmulator(emulator) && !executableExists(runtimeExecutable(emulator))) {
        const QString helperRomRoot = romPathFor(emulator);
        if (findRomFile(gameId, helperRomRoot).isEmpty()) {
            setError(QStringLiteral("No ROM found for %1 in %2. Add the game ROM, then try again.")
                         .arg(gameId, helperRomRoot.isEmpty() ? QStringLiteral("the ROM folder") : helperRomRoot));
            return false;
        }
        return launchOfficialHelper(QStringLiteral("fcade://play/%1/%2").arg(emulator, gameId), emulator);
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
        return launchDirectFlycast(gameId, true);
    }
    if (isHelperEmulator(emulator) && !executableExists(runtimeExecutable(emulator))) {
        const QString helperRomRoot = romPathFor(emulator);
        if (findRomFile(gameId, helperRomRoot).isEmpty()) {
            setError(QStringLiteral("No ROM found for %1 in %2. Add the game ROM, then try again.")
                         .arg(gameId, helperRomRoot.isEmpty() ? QStringLiteral("the ROM folder") : helperRomRoot));
            return false;
        }
        return launchOfficialHelper(QStringLiteral("fcade://training/%1/%2").arg(emulator, gameId), emulator);
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
    if (m_externalHelper) {
        m_externalMonitor.stop();
        m_externalHelper = false;
        if (m_installMethod == QStringLiteral("flatpak")) {
            QProcess::startDetached(QStringLiteral("flatpak"),
                                    {QStringLiteral("run"), QStringLiteral("--command=fcade-quark"),
                                     QStringLiteral("com.fightcade.Fightcade"), QStringLiteral("fcade://killemu")});
        } else if (m_installMethod == QStringLiteral("native")) {
            QProcess::startDetached(m_installPath + QStringLiteral("/emulator/fcade"),
                                    {QStringLiteral("fcade://killemu")});
        }
        setRunning(false);
        return;
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
    if (m_externalHelper) {
        m_externalMonitor.stop();
        m_externalHelper = false;
    }
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
