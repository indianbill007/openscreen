#pragma once

#include <QObject>
#include <cstdint>
#include <string>

namespace openscreen {

class TimelineWidget;
class EditorHistory;
class PlaybackEngine;

class TimelineController : public QObject {
    Q_OBJECT
public:
    explicit TimelineController(TimelineWidget* timeline,
                                EditorHistory* history,
                                PlaybackEngine* engine,
                                QObject* parent = nullptr);

    /// Sync the timeline UI with the current editor state
    void syncFromState();

private slots:
    // Zoom
    void onZoomAdded(int64_t startMs, int64_t endMs);
    void onZoomMoved(const QString& id, int64_t startMs, int64_t endMs);
    void onZoomResized(const QString& id, int64_t startMs, int64_t endMs);
    void onZoomDeleted(const QString& id);

    // Trim
    void onTrimAdded(int64_t startMs, int64_t endMs);
    void onTrimMoved(const QString& id, int64_t startMs, int64_t endMs);
    void onTrimResized(const QString& id, int64_t startMs, int64_t endMs);
    void onTrimDeleted(const QString& id);

    // Speed
    void onSpeedAdded(int64_t startMs, int64_t endMs);
    void onSpeedMoved(const QString& id, int64_t startMs, int64_t endMs);
    void onSpeedResized(const QString& id, int64_t startMs, int64_t endMs);
    void onSpeedDeleted(const QString& id);

    // Annotation
    void onAnnotationAdded(int64_t startMs, int64_t endMs);
    void onAnnotationMoved(const QString& id, int64_t startMs, int64_t endMs);
    void onAnnotationResized(const QString& id, int64_t startMs, int64_t endMs);
    void onAnnotationDeleted(const QString& id);

    // Seek
    void onSeekRequested(int64_t timeMs);

private:
    void pushStateAndSync();

    TimelineWidget* timeline_;
    EditorHistory* history_;
    PlaybackEngine* engine_;
};

} // namespace openscreen
