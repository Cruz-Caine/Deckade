#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class SettingsStore;
class NetplayProxy;

class QNetworkAccessManager;

class FightcadeLauncher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString romPath READ romPath NOTIFY installChanged)
    Q_PROPERTY(QString runtimeRoot READ runtimeRoot NOTIFY installChanged)
    Q_PROPERTY(QVariantList runtimeSummary READ runtimeSummary NOTIFY installChanged)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(bool isSteamDeck READ isSteamDeck CONSTANT)

public:
    explicit FightcadeLauncher(SettingsStore *settings, QObject *parent = nullptr);

    QString romPath() const { return m_romPath; }
    QString runtimeRoot() const { return m_runtimeRoot; }
    QVariantList runtimeSummary() const { return m_runtimeSummary; }
    bool running() const { return m_running; }
    QString lastError() const { return m_lastError; }
    bool isSteamDeck() const;

    Q_INVOKABLE void refreshInstall();
    Q_INVOKABLE bool launchMatch(const QVariantMap &match);
    Q_INVOKABLE bool launchSpectate(const QVariantMap &match);
    Q_INVOKABLE bool launchSolo(const QString &emulator, const QString &gameId);
    Q_INVOKABLE bool launchTraining(const QString &emulator, const QString &gameId);
    Q_INVOKABLE void killEmulator();
    Q_INVOKABLE void clearRunning();
    Q_INVOKABLE void openRomFolder();
    Q_INVOKABLE void reportUserStatus(const QString &token);
    Q_INVOKABLE bool hasLocalRom(const QString &emulator, const QString &gameId) const;
    // False when Deckade has no way to launch this emulator's games. A channel
    // that does not name an emulator is not treated as unsupported.
    Q_INVOKABLE bool isEmulatorSupported(const QString &emulator) const;
    Q_INVOKABLE QString romDirectoryFor(const QString &emulator) const;
    Q_INVOKABLE QString runtimeBinaryFor(const QString &emulator) const;

signals:
    void installChanged();
    void stateChanged();
    void errorOccurred(const QString &message);
    void notice(const QString &message);

private:
    QString normalizedEmulator(const QString &emulator) const;
    QString runtimeExecutable(const QString &emulator) const;
    QString runtimeName(const QString &emulator) const;
    bool launchRuntime(const QString &emulator, const QStringList &arguments,
                       const QString &gameId, bool singlePlayer,
                       const QVariantMap &extraEnvironment = {});
    bool prepareWorkDirectory(const QString &emulator, const QString &gameId);
    QString findRomFile(const QString &gameId, const QString &romRoot) const;
    QString romPathFor(const QString &emulator) const;
    bool flycastCanStart(const QString &gameId);
    bool startFlycast(const QStringList &arguments);
    bool launchFlycastMatch(const QVariantMap &match);
    void reportFlycastFinished();
    void setRunning(bool running);
    void setError(const QString &message);
    void inspectInstall();
    void inspectRuntimes();

    SettingsStore *m_settings = nullptr;
    QString m_romPath;
    QString m_runtimeRoot;
    QVariantList m_runtimeSummary;
    QProcess *m_process = nullptr;
    bool m_running = false;
    QString m_lastError;
    NetplayProxy *m_netplayProxy = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QString m_flycastQuarkId;
    QString m_flycastToken;
};
