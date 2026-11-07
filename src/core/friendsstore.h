#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>

class FriendsStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList friends READ friends NOTIFY changed)

public:
    explicit FriendsStore(QObject *parent = nullptr);
    QStringList friends() const { return m_friends; }
    Q_INVOKABLE bool isFriend(const QString &name) const;
    Q_INVOKABLE void addFriend(const QString &name);
    Q_INVOKABLE void removeFriend(const QString &name);
    Q_INVOKABLE void toggleFriend(const QString &name);

signals:
    void changed();

private:
    void save();
    QSettings m_settings;
    QStringList m_friends;
};
