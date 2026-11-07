#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantList>
#include <QVariantMap>

class MatchHistory : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList matches READ matches NOTIFY changed)

public:
    explicit MatchHistory(QObject *parent = nullptr);
    QVariantList matches() const { return m_matches; }

    Q_INVOKABLE void recordMatch(const QVariantMap &match);
    Q_INVOKABLE void clear();

signals:
    void changed();

private:
    void load();
    void save();

    QSettings m_settings;
    QVariantList m_matches;
};
