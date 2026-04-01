#include "TimelineItem.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

namespace openscreen {

namespace {
constexpr int kBorderRadius = 4;
constexpr int kLabelFontSize = 10;
constexpr int kSelectionBorderWidth = 2;
constexpr auto kSelectionBorderColor = "#6366f1";
constexpr int kNormalAlpha = 180;
constexpr int kSelectedAlpha = 220;
} // namespace

TimelineItem::TimelineItem(const QString& id, Variant variant, QWidget* parent)
    : QWidget(parent)
    , id_(id)
    , variant_(variant)
{
    setCursor(Qt::ArrowCursor);
    setMouseTracking(true);
}

void TimelineItem::setTimeRange(int64_t startMs, int64_t endMs)
{
    startMs_ = startMs;
    endMs_ = endMs;
    update();
}

void TimelineItem::setSelected(bool selected)
{
    selected_ = selected;
    update();
}

void TimelineItem::setLabel(const QString& label)
{
    label_ = label;
    update();
}

QColor TimelineItem::variantColor() const
{
    switch (variant_) {
    case Variant::Zoom:       return QColor("#3b82f6");
    case Variant::Trim:       return QColor("#ef4444");
    case Variant::Speed:      return QColor("#22c55e");
    case Variant::Annotation: return QColor("#a855f7");
    }
    return QColor("#3b82f6");
}

QColor TimelineItem::variantColorLight() const
{
    QColor base = variantColor();
    return base.lighter(140);
}

TimelineItem::DragMode TimelineItem::hitTest(int x) const
{
    if (x <= kResizeHandleWidth) {
        return DragMode::ResizeLeft;
    }
    if (x >= width() - kResizeHandleWidth) {
        return DragMode::ResizeRight;
    }
    return DragMode::Move;
}

void TimelineItem::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF body = QRectF(rect()).adjusted(1, 1, -1, -1);

    // Fill with variant color
    QColor fillColor = variantColor();
    fillColor.setAlpha(selected_ ? kSelectedAlpha : kNormalAlpha);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);
    painter.drawRoundedRect(body, kBorderRadius, kBorderRadius);

    // Draw resize handles at left/right edges
    const QColor handleColor = variantColorLight();
    painter.setBrush(handleColor);
    painter.drawRect(QRectF(body.left(), body.top(), 2, body.height()));
    painter.drawRect(QRectF(body.right() - 2, body.top(), 2, body.height()));

    // Draw label centered
    if (!label_.isEmpty()) {
        QFont labelFont;
        labelFont.setPixelSize(kLabelFontSize);
        painter.setFont(labelFont);
        painter.setPen(Qt::white);
        painter.drawText(body, Qt::AlignCenter, label_);
    }

    // Selection border
    if (selected_) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(kSelectionBorderColor), kSelectionBorderWidth));
        painter.drawRoundedRect(body, kBorderRadius, kBorderRadius);
    }
}

void TimelineItem::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragMode_ = hitTest(event->pos().x());
        dragStartX_ = event->pos().x();
        dragStartMs_ = startMs_;
        dragEndMs_ = endMs_;
        emit selected(id_);
    }
    QWidget::mousePressEvent(event);
}

void TimelineItem::mouseMoveEvent(QMouseEvent* event)
{
    if (dragMode_ == DragMode::None) {
        // Update cursor based on hover position
        const auto mode = hitTest(event->pos().x());
        if (mode == DragMode::ResizeLeft || mode == DragMode::ResizeRight) {
            setCursor(Qt::SizeHorCursor);
        } else {
            setCursor(Qt::SizeAllCursor);
        }
        return;
    }

    if (!xToTime || !timeToX) {
        return;
    }

    const int deltaX = event->pos().x() - dragStartX_;

    switch (dragMode_) {
    case DragMode::Move: {
        // Convert pixel delta to time delta
        const int64_t timeAtStart = xToTime(timeToX(dragStartMs_));
        const int64_t timeAtCurrent = xToTime(timeToX(dragStartMs_) + deltaX);
        const int64_t deltaMs = timeAtCurrent - timeAtStart;

        const int64_t newStart = dragStartMs_ + deltaMs;
        const int64_t newEnd = dragEndMs_ + deltaMs;
        emit moved(id_, newStart, newEnd);
        break;
    }
    case DragMode::ResizeLeft: {
        // Map the mouse position within the parent to a time
        const int parentX = mapToParent(event->pos()).x();
        const int64_t newStart = xToTime(parentX);
        emit resized(id_, newStart, dragEndMs_);
        break;
    }
    case DragMode::ResizeRight: {
        const int parentX = mapToParent(event->pos()).x();
        const int64_t newEnd = xToTime(parentX);
        emit resized(id_, dragStartMs_, newEnd);
        break;
    }
    case DragMode::None:
        break;
    }
}

void TimelineItem::mouseReleaseEvent(QMouseEvent* event)
{
    dragMode_ = DragMode::None;
    setCursor(Qt::ArrowCursor);
    QWidget::mouseReleaseEvent(event);
}

void TimelineItem::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    QAction* deleteAction = menu.addAction("Delete");

    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == deleteAction) {
        emit deleteRequested(id_);
    }
}

} // namespace openscreen
