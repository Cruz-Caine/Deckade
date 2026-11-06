#include "romlibrary.h"

#include "fightcadelauncher.h"
#include "../core/settingsstore.h"

#include <QStandardPaths>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

namespace {
constexpr auto UserAgent = "Fightcade2-WIN32-v2.1.45";

QStringList jsonFilesFor(const QString &emulator)
{
    if (emulator == QStringLiteral("fbneo")) {
        return {
            QStringLiteral("fbneo_roms.json"), QStringLiteral("fbneo_nes_roms.json"),
            QStringLiteral("fbneo_md_roms.json"),
            QStringLiteral("fbneo_sms_roms.json"), QStringLiteral("fbneo_gg_roms.json"),
            QStringLiteral("fbneo_pce_roms.json"), QStringLiteral("fbneo_sg1k_roms.json"),
            QStringLiteral("fbneo_msx_roms.json"), QStringLiteral("fbneo_tg_roms.json"),
            QStringLiteral("fbneo_cv_roms.json")
        };
    }
    if (emulator == QStringLiteral("snes9x")) {
        return {QStringLiteral("snes9x_roms.json")};
    }
    if (emulator == QStringLiteral("flycast")) {
        return {QStringLiteral("flycast_roms.json"), QStringLiteral("nulldc_roms.json")};
    }
    if (emulator == QStringLiteral("ggpofba") || emulator == QStringLiteral("fc1")) {
        return {QStringLiteral("fc1_roms.json")};
    }
    return {};
}

bool trustedUrl(const QString &url)
{
    return url.startsWith(QStringLiteral("https://fightcade.download/"))
        || url.startsWith(QStringLiteral("https://archive.org/download/fightcaderoms/"))
        || url.startsWith(QStringLiteral("https://github.com/blueminder/flycast-netplay-savestates/raw/main/"))
        || url.startsWith(QStringLiteral("https://raw.githubusercontent.com/blueminder/flycast-netplay-savestates/"))
        || url.startsWith(QStringLiteral("https://sf2mix.github.io/downloads/"));
}
} // namespace

RomLibrary::RomLibrary(FightcadeLauncher *launcher, SettingsStore *settings, QObject *parent)
    : QObject(parent)
    , m_launcher(launcher)
    , m_settings(settings)
{
}

RomLibrary::~RomLibrary()
{
    cancel();
}

bool RomLibrary::hasRom(const QString &emulator, const QString &gameId) const
{
    return m_launcher && m_launcher->hasLocalRom(emulator, gameId);
}

void RomLibrary::loadJson(const QString &fileName)
{
    if (m_loadedFiles.contains(fileName)) {
        return;
    }
    m_loadedFiles.insert(fileName);

    QString path = QStringLiteral(":/assets/rom-json/%1").arg(fileName);
    if (!QFile::exists(path)) {
        path = QDir(QCoreApplication::applicationDirPath())
                   .filePath(QStringLiteral("../assets/rom-json/%1").arg(fileName));
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = document.object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const QJsonObject value = it.value().toObject();
        const QString url = value.value(QStringLiteral("download")).toString();
        if (url.isEmpty()) continue;

        Entry entry;
        entry.jsonFile = fileName;
        entry.key = it.key();
        entry.url = url;
        entry.copyTo = value.value(QStringLiteral("copy_to")).toString();
        const QJsonArray requiredList = value.value(QStringLiteral("require")).toArray();
        for (const QJsonValue &required : requiredList) {
            if (required.isString()) entry.requirements.append(required.toString());
        }
        if (!m_entries.contains(entry.key)) {
            m_entries.insert(entry.key, entry);
        }
    }
}

void RomLibrary::loadEntriesFor(const QString &emulator)
{
    m_entries.clear();
    m_loadedFiles.clear();
    for (const QString &file : jsonFilesFor(emulator)) {
        loadJson(file);
    }
}

QString RomLibrary::findMainKey(const QString &emulator, const QString &gameId) const
{
    QStringList candidates{gameId};
    if (emulator == QStringLiteral("snes9x") && !gameId.startsWith(QStringLiteral("snes_"))) {
        candidates.prepend(QStringLiteral("snes_") + gameId);
    }
    if (emulator == QStringLiteral("flycast")) {
        if (gameId.startsWith(QStringLiteral("flycast_"))) {
            candidates.append(gameId.mid(8));
        } else {
            candidates.prepend(QStringLiteral("flycast_") + gameId);
        }
    }
    if (emulator == QStringLiteral("ggpofba") || emulator == QStringLiteral("fc1")) {
        if (gameId.startsWith(QStringLiteral("fc1_"))) candidates.append(gameId.mid(4));
    }

    for (const QString &candidate : std::as_const(candidates)) {
        if (m_entries.contains(candidate)) return candidate;
    }
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        if (it.key().endsWith(gameId, Qt::CaseInsensitive)) return it.key();
    }
    return {};
}

