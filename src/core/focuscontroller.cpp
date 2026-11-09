#include "focuscontroller.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QtMath>
#include <limits>

namespace {
// A Flickable (and therefore a ListView/GridView) exposes contentY and
// contentItem. Deckade's scrolling views all clip to their own bounds.
bool isScrollContainer(QQuickItem *item)
{
    return item && item->property("contentY").isValid()
        && item->property("contentItem").isValid();
}

// Returns the item's nearest scrolling ancestor when the item has been
// scrolled outside that ancestor's viewport, otherwise nullptr.
QQuickItem *scrolledOutContainer(QQuickItem *item, QRectF *viewport)
{
    for (QQuickItem *ancestor = item->parentItem(); ancestor; ancestor = ancestor->parentItem()) {
        if (!isScrollContainer(ancestor)) {
            continue;
        }
        const QRectF local = item->mapRectToItem(ancestor,
            QRectF(0, 0, item->width(), item->height()));
        const QRectF visible(0, 0, ancestor->width(), ancestor->height());
        if (!visible.intersects(local)) {
            if (viewport) {
                *viewport = visible;
            }
            return ancestor;
        }
        // Only the nearest scroller decides whether the row is on screen.
        break;
    }
    return nullptr;
}
} // namespace

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

    // When the focused row has been scrolled out of its list, navigating from
    // its hidden position would drag the list back to it. Up and Down start
    // from the viewport edge instead, so they land on a row the player can
    // see. Left and Right still leave the list from the row's own column.
    QQuickItem *scroller = nullptr;
    QRectF viewport;
    QPointF originCenter = origin->center;
    QRectF originRect = origin->rect;
    bool restrictToViewport = false;
    if (QQuickItem *container = scrolledOutContainer(origin->item, &viewport)) {
        const QRectF sceneViewport = container->mapRectToScene(viewport);
        if (dy != 0) {
            scroller = container;
            restrictToViewport = true;
            originCenter.setY(dy > 0 ? sceneViewport.top() : sceneViewport.bottom());
        } else {
            originCenter.setY(sceneViewport.center().y());
            originRect.setTop(sceneViewport.top());
            originRect.setBottom(sceneViewport.bottom());
        }
    }

    auto insideViewport = [&](const Candidate &candidate) {
        if (!scroller) {
            return true;
        }
        const QRectF local = candidate.item->mapRectToItem(scroller,
            QRectF(0, 0, candidate.item->width(), candidate.item->height()));
        return QRectF(0, 0, scroller->width(), scroller->height()).intersects(local);
    };

    // Horizontal moves stay in the row band the player is looking at;
    // vertical moves stay in the same column band. Only when nothing shares
    // that band do we consider diagonal candidates. Without this, a wide
    // window made Right from the featured banner jump to a card below it
    // instead of the panel beside it.
    auto pick = [&](bool requireBandOverlap) -> QQuickItem * {
        const Candidate *best = nullptr;
        qreal bestScore = std::numeric_limits<qreal>::max();

        for (const Candidate &candidate : items) {
            if (candidate.item == origin->item) {
                continue;
            }
            if (restrictToViewport && !insideViewport(candidate)) {
                continue;
            }

            const qreal deltaX = candidate.center.x() - originCenter.x();
            const qreal deltaY = candidate.center.y() - originCenter.y();

            if (dx > 0 && deltaX <= 1.0) continue;
            if (dx < 0 && deltaX >= -1.0) continue;
            if (dy > 0 && deltaY <= 1.0) continue;
            if (dy < 0 && deltaY >= -1.0) continue;

            const qreal primary = dx != 0 ? qAbs(deltaX) : qAbs(deltaY);
            const qreal secondary = dx != 0 ? qAbs(deltaY) : qAbs(deltaX);

            qreal overlap = 0.0;
            if (dx != 0) {
                const qreal top = qMax(candidate.rect.top(), originRect.top());
                const qreal bottom = qMin(candidate.rect.bottom(), originRect.bottom());
                overlap = qMax<qreal>(0.0, bottom - top);
            } else {
                const qreal left = qMax(candidate.rect.left(), originRect.left());
                const qreal right = qMin(candidate.rect.right(), originRect.right());
                overlap = qMax<qreal>(0.0, right - left);
            }

            if (requireBandOverlap && overlap <= 0.0) {
                continue;
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

        return best ? best->item : nullptr;
    };

    QQuickItem *target = pick(true);
    if (!target) {
        // No candidate within the band. Allow diagonal movement, but when a
        // list was scrolled, stay inside that list before leaving it.
        target = pick(false);
    }
    if (!target && restrictToViewport) {
        restrictToViewport = false;
        target = pick(false);
    }

    if (target) {
        focusItem(target);
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
