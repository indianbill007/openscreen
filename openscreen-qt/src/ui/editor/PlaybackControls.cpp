#include "PlaybackControls.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>

namespace openscreen {

namespace {
constexpr int kScrubberMax = 10000;
constexpr auto kPlayIcon = "\u25B6";
constexpr auto kPauseIcon = "\u23F8";
constexpr auto kTimeFontFamily = "Consolas, 'Courier New', monospace";
} // namespace

PlaybackControls::PlaybackControls(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet("background-color: #242424;");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);

    // Play/pause button
    playPauseButton_ = new QPushButton(QString::fromUtf8(kPlayIcon), this);
    playPauseButton_->setFixedSize(32, 32);
    playPauseButton_->setStyleSheet(
        "QPushButton {"
        "  background-color: #2a2a2a;"
        "  color: #ffffff;"
        "  border: 1px solid #333333;"
        "  border-radius: 6px;"
        "  font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #6366f1;"
        "  border-color: #6366f1;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #4f46e5;"
        "}"
    );
    layout->addWidget(playPauseButton_);

    // Timeline scrubber
    scrubber_ = new QSlider(Qt::Horizontal, this);
    scrubber_->setRange(0, kScrubberMax);
    scrubber_->setValue(0);
    layout->addWidget(scrubber_, /*stretch=*/1);

    // Time display: current / duration
    const QString timeLabelStyle = QString(
        "QLabel {"
        "  color: #ffffff;"
        "  background: transparent;"
        "  font-family: %1;"
        "  font-size: 12px;"
        "}"
    ).arg(kTimeFontFamily);

    currentTimeLabel_ = new QLabel(formatTime(0), this);
    currentTimeLabel_->setStyleSheet(timeLabelStyle);
    currentTimeLabel_->setMinimumWidth(56);
    currentTimeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(currentTimeLabel_);

    separatorLabel_ = new QLabel("/", this);
    separatorLabel_->setStyleSheet(
        "QLabel { color: #888888; background: transparent; font-size: 12px; }"
    );
    layout->addWidget(separatorLabel_);

    durationLabel_ = new QLabel(formatTime(0), this);
    durationLabel_->setStyleSheet(timeLabelStyle);
    durationLabel_->setMinimumWidth(56);
    durationLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(durationLabel_);

    // Connections
    connect(playPauseButton_, &QPushButton::clicked,
            this, &PlaybackControls::onPlayPauseClicked);
    connect(scrubber_, &QSlider::sliderPressed,
            this, &PlaybackControls::onSliderPressed);
    connect(scrubber_, &QSlider::sliderReleased,
            this, &PlaybackControls::onSliderReleased);
    connect(scrubber_, &QSlider::sliderMoved,
            this, &PlaybackControls::onSliderMoved);
}

void PlaybackControls::setDuration(int64_t durationMs)
{
    durationMs_ = durationMs;
    durationLabel_->setText(formatTime(durationMs_));
}

void PlaybackControls::setCurrentTime(int64_t timeMs)
{
    currentTimeMs_ = timeMs;
    if (!scrubbing_) {
        const int sliderValue = (durationMs_ > 0)
            ? static_cast<int>((timeMs * kScrubberMax) / durationMs_)
            : 0;
        scrubber_->setValue(sliderValue);
    }
    updateTimeLabel();
}

void PlaybackControls::setPlaying(bool playing)
{
    playing_ = playing;
    playPauseButton_->setText(
        playing_ ? QString::fromUtf8(kPauseIcon) : QString::fromUtf8(kPlayIcon)
    );
}

bool PlaybackControls::isPlaying() const
{
    return playing_;
}

void PlaybackControls::onPlayPauseClicked()
{
    playing_ = !playing_;
    playPauseButton_->setText(
        playing_ ? QString::fromUtf8(kPauseIcon) : QString::fromUtf8(kPlayIcon)
    );
    emit playPauseToggled(playing_);
}

void PlaybackControls::onSliderPressed()
{
    scrubbing_ = true;
    emit scrubStarted();
}

void PlaybackControls::onSliderReleased()
{
    scrubbing_ = false;
    const int64_t timeMs = (durationMs_ > 0)
        ? (static_cast<int64_t>(scrubber_->value()) * durationMs_) / kScrubberMax
        : 0;
    emit seekRequested(timeMs);
    emit scrubEnded();
}

void PlaybackControls::onSliderMoved(int value)
{
    if (!scrubbing_) {
        return;
    }
    const int64_t timeMs = (durationMs_ > 0)
        ? (static_cast<int64_t>(value) * durationMs_) / kScrubberMax
        : 0;
    currentTimeMs_ = timeMs;
    updateTimeLabel();
    emit seekRequested(timeMs);
}

void PlaybackControls::updateTimeLabel()
{
    currentTimeLabel_->setText(formatTime(currentTimeMs_));
}

QString PlaybackControls::formatTime(int64_t ms)
{
    if (ms < 0) {
        ms = 0;
    }
    const int64_t totalSeconds = ms / 1000;
    const int tenths = static_cast<int>((ms % 1000) / 100);
    const int minutes = static_cast<int>(totalSeconds / 60);
    const int seconds = static_cast<int>(totalSeconds % 60);
    return QString("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(tenths);
}

} // namespace openscreen
