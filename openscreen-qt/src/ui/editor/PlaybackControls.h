#pragma once
#include <QWidget>

class QSlider;
class QLabel;
class QPushButton;

namespace openscreen {

class PlaybackControls : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackControls(QWidget* parent = nullptr);
    ~PlaybackControls() override = default;

    void setDuration(int64_t durationMs);
    void setCurrentTime(int64_t timeMs);
    void setPlaying(bool playing);
    bool isPlaying() const;

signals:
    void playPauseToggled(bool playing);
    void seekRequested(int64_t timeMs);
    void scrubStarted();
    void scrubEnded();

private slots:
    void onPlayPauseClicked();
    void onSliderPressed();
    void onSliderReleased();
    void onSliderMoved(int value);

private:
    void updateTimeLabel();
    static QString formatTime(int64_t ms);

    QPushButton* playPauseButton_;
    QSlider* scrubber_;
    QLabel* currentTimeLabel_;
    QLabel* durationLabel_;
    QLabel* separatorLabel_;

    int64_t durationMs_ = 0;
    int64_t currentTimeMs_ = 0;
    bool playing_ = false;
    bool scrubbing_ = false;
};

} // namespace openscreen
