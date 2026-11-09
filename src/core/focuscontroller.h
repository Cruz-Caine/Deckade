#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QString>

class FocusController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool modalActive READ modalActive WRITE setModalActive NOTIFY modalActiveChanged)
    Q_PROPERTY(QString activeGroup READ activeGroup WRITE setActiveGroup NOTIFY activeGroupChanged)

public:
    explicit FocusController(QObject *parent = nullptr);

    bool modalActive() const { return m_modalActive; }
    void setModalActive(bool value);
    QString activeGroup() const { return m_activeGroup; }
    void setActiveGroup(const QString &value);

    Q_INVOKABLE void registerItem(QQuickItem *item);
    Q_INVOKABLE void unregisterItem(QQuickItem *item);
    Q_INVOKABLE void move(int dx, int dy);
    Q_INVOKABLE void focusFirst();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void activate();
    Q_INVOKABLE void secondaryActivate();
    Q_INVOKABLE void tertiaryActivate();
    Q_INVOKABLE void focusItem(QQuickItem *item);

signals:
    void modalActiveChanged();
    void activeGroupChanged();
    void focusMoved(QQuickItem *item);

private:
    struct Candidate {
        QQuickItem *item = nullptr;
        QRectF rect;
        QPointF center;
    };

    QList<Candidate> candidates() const;
    QQuickItem *currentItem() const;
    bool isEligible(QQuickItem *item) const;
    bool isRegistered(QQuickItem *item) const;
    static bool isEffectivelyVisible(QQuickItem *item);
    static bool isEffectivelyEnabled(QQuickItem *item);

    QList<QPointer<QQuickItem>> m_items;
    QPointer<QQuickItem> m_lastFocused;
    bool m_modalActive = false;
    QString m_activeGroup = QStringLiteral("main");
};
