#include "theme.h"

#include <QFontDatabase>

Theme::Theme(QObject *parent)
    : QObject(parent)
{
    QFontDatabase::addApplicationFont(QStringLiteral(":/assets/fonts/inter/Inter.ttf"));

    for (const QString &family : QFontDatabase::families()) {
        if (family.contains(QStringLiteral("Inter"), Qt::CaseInsensitive)) {
            m_bodyFamily = family;
            m_displayFamily = family;
        }
    }
}
