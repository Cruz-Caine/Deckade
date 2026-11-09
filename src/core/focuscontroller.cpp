#include "focuscontroller.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QtMath>
#include <limits>

FocusController::FocusController(QObject *parent)
    : QObject(parent)
{
}

void FocusController::setModalActive(bool value)
{
    if (m_modalActive == value) {
        return;
    }
    m_modalActive = value;
    emit modalActiveChanged();
}

void FocusController::setActiveGroup(const QString &value)
{
    const QString normalized = value.isEmpty() ? QStringLiteral("main") : value;
    if (m_activeGroup == normalized) {
        return;
    }
    m_activeGroup = normalized;
    emit activeGroupChanged();
}

void FocusController::registerItem(QQuickItem *item)
{
    if (!item) {
        return;
    }
    for (const QPointer<QQuickItem> &existing : std::as_const(m_items)) {
        if (existing == item) {
            return;
        }
    }
    m_items.append(item);
    if (!currentItem()) {
        m_lastFocused = item;
    }
}

void FocusController::unregisterItem(QQuickItem *item)
{
    for (int index = m_items.size() - 1; index >= 0; --index) {
        if (m_items.at(index).isNull() || m_items.at(index) == item) {
            m_items.removeAt(index);
        }
    }
    if (m_lastFocused == item) {
        m_lastFocused.clear();
    }
}

void FocusController::refresh()
{
    for (int index = m_items.size() - 1; index >= 0; --index) {
        if (m_items.at(index).isNull()) {
            m_items.removeAt(index);
        }
    }
}

void FocusController::focusFirst()
{
    const QList<Candidate> items = candidates();
    if (items.isEmpty()) {
        return;
    }

    Candidate best = items.first();
    for (const Candidate &candidate : items) {
        if (candidate.rect.top() < best.rect.top() - 1.0
            || (qAbs(candidate.rect.top() - best.rect.top()) <= 1.0
                && candidate.rect.left() < best.rect.left())) {
            best = candidate;
        }
    }
    focusItem(best.item);
}

void FocusController::focusItem(QQuickItem *item)
{
    if (!item || !isEligible(item)) {
        return;
    }
    item->forceActiveFocus(Qt::OtherFocusReason);
    m_lastFocused = item;
    emit focusMoved(item);
}

void FocusController::move(int dx, int dy)
{
    refresh();
    if (dx == 0 && dy == 0) {
        return;
    }

    const QList<Candidate> items = candidates();
    if (items.isEmpty()) {
        return;
    }

    QQuickItem *current = currentItem();
    const Candidate *origin = nullptr;
    for (const Candidate &candidate : items) {
        if (candidate.item == current) {
            origin = &candidate;
            break;
        }
    }

    if (!origin) {
        focusFirst();
        return;
    }

    const Candidate *best = nullptr;
    qreal bestScore = std::numeric_limits<qreal>::max();

    for (const Candidate &candidate : items) {
        if (candidate.item == origin->item) {
            continue;
        }

        const qreal deltaX = candidate.center.x() - origin->center.x();
        const qreal deltaY = candidate.center.y() - origin->center.y();

        if (dx > 0 && deltaX <= 1.0) continue;
        if (dx < 0 && deltaX >= -1.0) continue;
        if (dy > 0 && deltaY <= 1.0) continue;
        if (dy < 0 && deltaY >= -1.0) continue;

        const qreal primary = dx != 0 ? qAbs(deltaX) : qAbs(deltaY);
        const qreal secondary = dx != 0 ? qAbs(deltaY) : qAbs(deltaX);

        qreal overlap = 0.0;
        if (dx != 0) {
            const qreal top = qMax(candidate.rect.top(), origin->rect.top());
            const qreal bottom = qMin(candidate.rect.bottom(), origin->rect.bottom());
            overlap = qMax<qreal>(0.0, bottom - top);
        } else {
            const qreal left = qMax(candidate.rect.left(), origin->rect.left());
            const qreal right = qMin(candidate.rect.right(), origin->rect.right());
            overlap = qMax<qreal>(0.0, right - left);
        }

        qreal score = primary + secondary * (overlap > 0.0 ? 0.35 : 2.2);
        if (overlap > 0.0) {
            score -= qMin<qreal>(overlap, 120.0) * 0.35;
        }

        if (score < bestScore) {
            bestScore = score;
            best = &candidate;
        }
    }

    if (best) {
        focusItem(best->item);
    }
}

void FocusController::activate()
{
    QQuickItem *item = currentItem();
    if (!item) {
        focusFirst();
        item = currentItem();
    }
    if (!item || !isRegistered(item)) {
        return;
    }
    if (!QMetaObject::invokeMethod(item, "activated")) {
        focusFirst();
        item = currentItem();
        if (item) QMetaObject::invokeMethod(item, "activated");
    }
}

void FocusController::secondaryActivate()
{
    QQuickItem *item = currentItem();
    if (!item || !isRegistered(item)) {
        return;
    }
    QMetaObject::invokeMethod(item, "secondaryActivated");
}

void FocusController::tertiaryActivate()
{
    QQuickItem *item = currentItem();
    if (!item || !isRegistered(item)) {
        return;
    }
    QMetaObject::invokeMethod(item, "tertiaryActivated");
}

QList<FocusController::Candidate> FocusController::candidates() const
{
    QList<Candidate> result;
    for (const QPointer<QQuickItem> &pointer : m_items) {
        QQuickItem *item = pointer.data();
        if (!isEligible(item)) {
            continue;
        }
        const QRectF rect = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
        if (rect.width() <= 1.0 || rect.height() <= 1.0) {
            continue;
        }
        result.append({item, rect, rect.center()});
    }
    return result;
}

QQuickItem *FocusController::currentItem() const
{
    if (auto *window = qobject_cast<QQuickWindow *>(QGuiApplication::focusWindow())) {
        if (QQuickItem *item = window->activeFocusItem()) {
            if (isEligible(item)) {
                return item;
            }
        }
    }
    if (QQuickItem *last = m_lastFocused.data()) {
        if (isEligible(last)) {
            return last;
        }
    }
    return nullptr;
}

bool FocusController::isRegistered(QQuickItem *item) const
{
    for (const QPointer<QQuickItem> &registered : m_items) {
        if (registered == item) return true;
    }
    return false;
}

bool FocusController::isEligible(QQuickItem *item) const
{
    if (!item || !isRegistered(item)
        || !item->isVisible() || !isEffectivelyVisible(item) || !isEffectivelyEnabled(item)) {
        return false;
    }
    if (item->width() <= 1.0 || item->height() <= 1.0) {
        return false;
    }
    if (!item->property("navEnabled").isValid() || item->property("navEnabled").toBool()) {
        // Eligible unless explicitly disabled.
    } else {
        return false;
    }

    const QString group = item->property("navGroup").toString();
    if (!m_activeGroup.isEmpty() && !group.isEmpty() && group != m_activeGroup) {
        return false;
    }
    return true;
}

bool FocusController::isEffectivelyVisible(QQuickItem *item)
{
    for (QQuickItem *current = item; current; current = current->parentItem()) {
        if (!current->isVisible() || current->opacity() <= 0.01) {
            return false;
        }
    }
    return true;
}

bool FocusController::isEffectivelyEnabled(QQuickItem *item)
{
    for (QQuickItem *current = item; current; current = current->parentItem()) {
        if (!current->isEnabled()) {
            return false;
        }
    }
    return true;
}
