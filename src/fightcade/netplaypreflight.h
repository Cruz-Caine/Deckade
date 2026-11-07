#pragma once
#include <QString>
#include <QStringList>

namespace NetplayPreflight {
QStringList warnings(const QString &masterHost, quint16 masterPort, int proxyPort);
}
