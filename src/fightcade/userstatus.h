#pragma once

#include <QJsonObject>
#include <QString>

namespace FightcadeUserStatus {

QString md5Hex(const QString &value);
QString machineSeed();
QString uuidFromSeed(const QString &seed);
QString currentUuid();
QString currentGuid();
QString currentHuid();
QString currentStatus();
QString hash(const QString &token, const QString &uuid, const QString &guid,
             const QString &userStatus, const QString &huid, int version = 6);
QJsonObject payload(const QString &token);

} // namespace FightcadeUserStatus
