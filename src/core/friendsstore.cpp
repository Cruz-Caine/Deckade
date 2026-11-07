#include "friendsstore.h"

FriendsStore::FriendsStore(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope,
                 QStringLiteral("deckade"), QStringLiteral("deckade"))
{
    m_friends = m_settings.value(QStringLiteral("social/friends")).toStringList();
    m_friends.removeDuplicates();
    m_friends.sort(Qt::CaseInsensitive);
}

bool FriendsStore::isFriend(const QString &name) const
{
    return m_friends.contains(name, Qt::CaseInsensitive);
}

void FriendsStore::addFriend(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || isFriend(trimmed)) return;
    m_friends.append(trimmed);
    m_friends.sort(Qt::CaseInsensitive);
    save();
}

void FriendsStore::removeFriend(const QString &name)
{
    const int index = m_friends.indexOf(name);
    if (index < 0) return;
    m_friends.removeAt(index);
    save();
}

void FriendsStore::toggleFriend(const QString &name)
{
    if (isFriend(name)) removeFriend(name); else addFriend(name);
}

void FriendsStore::save()
{
    m_settings.setValue(QStringLiteral("social/friends"), m_friends);
    m_settings.sync();
    emit changed();
}
