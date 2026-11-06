#include "settingsstore.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QtMath>

namespace {
QString decodeCookie(const QString &stored)
{
    if (stored.isEmpty()) {
        return {};
    }
    return QString::fromUtf8(QByteArray::fromBase64(stored.toUtf8()));
}
} // namespace

SettingsStore::SettingsStore(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope,
                 QStringLiteral("deckade"), QStringLiteral("deckade"))
{
    m_installMethod = m_settings.value(QStringLiteral("install/method"), QStringLiteral("auto")).toString();
    m_fightcadePath = m_settings.value(QStringLiteral("install/path")).toString();
    m_romPath = m_settings.value(QStringLiteral("install/romPath")).toString();
    m_showControllerHints = m_settings.value(QStringLiteral("ui/controllerHints"), true).toBool();
    m_autoJoinLastChannels = m_settings.value(QStringLiteral("ui/autoJoinLastChannels"), true).toBool();
    m_reduceMotion = m_settings.value(QStringLiteral("ui/reduceMotion"), false).toBool();
    m_fullscreen = m_settings.value(QStringLiteral("ui/fullscreen"), true).toBool();
    m_autoDownloadRoms = m_settings.value(QStringLiteral("roms/autoDownload"), true).toBool();
    m_rememberLogin = m_settings.value(QStringLiteral("session/remember"), true).toBool();
    m_uiScale = qBound<qreal>(0.8, m_settings.value(QStringLiteral("ui/scale"), 1.0).toDouble(), 1.0);
    m_controllerIndex = qMax(0, m_settings.value(QStringLiteral("input/controllerIndex"), 0).toInt());
    m_lastChannels = m_settings.value(QStringLiteral("lobby/lastChannels")).toStringList();
    m_sessionCookie = decodeCookie(m_settings.value(QStringLiteral("session/cookie")).toString());
    m_sessionName = m_settings.value(QStringLiteral("session/name")).toString();
    m_sessionDisplayName = m_settings.value(QStringLiteral("session/displayName")).toString();
    protectConfigFile();
}

QString SettingsStore::installMethod() const { return m_installMethod; }
QString SettingsStore::fightcadePath() const { return m_fightcadePath; }
QString SettingsStore::romPath() const { return m_romPath; }
bool SettingsStore::showControllerHints() const { return m_showControllerHints; }
bool SettingsStore::autoJoinLastChannels() const { return m_autoJoinLastChannels; }
bool SettingsStore::reduceMotion() const { return m_reduceMotion; }
bool SettingsStore::fullscreen() const { return m_fullscreen; }
bool SettingsStore::autoDownloadRoms() const { return m_autoDownloadRoms; }
bool SettingsStore::rememberLogin() const { return m_rememberLogin; }
qreal SettingsStore::uiScale() const { return m_uiScale; }
int SettingsStore::controllerIndex() const { return m_controllerIndex; }
QStringList SettingsStore::lastChannels() const { return m_lastChannels; }
bool SettingsStore::hasSession() const { return !m_sessionCookie.isEmpty() && !m_sessionName.isEmpty(); }
QString SettingsStore::sessionName() const { return m_sessionName; }
QString SettingsStore::sessionDisplayName() const { return m_sessionDisplayName; }
QString SettingsStore::sessionCookie() const { return m_sessionCookie; }

void SettingsStore::setInstallMethod(const QString &value)
{
    const QString normalized = value == QStringLiteral("native") || value == QStringLiteral("flatpak")
        ? value
        : QStringLiteral("auto");
    if (m_installMethod == normalized) {
        return;
    }
    m_installMethod = normalized;
    persist(QStringLiteral("install/method"), m_installMethod);
    emit installMethodChanged();
}

void SettingsStore::setFightcadePath(const QString &value)
{
    const QString normalized = QDir::cleanPath(value.trimmed());
    if (m_fightcadePath == normalized) {
        return;
    }
    m_fightcadePath = normalized;
    persist(QStringLiteral("install/path"), m_fightcadePath);
    emit fightcadePathChanged();
}

void SettingsStore::setRomPath(const QString &value)
{
    const QString normalized = QDir::cleanPath(value.trimmed());
    if (m_romPath == normalized) {
        return;
    }
    m_romPath = normalized;
    persist(QStringLiteral("install/romPath"), m_romPath);
    emit romPathChanged();
}

