#pragma once
#include <QWidget>
#include <cstdint>

namespace openscreen {

class TimelineRuler : public QWidget {
    Q_OBJECT
public:
    explicit TimelineRuler(QWidget* parent = nullptr);
    ~TimelineRuler() override = default;

    void setDuration(int64_t durationMs);
    void setViewRange(int64_t startMs, int64_t endMs);
    void setPlayheadPosition(int64_t timeMs);

    int64_t positionToTime(int x) const;
    int timeToPosition(int64_t timeMs) const;

signals:
    void seekRequested(int64_t timeMs);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    struct TickConfig {
        int64_t majorIntervalMs;
        int64_t minorIntervalMs;
        QString format;
    };
    TickConfig computeTickConfig() const;
    QString formatTime(int64_t ms) const;

    int64_t durationMs_ = 0;
    int64_t viewStartMs_ = 0;
    int64_t viewEndMs_ = 0;
    int64_t playheadMs_ = 0;
};

} // namespace openscreen