QString RomLibrary::subfolderFor(const QString &jsonFile) const
{
    if (jsonFile == QStringLiteral("fbneo_roms.json")) return {};
    if (jsonFile.startsWith(QStringLiteral("fbneo_")) && jsonFile.endsWith(QStringLiteral("_roms.json"))) {
        QString system = jsonFile.mid(6);
        system.chop(10);
        if (system == QStringLiteral("md")) return QStringLiteral("megadrive");
        if (system == QStringLiteral("gg")) return QStringLiteral("gamegear");
        if (system == QStringLiteral("sg1k")) return QStringLiteral("sg1000");
        if (system == QStringLiteral("cv")) return QStringLiteral("coleco");
        return system;
    }
    return {};
}

QString RomLibrary::fileNameForUrl(const QString &url) const
{
    QString name = QFileInfo(QUrl(url).path()).fileName();
    name = QUrl::fromPercentEncoding(name.toUtf8());
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._()\\- ]")), QStringLiteral("_"));
    return name;
}

QString RomLibrary::flycastDataDirectory() const
{
    if (!m_launcher) return {};
    if (m_launcher->installMethod() == QStringLiteral("flatpak")) {
        return QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
            .filePath(QStringLiteral(".var/app/com.fightcade.Fightcade/data/config/flycast/data"));
    }
    if (m_launcher->installMethod() == QStringLiteral("native")
        && !m_launcher->installPath().isEmpty()) {
        return QDir(m_launcher->installPath()).filePath(QStringLiteral("emulator/flycast/data"));
    }
    return {};
}

QString RomLibrary::destinationFor(const Entry &entry) const
{
    if (!entry.copyTo.isEmpty()) {
        const QString dataDir = flycastDataDirectory();
        if (!dataDir.isEmpty()) {
            return QDir(dataDir).filePath(QFileInfo(entry.copyTo).fileName());
        }
    }

    const QString folder = QDir(m_romRoot).filePath(subfolderFor(entry.jsonFile));
    QString name = fileNameForUrl(entry.url);
    if (name.isEmpty()) name = entry.key + QStringLiteral(".zip");
    return QDir(folder).filePath(name);
}

bool RomLibrary::collect(const QString &key)
{
    if (m_collected.contains(key)) {
        return true;
    }
    if (!m_entries.contains(key)) {
        fail(QStringLiteral("Fightcade's ROM database has no entry for %1.").arg(key));
        return false;
    }
    m_collected.insert(key);
    const Entry entry = m_entries.value(key);

    for (const QString &required : entry.requirements) {
        if (!collect(required)) return false;
    }

    const QString destination = destinationFor(entry);
    if (!QFileInfo::exists(destination)) {
        m_queue.enqueue(key);
    }
    return true;
}

void RomLibrary::ensureRom(const QString &emulator, const QString &gameId)
{
    if (m_busy) {
        if (emulator == m_emulator && gameId == m_gameId) {
            emit notice(QStringLiteral("ROM download already in progress."));
        } else {
            emit romFailed(emulator, gameId, QStringLiteral("Another ROM download is in progress."));
        }
        return;
    }

    loadEntriesFor(emulator);
    const QString key = findMainKey(emulator, gameId);
    if (key.isEmpty()) {
        emit romFailed(emulator, gameId,
                       QStringLiteral("No ROM entry for %1. Update the ROM database or add the file manually.").arg(gameId));
        return;
    }

    m_emulator = emulator;
    m_gameId = gameId;
    m_mainKey = key;
    m_romRoot = m_launcher ? m_launcher->romDirectoryFor(emulator) : QString();
    if (m_romRoot.isEmpty()) {
        emit romFailed(emulator, gameId, QStringLiteral("Could not determine the ROM folder."));
        return;
    }

    m_queue.clear();
    m_collected.clear();
    m_cancelled = false;
    m_mainDestination = destinationFor(m_entries.value(key));
    if (!collect(key)) {
        return;
    }

    if (m_queue.isEmpty()) {
        emit romReady(emulator, gameId, m_mainDestination);
        reset();
        return;
    }

    if (!m_settings->autoDownloadRoms()) {
        emit romFailed(emulator, gameId,
                       QStringLiteral("ROM is missing and automatic downloads are disabled. Add it to %1.").arg(m_romRoot));
        reset();
        return;
    }

    m_busy = true;
    emit stateChanged();
    setStatus(QStringLiteral("Preparing ROM..."));
    startNext();
}

