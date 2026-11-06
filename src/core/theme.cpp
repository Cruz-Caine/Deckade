#include "theme.h"

#include <QFontDatabase>

Theme::Theme(QObject *parent)
    : QObject(parent)
{
    const QStringList displayFonts = {
        QStringLiteral(":/assets/fonts/Rajdhani-Bold.ttf"),
        QStringLiteral(":/assets/fonts/Rajdhani-SemiBold.ttf"),
        QStringLiteral(":/assets/fonts/Rajdhani-Medium.ttf")
    };
    for (const QString &font : displayFonts) {
        QFontDatabase::addApplicationFont(font);
    }

    QFontDatabase::addApplicationFont(QStringLiteral(":/assets/fonts/inter/Inter.ttf"));

    const QStringList families = QFontDatabase::families();
    for (const QString &family : families) {
        if (family.contains(QStringLiteral("Rajdhani"), Qt::CaseInsensitive)) {
            m_displayFamily = family;
        }
        if (family.contains(QStringLiteral("Inter"), Qt::CaseInsensitive)) {
            m_bodyFamily = family;
        }
    }
}
