#include "matchhistory.h"

#include <algorithm>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

MatchHistory::MatchHistory(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope,
                 QStringLiteral("deckade"), QStringLiteral("deckade"))
{
    load();
}

void MatchHistory::load()
{
    m_matches.clear();
    const QByteArray raw = m_settings.value(QStringLiteral("history/matches")).toByteArray();
    const QJsonArray array = QJsonDocument::fromJson(raw).array();
    for (const QJsonValue &value : array) {
        const QVariantMap item = value.toObject().toVariantMap();
        if (!item.value(QStringLiteral("channelName")).toString().isEmpty()) {
            m_matches.append(item);
        }
    }
}

void MatchHistory::save()
{
    QJsonArray array;
    for (const QVariant &value : std::as_const(m_matches)) {
        array.append(QJsonObject::fromVariantMap(value.toMap()));
    }
    m_settings.setValue(QStringLiteral("history/matches"),
                        QJsonDocument(array).toJson(QJsonDocument::Compact));
    m_settings.sync();
    emit changed();
}

void MatchHistory::recordMatch(const QVariantMap &match)
{
    const QString channelName = match.value(QStringLiteral("channelName")).toString();
    if (channelName.isEmpty()) return;
    QVariantMap entry{
        {"channelName", channelName},
        {"gameId", match.value(QStringLiteral("gameId")).toString()},
        {"opponent", match.value(QStringLiteral("opponent")).toString()},
        {"ranked", match.value(QStringLiteral("ranked")).toInt()},
        {"delay", match.value(QStringLiteral("delay")).toInt()},
        {"timestamp", QDateTime::currentMSecsSinceEpoch()}
    };
    // Keep the most recent occurrence of the same opponent/channel pair.
    m_matches.erase(std::remove_if(m_matches.begin(), m_matches.end(),
        [&](const QVariant &value) {
            const QVariantMap old = value.toMap();
            return old.value(QStringLiteral("channelName")).toString() == channelName
                && old.value(QStringLiteral("opponent")).toString()
                    == entry.value(QStringLiteral("opponent")).toString();
        }), m_matches.end());
    m_matches.prepend(entry);
    while (m_matches.size() > 20) m_matches.removeLast();
    save();
}

void MatchHistory::clear()
{
    m_matches.clear();
    save();
}
