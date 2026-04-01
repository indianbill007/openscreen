#include "TimelineWidget.h"

#include <QKeyEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "TimelineItem.h"
#include "TimelineRow.h"
#include "TimelineRuler.h"
#include "../../core/EditorState.h"

namespace openscreen {

namespace {
constexpr int kMinWidgetHeight = 280;
constexpr int kPlayheadWidth = 2;
constexpr auto kPlayheadColor = "#ef4444";
constexpr double kZoomStep = 1.15;  // Multiplier per scroll tick
} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(kMinWidgetHeight);

    ruler_ = new TimelineRuler(this);
    zoomRow_ = new TimelineRow("ZOOM", QColor("#3b82f6"), this);
    trimRow_ = new TimelineRow("TRIM", QColor("#ef4444"), this);
    speedRow_ = new TimelineRow("SPEED", QColor("#22c55e"), this);
    annotationRow_ = new TimelineRow("ANNO", QColor("#a855f7"), this);
    horizontalScroll_ = new QScrollBar(Qt::Horizontal, this);

    setupLayout();
}

// ---------------------------------------------------------------------------
// Layout & wiring
// ---------------------------------------------------------------------------

void TimelineWidget::setupLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(ruler_);
    layout->addWidget(zoomRow_);
    layout->addWidget(trimRow_);
    layout->addWidget(speedRow_);
    layout->addWidget(annotationRow_);
    layout->addWidget(horizontalScroll_);

    // --- Ruler seek -> forward outward ---
    connect(ruler_, &TimelineRuler::seekRequested,
            this, &TimelineWidget::seekRequested);

    // --- Zoom row signals ---
    connect(zoomRow_, &TimelineRow::itemSelected,
            this, [this](const QString& id) {
                selectedItemId_ = id;
                emit zoomSelected(id);
            });
    connect(zoomRow_, &TimelineRow::itemMoved,
            this, &TimelineWidget::zoomMoved);
    connect(zoomRow_, &TimelineRow::itemResized,
            this, &TimelineWidget::zoomResized);
    connect(zoomRow_, &TimelineRow::itemDeleteRequested,
            this, &TimelineWidget::zoomDeleted);
    connect(zoomRow_, &TimelineRow::addRequested, this, [this]() {
        const int64_t span = computeDefaultRegionSpan();
        const int64_t halfSpan = span / 2;
        const int64_t start = std::max(int64_t{0}, playheadMs_ - halfSpan);
        const int64_t end = std::min(durationMs_, start + span);
        emit zoomAdded(start, end);
    });

    // --- Trim row signals ---
    connect(trimRow_, &TimelineRow::itemSelected,
            this, [this](const QString& id) {
                selectedItemId_ = id;
                emit trimSelected(id);
            });
    connect(trimRow_, &TimelineRow::itemMoved,
            this, &TimelineWidget::trimMoved);
    connect(trimRow_, &TimelineRow::itemResized,
            this, &TimelineWidget::trimResized);
    connect(trimRow_, &TimelineRow::itemDeleteRequested,
            this, &TimelineWidget::trimDeleted);
    connect(trimRow_, &TimelineRow::addRequested, this, [this]() {
        const int64_t span = computeDefaultRegionSpan();
        const int64_t halfSpan = span / 2;
        const int64_t start = std::max(int64_t{0}, playheadMs_ - halfSpan);
        const int64_t end = std::min(durationMs_, start + span);
        emit trimAdded(start, end);
    });

    // --- Speed row signals ---
    connect(speedRow_, &TimelineRow::itemSelected,
            this, [this](const QString& id) {
                selectedItemId_ = id;
                emit speedSelected(id);
            });
    connect(speedRow_, &TimelineRow::itemMoved,
            this, &TimelineWidget::speedMoved);
    connect(speedRow_, &TimelineRow::itemResized,
            this, &TimelineWidget::speedResized);
    connect(speedRow_, &TimelineRow::itemDeleteRequested,
            this, &TimelineWidget::speedDeleted);
    connect(speedRow_, &TimelineRow::addRequested, this, [this]() {
        const int64_t span = computeDefaultRegionSpan();
        const int64_t halfSpan = span / 2;
        const int64_t start = std::max(int64_t{0}, playheadMs_ - halfSpan);
        const int64_t end = std::min(durationMs_, start + span);
        emit speedAdded(start, end);
    });

    // --- Annotation row signals ---
    connect(annotationRow_, &TimelineRow::itemSelected,
            this, [this](const QString& id) {
                selectedItemId_ = id;
                emit annotationSelected(id);
            });
    connect(annotationRow_, &TimelineRow::itemMoved,
            this, &TimelineWidget::annotationMoved);
    connect(annotationRow_, &TimelineRow::itemResized,
            this, &TimelineWidget::annotationResized);
    connect(annotationRow_, &TimelineRow::itemDeleteRequested,
            this, &TimelineWidget::annotationDeleted);
    connect(annotationRow_, &TimelineRow::addRequested, this, [this]() {
        const int64_t span = computeDefaultRegionSpan();
        const int64_t halfSpan = span / 2;
        const int64_t start = std::max(int64_t{0}, playheadMs_ - halfSpan);
        const int64_t end = std::min(durationMs_, start + span);
        emit annotationAdded(start, end);
    });

    // --- Horizontal scrollbar ---
    connect(horizontalScroll_, &QScrollBar::valueChanged,
            this, [this](int value) {
                viewStartMs_ = static_cast<int64_t>(value);
                updateViewRange();
            });
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void TimelineWidget::setDuration(int64_t durationMs)
{
    durationMs_ = durationMs;
    viewEndMs_ = durationMs;
    viewStartMs_ = 0;
    zoomLevel_ = kMinZoom;

    ruler_->setDuration(durationMs);

    horizontalScroll_->setRange(0, static_cast<int>(durationMs));
    horizontalScroll_->setPageStep(static_cast<int>(durationMs));

    updateViewRange();
}