void SettingsStore::setShowControllerHints(bool value)
{
    if (m_showControllerHints == value) {
        return;
    }
    m_showControllerHints = value;
    persist(QStringLiteral("ui/controllerHints"), value);
    emit showControllerHintsChanged();
}

void SettingsStore::setAutoJoinLastChannels(bool value)
{
    if (m_autoJoinLastChannels == value) {
        return;
    }
    m_autoJoinLastChannels = value;
    persist(QStringLiteral("ui/autoJoinLastChannels"), value);
    emit autoJoinLastChannelsChanged();
}

void SettingsStore::setReduceMotion(bool value)
{
    if (m_reduceMotion == value) {
        return;
    }
    m_reduceMotion = value;
    persist(QStringLiteral("ui/reduceMotion"), value);
    emit reduceMotionChanged();
}

void SettingsStore::setFullscreen(bool value)
{
    if (m_fullscreen == value) {
        return;
    }
    m_fullscreen = value;
    persist(QStringLiteral("ui/fullscreen"), value);
    emit fullscreenChanged();
}

void SettingsStore::setAutoDownloadRoms(bool value)
{
    if (m_autoDownloadRoms == value) {
        return;
    }
    m_autoDownloadRoms = value;
    persist(QStringLiteral("roms/autoDownload"), value);
    emit autoDownloadRomsChanged();
}

void SettingsStore::setRememberLogin(bool value)
{
    if (m_rememberLogin == value) {
        return;
    }
    m_rememberLogin = value;
    persist(QStringLiteral("session/remember"), value);
    emit rememberLoginChanged();
}

void SettingsStore::setUiScale(qreal value)
{
    const qreal normalized = qBound<qreal>(0.8, value, 1.0);
    if (qFuzzyCompare(m_uiScale, normalized)) {
        return;
    }
    m_uiScale = normalized;
    persist(QStringLiteral("ui/scale"), normalized);
    emit uiScaleChanged();
}

void SettingsStore::setControllerIndex(int value)
{
    const int normalized = qMax(0, value);
    if (m_controllerIndex == normalized) {
        return;
    }
    m_controllerIndex = normalized;
    persist(QStringLiteral("input/controllerIndex"), normalized);
    emit controllerIndexChanged();
}

void SettingsStore::setLastChannels(const QStringList &value)
{
    if (m_lastChannels == value) {
        return;
    }
    m_lastChannels = value;
    persist(QStringLiteral("lobby/lastChannels"), value);
    emit lastChannelsChanged();
}

void SettingsStore::saveSession(const QString &cookie, const QString &name, const QString &displayName)
{
    if (cookie.isEmpty() || name.isEmpty()) {
        return;
    }
    m_sessionCookie = cookie;
    m_sessionName = name;
    m_sessionDisplayName = displayName.isEmpty() ? name : displayName;
    m_settings.setValue(QStringLiteral("session/cookie"), QString::fromUtf8(cookie.toUtf8().toBase64()));
    m_settings.setValue(QStringLiteral("session/name"), m_sessionName);
    m_settings.setValue(QStringLiteral("session/displayName"), m_sessionDisplayName);
    m_settings.sync();
    protectConfigFile();
    emit sessionChanged();
}

void SettingsStore::clearSession()
{
    m_sessionCookie.clear();
    m_sessionName.clear();
    m_sessionDisplayName.clear();
    m_settings.remove(QStringLiteral("session/cookie"));
    m_settings.remove(QStringLiteral("session/name"));
    m_settings.remove(QStringLiteral("session/displayName"));
    m_settings.sync();
    emit sessionChanged();
}

void SettingsStore::rememberChannel(const QString &channelName)
{
    if (channelName.isEmpty()) {
        return;
    }
    QStringList channels = m_lastChannels;
    channels.removeAll(channelName);
    channels.prepend(channelName);
    while (channels.size() > 8) {
        channels.removeLast();
    }
    setLastChannels(channels);
}

void SettingsStore::protectConfigFile()
{
    const QString path = m_settings.fileName();
    if (path.isEmpty() || !QFile::exists(path)) {
        return;
    }
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
}

void SettingsStore::persist(const QString &key, const QVariant &value)
{
    m_settings.setValue(key, value);
    m_settings.sync();
}
