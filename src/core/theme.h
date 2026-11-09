#pragma once

#include <QColor>
#include <QObject>
#include <QString>

class Theme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor background READ background CONSTANT)
    Q_PROPERTY(QColor backgroundAlt READ backgroundAlt CONSTANT)
    Q_PROPERTY(QColor panel READ panel CONSTANT)
    Q_PROPERTY(QColor panelRaised READ panelRaised CONSTANT)
    Q_PROPERTY(QColor panelHover READ panelHover CONSTANT)
    Q_PROPERTY(QColor border READ border CONSTANT)
    Q_PROPERTY(QColor borderStrong READ borderStrong CONSTANT)
    Q_PROPERTY(QColor text READ text CONSTANT)
    Q_PROPERTY(QColor textMuted READ textMuted CONSTANT)
    Q_PROPERTY(QColor textFaint READ textFaint CONSTANT)
    Q_PROPERTY(QColor accent READ accent CONSTANT)
    Q_PROPERTY(QColor accentSoft READ accentSoft CONSTANT)
    Q_PROPERTY(QColor cyan READ cyan CONSTANT)
    Q_PROPERTY(QColor gold READ gold CONSTANT)
    Q_PROPERTY(QColor green READ green CONSTANT)
    Q_PROPERTY(QColor red READ red CONSTANT)
    Q_PROPERTY(QString displayFamily READ displayFamily CONSTANT)
    Q_PROPERTY(QString bodyFamily READ bodyFamily CONSTANT)

public:
    explicit Theme(QObject *parent = nullptr);

    QColor background() const { return QColor(QStringLiteral("#12151c")); }
    QColor backgroundAlt() const { return QColor(QStringLiteral("#161a22")); }
    QColor panel() const { return QColor(QStringLiteral("#1b1f29")); }
    QColor panelRaised() const { return QColor(QStringLiteral("#242935")); }
    QColor panelHover() const { return QColor(QStringLiteral("#2c3241")); }
    QColor border() const { return QColor(QStringLiteral("#2f3644")); }
    QColor borderStrong() const { return QColor(QStringLiteral("#465063")); }
    QColor text() const { return QColor(QStringLiteral("#eceef3")); }
    QColor textMuted() const { return QColor(QStringLiteral("#a4acba")); }
    QColor textFaint() const { return QColor(QStringLiteral("#737c8c")); }
    QColor accent() const { return QColor(QStringLiteral("#4f8cff")); }
    QColor accentSoft() const { return QColor(QStringLiteral("#7aa8ff")); }
    QColor cyan() const { return QColor(QStringLiteral("#6fb7ff")); }
    QColor gold() const { return QColor(QStringLiteral("#f0c063")); }
    QColor green() const { return QColor(QStringLiteral("#5fd39a")); }
    QColor red() const { return QColor(QStringLiteral("#f0697c")); }
    QString displayFamily() const { return m_displayFamily; }
    QString bodyFamily() const { return m_bodyFamily; }

private:
    QString m_displayFamily = QStringLiteral("DejaVu Sans");
    QString m_bodyFamily = QStringLiteral("DejaVu Sans");
};
