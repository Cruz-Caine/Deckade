#include "userstatus.h"

#include <QCryptographicHash>
#include <QSysInfo>

namespace FightcadeUserStatus {

QString md5Hex(const QString &value)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Md5).toHex());
}

QString machineSeed()
{
    QByteArray seed = QSysInfo::machineUniqueId();
    if (seed.isEmpty()) {
        seed = QSysInfo::machineHostName().toUtf8();
    }
    if (seed.isEmpty()) {
        seed = QByteArrayLiteral("deckade");
    }
    return QString::fromUtf8(seed);
}

QString uuidFromSeed(const QString &seed)
{
    const QString hex = md5Hex(QStringLiteral("deckade-uuid:") + seed);
    return QStringLiteral("%1-%2-%3-%4-%5")
        .arg(hex.mid(0, 8), hex.mid(8, 4), hex.mid(12, 4), hex.mid(16, 4), hex.mid(20, 12));
}

QString currentUuid()
{
    return uuidFromSeed(machineSeed());
}

QString currentGuid()
{
    return currentUuid();
}

QString currentHuid()
{
    return QStringLiteral("0030613289.684") + md5Hex(machineSeed());
}

QString currentStatus()
{
    // Deckade does not distinguish Wi-Fi from Ethernet yet. Reporting a wired
    // connection keeps the optional Wi-Fi challenge filter from rejecting us.
    return QStringLiteral("stcable");
}

QString hash(const QString &token, const QString &uuid, const QString &guid,
             const QString &userStatus, const QString &huid, int version)
{
    const QString input = QStringLiteral("3jedoQ") + token
        + QStringLiteral("qmkq0") + uuid
        + QStringLiteral("dsnds") + guid
        + QStringLiteral("sec or") + QString::number(version)
        + QStringLiteral("2jden3") + userStatus
        + QStringLiteral("llNjjha") + huid;
    return md5Hex(input);
}

QJsonObject payload(const QString &token)
{
    const QString uuid = currentUuid();
    const QString guid = currentGuid();
    const QString huid = currentHuid();
    const QString status = currentStatus();
    constexpr int version = 6;
    return QJsonObject{
        {"req", "userstatus"},
        {"token", token},
        {"userstatus", status},
        {"uuid", uuid},
        {"guid", guid},
        {"huid", huid},
        {"version", version},
        {"hash", hash(token, uuid, guid, status, huid, version)}
    };
}

} // namespace FightcadeUserStatus
