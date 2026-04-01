#include "TimelineRow.h"
#include "TimelineItem.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QResizeEvent>

#include <algorithm>

namespace openscreen {

namespace {
constexpr auto kBackgroundColor = "#18181b";
constexpr auto kBorderColor = "#2a2a2a";
constexpr auto kHintColor = "#ffffff";
constexpr int kHintAlpha = 21; // ~0x15, matching #ffffff15
constexpr int kLabelFontSize = 9;
constexpr int kAddButtonSize = 20;
constexpr int kAddButtonMargin = 4;
} // namespace

TimelineRow::TimelineRow(const QString& label, const QColor& labelColor,
                         QWidget* parent)
    : QWidget(parent)
    , label_(label)
    , labelColor_(labelColor)
{
    setFixedHeight(kRowHeight);
    setMouseTracking(true);
    setStyleSheet(
        QString("background-color: %1; border-bottom: 1px solid %2;")
            .arg(kBackgroundColor, kBorderColor));

    // "+" button at the left side
    addButton_ = new QPushButton("+", this);
    addButton_->setFixedSize(kAddButtonSize, kAddButtonSize);
    addButton_->move(kAddButtonMargin,
                     (kRowHeight - kAddButtonSize) / 2);
    addButton_->setStyleSheet(
        "QPushButton {"
        "  background-color: #2a2a2a;"
        "  color: #ffffff;"
        "  border: 1px solid #333333;"
        "  border-radius: 4px;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background-color: #6366f1;"
        "  border-color: #6366f1;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #4f46e5;"
        "}"
    );

    connect(addButton_, &QPushButton::clicked,
            this, &TimelineRow::addRequested);
}

void TimelineRow::setViewRange(int64_t startMs, int64_t endMs)
{
    viewStartMs_ = startMs;
    viewEndMs_ = endMs;
    layoutItems();
    update();
}

void TimelineRow::setDuration(int64_t durationMs)
{
    durationMs_ = durationMs;
    if (viewEndMs_ == 0) {
        viewEndMs_ = durationMs;
    }
    update();
}

TimelineItem* TimelineRow::addItem(const QString& id,
                                   TimelineItem::Variant variant,
                                   int64_t startMs, int64_t endMs,
                                   const QString& label)
{
    auto* item = new TimelineItem(id, variant, this);
    item->setTimeRange(startMs, endMs);
    if (!label.isEmpty()) {
        item->setLabel(label);
    }
    item->show();
    connectItem(item);
    items_.push_back(item);
    layoutItems();
    return item;
}

bool TimelineRow::removeItem(const QString& id)
{
    auto it = std::find_if(items_.begin(), items_.end(),
                           [&id](const TimelineItem* item) {
                               return item->id() == id;
                           });
    if (it == items_.end()) {
        return false;
    }
    delete *it;
    items_.erase(it);
    layoutItems();
    return true;
}

void TimelineRow::clearItems()
{
    for (auto* item : items_) {
        delete item;
    }
    items_.clear();
    layoutItems();
}

bool TimelineRow::updateItemRange(const QString& id,
                                  int64_t startMs, int64_t endMs)
{
    auto it = std::find_if(items_.begin(), items_.end(),
                           [&id](const TimelineItem* item) {
                               return item->id() == id;
                           });
    if (it == items_.end()) {
        return false;
    }
    (*it)->setTimeRange(startMs, endMs);
    layoutItems();
    return true;
}

void TimelineRow::selectItem(const QString& id)
{
    for (auto* item : items_) {
        item->setSelected(item->id() == id);
    }
}

void TimelineRow::deselectAll()
{
    for (auto* item : items_) {
        item->setSelected(false);
    }
}

int TimelineRow::timeToPosition(int64_t timeMs) const
{
    const int contentWidth = width() - kHeaderWidth;
    const int64_t range = viewEndMs_ - viewStartMs_;
    if (range <= 0 || contentWidth <= 0) {
        return 0;
    }
    const double ratio =
        static_cast<double>(timeMs - viewStartMs_) / range;
    return static_cast<int>(ratio * contentWidth);
}

int64_t TimelineRow::positionToTime(int x) const
{
    const int contentWidth = width() - kHeaderWidth;
    const int64_t range = viewEndMs_ - viewStartMs_;
    if (range <= 0 || contentWidth <= 0) {
        return viewStartMs_;
    }
    const double ratio = static_cast<double>(x) / contentWidth;
    return viewStartMs_ + static_cast<int64_t>(ratio * range);
}

void TimelineRow::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Background
    painter.fillRect(rect(), QColor(kBackgroundColor));

    // Row label — drawn vertically at left, uppercase, tracking-widest
    {
        painter.save();
        QFont labelFont;
        labelFont.setPixelSize(kLabelFontSize);
        labelFont.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
        labelFont.setCapitalization(QFont::AllUppercase);
        painter.setFont(labelFont);
        painter.setPen(labelColor_);

        // Rotate and draw centered in the header column
        const int cx = kHeaderWidth / 2;
        const int cy = height() / 2;
        painter.translate(cx, cy);
        painter.rotate(-90);
        const QRect textRect(-height() / 2, -kHeaderWidth / 2,
                             height(), kHeaderWidth);
        painter.drawText(textRect, Qt::AlignCenter, label_.toUpper());
        painter.restore();
    }

    // Bottom border line
    painter.setPen(QPen(QColor(kBorderColor), 1));
    painter.drawLine(0, height() - 1, width(), height() - 1);

    // Hint text when no items exist
    if (items_.empty()) {
        QColor hintColor(kHintColor);
        hintColor.setAlpha(kHintAlpha);
        painter.setPen(hintColor);
        QFont hintFont;
        hintFont.setPixelSize(11);
        painter.setFont(hintFont);
        const QRect contentRect(kHeaderWidth, 0,
                                width() - kHeaderWidth, height());
        painter.drawText(contentRect, Qt::AlignCenter,
                         QStringLiteral("Click + to add"));
    }
}

void TimelineRow::resizeEvent(QResizeEvent* /*event*/)
{
    layoutItems();
}

void TimelineRow::layoutItems()
{
    const int contentWidth = width() - kHeaderWidth;
    const int64_t range = viewEndMs_ - viewStartMs_;

    for (auto* item : items_) {
        if (range <= 0 || contentWidth <= 0) {
            item->setGeometry(kHeaderWidth, kItemMarginY, 0,
                              kRowHeight - 2 * kItemMarginY);
            continue;
        }

        const int x = timeToPosition(item->startMs());
        const int w = timeToPosition(item->endMs()) - x;
        item->setGeometry(kHeaderWidth + x, kItemMarginY,
                          std::max(w, 1),
                          kRowHeight - 2 * kItemMarginY);

        // Provide conversion functions so items can map between
        // pixel coordinates and time values during drag operations.
        item->timeToX = [this](int64_t ms) { return timeToPosition(ms); };
        item->xToTime = [this](int px) { return positionToTime(px); };
    }

    update();
}

void TimelineRow::connectItem(TimelineItem* item)
{
    connect(item, &TimelineItem::selected,
            this, [this](const QString& id) {
                selectItem(id);
                emit itemSelected(id);
            });

    connect(item, &TimelineItem::moved,
            this, &TimelineRow::itemMoved);

    connect(item, &TimelineItem::resized,
            this, &TimelineRow::itemResized);

    connect(item, &TimelineItem::deleteRequested,
            this, &TimelineRow::itemDeleteRequested);
}

} // namespace openscreen
