#pragma once
#include <QWidget>
#include <QScrollBar>
#include <cstdint>
#include <string>

namespace openscreen {

class TimelineRuler;
class TimelineRow;
class TimelineItem;
struct EditorState;

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    /// Set the total video duration
    void setDuration(int64_t durationMs);

    /// Set the current playhead position
    void setPlayheadPosition(int64_t timeMs);

    /// Load editor state into the timeline (populates all rows)
    void loadFromEditorState(const EditorState& state);

    /// Get current selection
    QString selectedItemId() const;

    /// Select a specific item
    void selectItem(const QString& id);

    /// Deselect all
    void deselectAll();

signals:
    // Playback
    void seekRequested(int64_t timeMs);

    // Zoom regions
    void zoomAdded(int64_t startMs, int64_t endMs);
    void zoomMoved(const QString& id, int64_t startMs, int64_t endMs);
    void zoomResized(const QString& id, int64_t startMs, int64_t endMs);
    void zoomDeleted(const QString& id);
    void zoomSelected(const QString& id);

    // Trim regions
    void trimAdded(int64_t startMs, int64_t endMs);
    void trimMoved(const QString& id, int64_t startMs, int64_t endMs);
    void trimResized(const QString& id, int64_t startMs, int64_t endMs);
    void trimDeleted(const QString& id);
    void trimSelected(const QString& id);

    // Speed regions
    void speedAdded(int64_t startMs, int64_t endMs);
    void speedMoved(const QString& id, int64_t startMs, int64_t endMs);
    void speedResized(const QString& id, int64_t startMs, int64_t endMs);
    void speedDeleted(const QString& id);
    void speedSelected(const QString& id);

    // Annotation regions
    void annotationAdded(int64_t startMs, int64_t endMs);
    void annotationMoved(const QString& id, int64_t startMs, int64_t endMs);
    void annotationResized(const QString& id, int64_t startMs, int64_t endMs);
    void annotationDeleted(const QString& id);
    void annotationSelected(const QString& id);

    // Selection cleared
    void selectionCleared();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupLayout();
    void updateViewRange();
    void drawPlayhead(QPainter& painter);
    int64_t computeDefaultRegionSpan() const;

    TimelineRuler* ruler_;
    TimelineRow* zoomRow_;
    TimelineRow* trimRow_;
    TimelineRow* speedRow_;
    TimelineRow* annotationRow_;
    QScrollBar* horizontalScroll_;

    int64_t durationMs_ = 0;
    int64_t viewStartMs_ = 0;
    int64_t viewEndMs_ = 0;
    int64_t playheadMs_ = 0;
    double zoomLevel_ = 1.0;  // 1.0 = fit all, higher = zoom in
    QString selectedItemId_;

    static constexpr double kMinZoom = 1.0;
    static constexpr double kMaxZoom = 50.0;
};

} // namespace openscreen