void RomLibrary::startNext()
{
    if (m_cancelled) return;
    while (!m_queue.isEmpty()) {
        const QString key = m_queue.dequeue();
        if (!m_entries.contains(key)) {
            fail(QStringLiteral("Missing ROM metadata for %1.").arg(key));
            return;
        }
        const Entry entry = m_entries.value(key);
        const QString destination = destinationFor(entry);
        if (QFileInfo::exists(destination)) continue;

        QDir().mkpath(QFileInfo(destination).absolutePath());
        m_partPath = destination + QStringLiteral(".part");
        m_output = new QFile(m_partPath, this);
        if (!m_output->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            fail(QStringLiteral("Could not write to %1.").arg(m_partPath));
            return;
        }
        download(entry);
        return;
    }

    finishReady();
}

void RomLibrary::download(const Entry &entry)
{
    if (!trustedUrl(entry.url)) {
        fail(QStringLiteral("Refusing an untrusted ROM URL."));
        return;
    }

    QNetworkRequest request(QUrl(entry.url));
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(UserAgent));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    setStatus(QStringLiteral("Downloading %1...").arg(fileNameForUrl(entry.url)));
    m_reply = m_network.get(request);

    connect(m_reply, &QNetworkReply::downloadProgress, this,
            [this, entry](qint64 received, qint64 total) {
                if (total > 0) {
                    setProgress(static_cast<qreal>(received) / static_cast<qreal>(total));
                    setStatus(QStringLiteral("Downloading %1  %2%")
                                  .arg(fileNameForUrl(entry.url))
                                  .arg(static_cast<int>(m_progress * 100)));
                } else {
                    setStatus(QStringLiteral("Downloading %1...").arg(fileNameForUrl(entry.url)));
                }
            });
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_reply && m_output) m_output->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, entry] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        if (reply) reply->deleteLater();
        if (m_output) {
            m_output->close();
            m_output->deleteLater();
            m_output = nullptr;
        }
        if (m_cancelled) return;

        if (!reply || reply->error() != QNetworkReply::NoError) {
            const QString error = reply ? reply->errorString() : QStringLiteral("Download cancelled.");
            QFile::remove(m_partPath);
            fail(QStringLiteral("ROM download failed: %1").arg(error));
            return;
        }

        const QString destination = destinationFor(entry);
        QFile::remove(destination);
        if (!QFile::rename(m_partPath, destination)) {
            fail(QStringLiteral("Could not move the downloaded ROM into place."));
            return;
        }
        m_partPath.clear();
        setProgress(1.0);
        startNext();
    });
}

void RomLibrary::finishReady()
{
    const QString emulator = m_emulator;
    const QString gameId = m_gameId;
    const QString path = m_mainDestination;
    reset();
    emit romReady(emulator, gameId, path);
}

void RomLibrary::fail(const QString &message)
{
    const QString emulator = m_emulator;
    const QString gameId = m_gameId;
    reset();
    emit romFailed(emulator, gameId, message);
}

void RomLibrary::cancel()
{
    if (!m_busy) return;
    m_cancelled = true;
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_output) {
        m_output->close();
        m_output->deleteLater();
        m_output = nullptr;
    }
    if (!m_partPath.isEmpty()) QFile::remove(m_partPath);
    m_partPath.clear();
    const QString emulator = m_emulator;
    const QString gameId = m_gameId;
    reset();
    emit romFailed(emulator, gameId, QStringLiteral("ROM download cancelled."));
}

void RomLibrary::reset()
{
    m_busy = false;
    m_cancelled = false;
    m_status.clear();
    m_progress = 0;
    m_emulator.clear();
    m_gameId.clear();
    m_mainKey.clear();
    m_mainDestination.clear();
    m_romRoot.clear();
    m_queue.clear();
    m_collected.clear();
    m_entries.clear();
    m_loadedFiles.clear();
    m_partPath.clear();
    if (m_output) {
        m_output->close();
        m_output->deleteLater();
        m_output = nullptr;
    }
    emit stateChanged();
    emit progressChanged();
}

void RomLibrary::setStatus(const QString &status)
{
    if (m_status == status) return;
    m_status = status;
    emit stateChanged();
}

void RomLibrary::setProgress(qreal progress)
{
    const qreal normalized = qBound<qreal>(0.0, progress, 1.0);
    if (qFuzzyCompare(m_progress, normalized)) return;
    m_progress = normalized;
    emit progressChanged();
}
