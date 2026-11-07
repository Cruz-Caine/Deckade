#pragma once
#include <QList>
#include <QString>
#include <QStringList>

struct PortMappingResult {
    int port = 0;
    QString protocol;
    bool mapped = false;
    QString message;
};

namespace PortMapping {
QList<PortMappingResult> mapUdp(const QList<int> &ports, int lifetime = 3600);
QString defaultGateway();
QString localIPv4(const QString &gateway = {});
}
