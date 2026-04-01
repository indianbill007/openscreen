#include "TimelineRuler.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

namespace openscreen {

namespace {
constexpr int kRulerHeight = 28;
constexpr int kMinorTickHeight = 4;
constexpr int kMajorTickHeight = 10;
constexpr int kPlayheadWidth = 2;

constexpr auto kBackgroundColor = "#1a1a1a";
constexpr auto kMinorTickColor = "#444444";
constexpr auto kMajorTickColor = "#888888";
constexpr auto kLabelColor = "#888888";
constexpr auto kPlayheadColor = "#ef4444";

constexpr int64_t kOneSecondMs = 1000;
constexpr int64_t kFiveSecondsMs = 5 * kOneSecondMs;
constexpr int64_t kFifteenSecondsMs = 15 * kOneSecondMs;
constexpr int64_t kOneMinuteMs = 60 * kOneSecondMs;
constexpr int64_t kFiveMinutesMs = 5 * kOneMinuteMs;
constexpr int64_t kTwoMinutesMs = 2 * kOneMinuteMs;
constexpr int64_t kTenMinutesMs = 10 * kOneMinuteMs;
} // namespace

TimelineRuler::TimelineRuler(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(kRulerHeight);
    setMouseTracking(true);
}

void TimelineRuler::setDuration(int64_t durationMs)
{
    durationMs_ = durationMs;
    if (viewEndMs_ == 0) {
        viewEndMs_ = durationMs;
    }
    update();
}

void TimelineRuler::setViewRange(int64_t startMs, int64_t endMs)
{
    viewStartMs_ = startMs;
    viewEndMs_ = endMs;
    update();
}

void TimelineRuler::setPlayheadPosition(int64_t timeMs)
{
    playheadMs_ = timeMs;
    update();
}

int64_t TimelineRuler::positionToTime(int x) const
{
    const int64_t range = viewEndMs_ - viewStartMs_;
    if (range <= 0 || width() <= 0) {
        return viewStartMs_;
    }
    const double ratio = static_cast<double>(x) / width();
    return viewStartMs_ + static_cast<int64_t>(ratio * range);
}

int TimelineRuler::timeToPosition(int64_t timeMs) const
{
    const int64_t range = viewEndMs_ - viewStartMs_;
    if (range <= 0) {
        return 0;
    }
    const double ratio = static_cast<double>(timeMs - viewStartMs_) / range;
    return static_cast<int>(ratio * width());
}

TimelineRuler::TickConfig TimelineRuler::computeTickConfig() const
{
    const int64_t range = viewEndMs_ - viewStartMs_;

    if (range < kFiveSecondsMs) {
        return {kOneSecondMs, 100, "s.f"};
    }
    if (range < 30 * kOneSecondMs) {
        return {kFiveSecondsMs, kOneSecondMs, "m:ss"};
    }
    if (range < kTwoMinutesMs) {
        return {kFifteenSecondsMs, kFiveSecondsMs, "m:ss"};
    }
    if (range < kTenMinutesMs) {
        return {kOneMinuteMs, kFifteenSecondsMs, "m:ss"};
    }
    return {kFiveMinutesMs, kOneMinuteMs, "m:ss"};
}

QString TimelineRuler::formatTime(int64_t ms) const
{
    if (ms < 0) {
        ms = 0;
    }
    const int64_t totalSeconds = ms / 1000;
    const int minutes = static_cast<int>(totalSeconds / 60);
    const int seconds = static_cast<int>(totalSeconds % 60);

    if (ms < kOneMinuteMs && viewEndMs_ - viewStartMs_ < kFiveSecondsMs) {
        // Sub-second precision: "S.T" format
        const int tenths = static_cast<int>((ms % 1000) / 100);
        return QString("%1.%2").arg(totalSeconds).arg(tenths);
    }

    // MM:SS format
    return QString("%1:%2")
        .arg(minutes)
        .arg(seconds, 2, 10, QChar('0'));
}

void TimelineRuler::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Background
    painter.fillRect(rect(), QColor(kBackgroundColor));

    const int64_t range = viewEndMs_ - viewStartMs_;
    if (range <= 0) {
        return;
    }

    const auto tickConfig = computeTickConfig();
    const int h = height();

    // Draw minor ticks
    painter.setPen(QPen(QColor(kMinorTickColor), 1));
    {
        const int64_t firstMinor =
            (viewStartMs_ / tickConfig.minorIntervalMs) * tickConfig.minorIntervalMs;
        for (int64_t t = firstMinor; t <= viewEndMs_; t += tickConfig.minorIntervalMs) {
            if (t < viewStartMs_) {
                continue;
            }
            const int x = timeToPosition(t);
            painter.drawLine(x, h - kMinorTickHeight, x, h);
        }
    }

    // Draw major ticks and labels
    painter.setPen(QPen(QColor(kMajorTickColor), 1));
    const QFont labelFont("Consolas, 'Courier New', monospace", 8);
    painter.setFont(labelFont);
    {
        const int64_t firstMajor =
            (viewStartMs_ / tickConfig.majorIntervalMs) * tickConfig.majorIntervalMs;
        for (int64_t t = firstMajor; t <= viewEndMs_; t += tickConfig.majorIntervalMs) {
            if (t < viewStartMs_) {
                continue;
            }
            const int x = timeToPosition(t);

            // Major tick line
            painter.setPen(QPen(QColor(kMajorTickColor), 1));
            painter.drawLine(x, h - kMajorTickHeight, x, h);

            // Time label
            painter.setPen(QColor(kLabelColor));
            const QString label = formatTime(t);
            const QRect textRect(x - 30, 0, 60, h - kMajorTickHeight - 1);
            painter.drawText(textRect, Qt::AlignCenter, label);
        }
    }

    // Draw playhead
    if (playheadMs_ >= viewStartMs_ && playheadMs_ <= viewEndMs_) {
        const int px = timeToPosition(playheadMs_);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(kPlayheadColor));
        painter.drawRect(px - kPlayheadWidth / 2, 0, kPlayheadWidth, h);
    }
}

void TimelineRuler::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int64_t timeMs = positionToTime(event->pos().x());
        emit seekRequested(timeMs);
    }
    QWidget::mousePressEvent(event);
}

} // namespace openscreen