void TimelineWidget::setPlayheadPosition(int64_t timeMs)
{
    playheadMs_ = timeMs;
    ruler_->setPlayheadPosition(timeMs);
    update();  // Repaint for playhead line
}

void TimelineWidget::loadFromEditorState(const EditorState& state)
{
    // Clear existing items from all rows
    zoomRow_->clear();
    trimRow_->clear();
    speedRow_->clear();
    annotationRow_->clear();
    selectedItemId_.clear();

    // Populate zoom regions
    for (const auto& region : state.zoomRegions) {
        const QString id = QString::fromStdString(region.id);
        const QString label = QString("D%1").arg(static_cast<int>(region.depth));
        zoomRow_->addItem(id, TimelineItem::Variant::Zoom,
                          region.startMs, region.endMs, label);
    }

    // Populate trim regions
    for (const auto& region : state.trimRegions) {
        const QString id = QString::fromStdString(region.id);
        trimRow_->addItem(id, TimelineItem::Variant::Trim,
                          region.startMs, region.endMs, QString());
    }

    // Populate speed regions
    for (const auto& region : state.speedRegions) {
        const QString id = QString::fromStdString(region.id);
        const QString label = QString("%1x")
            .arg(playbackSpeedValue(region.speed), 0, 'g', 3);
        speedRow_->addItem(id, TimelineItem::Variant::Speed,
                           region.startMs, region.endMs, label);
    }

    // Populate annotation regions
    for (const auto& region : state.annotationRegions) {
        const QString id = QString::fromStdString(region.id);
        QString label;
        switch (region.type) {
        case AnnotationType::Text:   label = "Text";   break;
        case AnnotationType::Image:  label = "Image";  break;
        case AnnotationType::Figure: label = "Figure"; break;
        }
        annotationRow_->addItem(id, TimelineItem::Variant::Annotation,
                                region.startMs, region.endMs, label);
    }

    updateViewRange();
}

QString TimelineWidget::selectedItemId() const
{
    return selectedItemId_;
}

void TimelineWidget::selectItem(const QString& id)
{
    selectedItemId_ = id;
    zoomRow_->selectItem(id);
    trimRow_->selectItem(id);
    speedRow_->selectItem(id);
    annotationRow_->selectItem(id);
}

void TimelineWidget::deselectAll()
{
    selectedItemId_.clear();
    zoomRow_->deselectAll();
    trimRow_->deselectAll();
    speedRow_->deselectAll();
    annotationRow_->deselectAll();
    emit selectionCleared();
}

// ---------------------------------------------------------------------------
// View range management
// ---------------------------------------------------------------------------

