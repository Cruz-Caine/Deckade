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

    QColor background() const { return QColor(QStringLiteral("#090b11")); }
    QColor backgroundAlt() const { return QColor(QStringLiteral("#0e111a")); }
    QColor panel() const { return QColor(QStringLiteral("#141925")); }
    QColor panelRaised() const { return QColor(QStringLiteral("#1b2231")); }
    QColor panelHover() const { return QColor(QStringLiteral("#232c3f")); }
    QColor border() const { return QColor(QStringLiteral("#283249")); }
    QColor borderStrong() const { return QColor(QStringLiteral("#3c4a68")); }
    QColor text() const { return QColor(QStringLiteral("#f4f6fb")); }
    QColor textMuted() const { return QColor(QStringLiteral("#a6b0c3")); }
    QColor textFaint() const { return QColor(QStringLiteral("#6b768c")); }
    QColor accent() const { return QColor(QStringLiteral("#ff4655")); }
    QColor accentSoft() const { return QColor(QStringLiteral("#ff7a84")); }
    QColor cyan() const { return QColor(QStringLiteral("#43d9ff")); }
    QColor gold() const { return QColor(QStringLiteral("#ffc857")); }
    QColor green() const { return QColor(QStringLiteral("#55e6a5")); }
    QColor red() const { return QColor(QStringLiteral("#ff5470")); }
    QString displayFamily() const { return m_displayFamily; }
    QString bodyFamily() const { return m_bodyFamily; }

private:
    QString m_displayFamily = QStringLiteral("DejaVu Sans");
    QString m_bodyFamily = QStringLiteral("DejaVu Sans");
};
