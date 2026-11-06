#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QStringList>

class FightcadeLauncher;
class SettingsStore;
class QFile;
class QNetworkReply;

class RomLibrary : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString activeEmulator READ activeEmulator NOTIFY stateChanged)
    Q_PROPERTY(QString activeGameId READ activeGameId NOTIFY stateChanged)

public:
    explicit RomLibrary(FightcadeLauncher *launcher, SettingsStore *settings,
                        QObject *parent = nullptr);
    ~RomLibrary() override;

    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    qreal progress() const { return m_progress; }
    QString activeEmulator() const { return m_emulator; }
    QString activeGameId() const { return m_gameId; }

    Q_INVOKABLE bool hasRom(const QString &emulator, const QString &gameId) const;
    Q_INVOKABLE void ensureRom(const QString &emulator, const QString &gameId);
    Q_INVOKABLE void cancel();

signals:
    void stateChanged();
    void progressChanged();
    void romReady(const QString &emulator, const QString &gameId, const QString &path);
    void romFailed(const QString &emulator, const QString &gameId, const QString &message);
    void notice(const QString &message);

private:
    struct Entry {
        QString jsonFile;
        QString key;
        QString url;
        QString copyTo;
        QStringList requirements;
    };

    void loadEntriesFor(const QString &emulator);
    void loadJson(const QString &fileName);
    QString findMainKey(const QString &emulator, const QString &gameId) const;
    QString destinationFor(const Entry &entry) const;
    QString flycastDataDirectory() const;
    QString subfolderFor(const QString &jsonFile) const;
    QString fileNameForUrl(const QString &url) const;
    bool collect(const QString &key);
    void startNext();
    void download(const Entry &entry);
    void finishReady();
    void fail(const QString &message);
    void reset();
    void setStatus(const QString &status);
    void setProgress(qreal progress);

    FightcadeLauncher *m_launcher = nullptr;
    SettingsStore *m_settings = nullptr;
    QNetworkAccessManager m_network;
    QHash<QString, Entry> m_entries;
    QSet<QString> m_loadedFiles;
    QSet<QString> m_collected;
    QQueue<QString> m_queue;
    QString m_emulator;
    QString m_gameId;
    QString m_mainKey;
    QString m_mainDestination;
    QString m_romRoot;
    QString m_status;
    qreal m_progress = 0;
    bool m_busy = false;
    bool m_cancelled = false;
    QNetworkReply *m_reply = nullptr;
    QFile *m_output = nullptr;
    QString m_partPath;
};