void TimelineWidget::updateViewRange()
{
    if (durationMs_ <= 0) {
        return;
    }

    const int64_t visibleMs = static_cast<int64_t>(
        static_cast<double>(durationMs_) / zoomLevel_);
    viewEndMs_ = viewStartMs_ + visibleMs;

    // Clamp so we don't go past the end
    if (viewEndMs_ > durationMs_) {
        viewEndMs_ = durationMs_;
        viewStartMs_ = durationMs_ - visibleMs;
        if (viewStartMs_ < 0) {
            viewStartMs_ = 0;
        }
    }

    // Update child widgets with the new view range
    ruler_->setViewRange(viewStartMs_, viewEndMs_);
    zoomRow_->setViewRange(viewStartMs_, viewEndMs_);
    trimRow_->setViewRange(viewStartMs_, viewEndMs_);
    speedRow_->setViewRange(viewStartMs_, viewEndMs_);
    annotationRow_->setViewRange(viewStartMs_, viewEndMs_);

    // Update scrollbar to reflect the current view (block signals to avoid
    // recursive updates)
    horizontalScroll_->blockSignals(true);
    horizontalScroll_->setRange(
        0, static_cast<int>(std::max(int64_t{0}, durationMs_ - visibleMs)));
    horizontalScroll_->setPageStep(static_cast<int>(visibleMs));
    horizontalScroll_->setValue(static_cast<int>(viewStartMs_));
    horizontalScroll_->blockSignals(false);

    update();
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void TimelineWidget::wheelEvent(QWheelEvent* event)
{
    if (durationMs_ <= 0) {
        return;
    }

    const int delta = event->angleDelta().y();
    if (delta == 0) {
        return;
    }

    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl+scroll: zoom in/out centered on mouse position
        const double mouseRatio =
            static_cast<double>(event->position().x()) / width();
        const int64_t mouseTimeMs =
            viewStartMs_ + static_cast<int64_t>(
                mouseRatio * (viewEndMs_ - viewStartMs_));

        // Adjust zoom level
        if (delta > 0) {
            zoomLevel_ = std::min(kMaxZoom, zoomLevel_ * kZoomStep);
        } else {
            zoomLevel_ = std::max(kMinZoom, zoomLevel_ / kZoomStep);
        }

        // Recompute view range keeping mouse position anchored
        const int64_t newVisibleMs = static_cast<int64_t>(
            static_cast<double>(durationMs_) / zoomLevel_);
        viewStartMs_ = mouseTimeMs -
            static_cast<int64_t>(mouseRatio * newVisibleMs);

        // Clamp
        if (viewStartMs_ < 0) {
            viewStartMs_ = 0;
        }
        if (viewStartMs_ + newVisibleMs > durationMs_) {
            viewStartMs_ = durationMs_ - newVisibleMs;
            if (viewStartMs_ < 0) {
                viewStartMs_ = 0;
            }
        }

        updateViewRange();
        event->accept();
    } else if (event->modifiers() & Qt::ShiftModifier) {
        // Shift+scroll: horizontal pan
        const int64_t visibleMs = viewEndMs_ - viewStartMs_;
        const int64_t panStep = visibleMs / 10;
        const int64_t panDelta = (delta > 0) ? -panStep : panStep;

        viewStartMs_ += panDelta;

        // Clamp
        if (viewStartMs_ < 0) {
            viewStartMs_ = 0;
        }
        if (viewStartMs_ + visibleMs > durationMs_) {
            viewStartMs_ = durationMs_ - visibleMs;
            if (viewStartMs_ < 0) {
                viewStartMs_ = 0;
            }
        }

        updateViewRange();
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void TimelineWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    drawPlayhead(painter);
}

void TimelineWidget::drawPlayhead(QPainter& painter)
{
    if (durationMs_ <= 0) {
        return;
    }

    const int64_t visibleRange = viewEndMs_ - viewStartMs_;
    if (visibleRange <= 0) {
        return;
    }

    if (playheadMs_ < viewStartMs_ || playheadMs_ > viewEndMs_) {
        return;
    }

    const double ratio =
        static_cast<double>(playheadMs_ - viewStartMs_) / visibleRange;
    const int x = static_cast<int>(ratio * width());

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(kPlayheadColor));

    // Draw from below the ruler to the bottom of the last row
    // (above the scrollbar)
    const int top = ruler_->geometry().bottom();
    const int bottom = horizontalScroll_->geometry().top();
    painter.drawRect(x - kPlayheadWidth / 2, top, kPlayheadWidth, bottom - top);
}

void TimelineWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        deselectAll();
        event->accept();
        return;
    }

    if (selectedItemId_.isEmpty()) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        // Determine which row contains the selected item and emit the
        // appropriate delete signal.
        if (zoomRow_->containsItem(selectedItemId_)) {
            emit zoomDeleted(selectedItemId_);
        } else if (trimRow_->containsItem(selectedItemId_)) {
            emit trimDeleted(selectedItemId_);
        } else if (speedRow_->containsItem(selectedItemId_)) {
            emit speedDeleted(selectedItemId_);
        } else if (annotationRow_->containsItem(selectedItemId_)) {
            emit annotationDeleted(selectedItemId_);
        }

        selectedItemId_.clear();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

int64_t TimelineWidget::computeDefaultRegionSpan() const
{
    constexpr int64_t kMinSpanMs = 1000;
    const int64_t tenthOfDuration = durationMs_ / 10;
    return std::max(kMinSpanMs, tenthOfDuration);
}

} // namespace openscreen
