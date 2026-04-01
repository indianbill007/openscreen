#include "TimelineController.h"
#include "TimelineWidget.h"
#include "core/EditorState.h"
#include "core/types.h"
#include "render/PlaybackEngine.h"

#include <QUuid>
#include <algorithm>

namespace openscreen {

namespace {

/// Generate a short unique ID for a new region
std::string generateId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction & wiring
// ---------------------------------------------------------------------------

TimelineController::TimelineController(TimelineWidget* timeline,
                                       EditorHistory* history,
                                       PlaybackEngine* engine,
                                       QObject* parent)
    : QObject(parent)
    , timeline_(timeline)
    , history_(history)
    , engine_(engine)
{
    // Zoom
    connect(timeline_, &TimelineWidget::zoomAdded,    this, &TimelineController::onZoomAdded);
    connect(timeline_, &TimelineWidget::zoomMoved,    this, &TimelineController::onZoomMoved);
    connect(timeline_, &TimelineWidget::zoomResized,  this, &TimelineController::onZoomResized);
    connect(timeline_, &TimelineWidget::zoomDeleted,  this, &TimelineController::onZoomDeleted);

    // Trim
    connect(timeline_, &TimelineWidget::trimAdded,    this, &TimelineController::onTrimAdded);
    connect(timeline_, &TimelineWidget::trimMoved,    this, &TimelineController::onTrimMoved);
    connect(timeline_, &TimelineWidget::trimResized,  this, &TimelineController::onTrimResized);
    connect(timeline_, &TimelineWidget::trimDeleted,  this, &TimelineController::onTrimDeleted);

    // Speed
    connect(timeline_, &TimelineWidget::speedAdded,   this, &TimelineController::onSpeedAdded);
    connect(timeline_, &TimelineWidget::speedMoved,   this, &TimelineController::onSpeedMoved);
    connect(timeline_, &TimelineWidget::speedResized, this, &TimelineController::onSpeedResized);
    connect(timeline_, &TimelineWidget::speedDeleted, this, &TimelineController::onSpeedDeleted);

    // Annotation
    connect(timeline_, &TimelineWidget::annotationAdded,   this, &TimelineController::onAnnotationAdded);
    connect(timeline_, &TimelineWidget::annotationMoved,   this, &TimelineController::onAnnotationMoved);
    connect(timeline_, &TimelineWidget::annotationResized, this, &TimelineController::onAnnotationResized);
    connect(timeline_, &TimelineWidget::annotationDeleted, this, &TimelineController::onAnnotationDeleted);

    // Seek
    connect(timeline_, &TimelineWidget::seekRequested, this, &TimelineController::onSeekRequested);
}

// ---------------------------------------------------------------------------
// Sync
// ---------------------------------------------------------------------------

void TimelineController::syncFromState()
{
    timeline_->loadFromEditorState(history_->state());
}

void TimelineController::pushStateAndSync()
{
    // The caller has already pushed the state to history_;
    // refresh the timeline UI.
    syncFromState();

    // Update the playback engine's pointer to the current state.
    engine_->setEditorState(&history_->state());
}

// ---------------------------------------------------------------------------
// Zoom slots
// ---------------------------------------------------------------------------

void TimelineController::onZoomAdded(int64_t startMs, int64_t endMs)
{
    auto state = history_->state(); // copy
    ZoomRegion region;
    region.id = generateId();
    region.startMs = static_cast<int>(startMs);
    region.endMs = static_cast<int>(endMs);
    region.depth = DEFAULT_ZOOM_DEPTH;          // D3
    region.focus = ZoomFocus{0.5, 0.5};
    state.zoomRegions.push_back(region);
    history_->pushState(state);
    pushStateAndSync();
}

void TimelineController::onZoomMoved(const QString& id, int64_t startMs, int64_t endMs)
{
    auto state = history_->state();
    const auto idStr = id.toStdString();
    for (auto& z : state.zoomRegions) {
        if (z.id == idStr) {
            z.startMs = static_cast<int>(startMs);
            z.endMs = static_cast<int>(endMs);
            break;
        }
    }
    history_->pushState(state);
    pushStateAndSync();
}

void TimelineController::onZoomResized(const QString& id, int64_t startMs, int64_t endMs)
{
    auto state = history_->state();
    const auto idStr = id.toStdString();
    for (auto& z : state.zoomRegions) {
        if (z.id == idStr) {
            z.startMs = static_cast<int>(startMs);
            z.endMs = static_cast<int>(endMs);
            break;
        }
    }
    history_->pushState(state);
    pushStateAndSync();
}

void TimelineController::onZoomDeleted(const QString& id)
{
    auto state = history_->state();
    const auto idStr = id.toStdString();
    auto& v = state.zoomRegions;
    v.erase(std::remove_if(v.begin(), v.end(),
        [&](const ZoomRegion& z) { return z.id == idStr; }),
        v.end());
    history_->pushState(state);
    pushStateAndSync();
}

// ---------------------------------------------------------------------------
// Trim slots
// ---------------------------------------------------------------------------

void TimelineController::onTrimAdded(int64_t startMs, int64_t endMs)
{
    auto state = history_->state();
    TrimRegion region;
    region.id = generateId();
    region.startMs = static_cast<int>(startMs);
    region.endMs = static_cast<int>(endMs);
    state.trimRegions.push_back(region);
    history_->pushState(state);
    pushStateAndSync();
}

void TimelineController::onTrimMoved(const QString& id, int64_t startMs, int64_t endMs)
{
    auto state = history_->state();
    const auto idStr = id.toStdString();
    for (auto& t : state.trimRegions) {
        if (t.id == idStr) {
            t.startMs = static_cast<int>(startMs);
            t.endMs = static_cast<int>(endMs);
            break;
        }
    }
    history_->pushState(state);
    pushStateAndSync();
}

void TimelineController::onTrimResized(const QString& id, int64_t startMs, int64_t endMs)
{
    auto state = history_->state();
    const auto idStr = id.toStdString();
    for (auto& t : state.trimRegions) {
        if (t.id == idStr) {
            t.startMs = static_cast<int>(startMs);
            t.endMs = static_cast<int>(endMs);
            break;
        }
    }
    history_->pushState(state);
    pushStateAndSync();
}

void TimelineController::onTrimDeleted(const QString& id)
{
    auto state = history_->state();
    const auto idStr = id.toStdString();
    auto& v = state.trimRegions;
    v.erase(std::remove_if(v.begin(), v.end(),
        [&](const TrimRegion& t) { return t.id == idStr; }),
        v.end());
    history_->pushState(state);
    pushStateAndSync();
}

// ---------------------------------------------------------------------------
// Speed slots
// ---------------------------------------------------------------------------

void TimelineController::onSpeedAdded(int64_t startMs, int64_t endMs)
{
    auto state = history_->state();
    SpeedRegion region;
    region.id = generateId();
    region.startMs = static_cast<int>(startMs);
    region.endMs = static_cast<int>(endMs);
    region.speed = PlaybackSpeed::X1_50;
    state.speedRegions.push_back(region);
    history_->pushState(state);
    pushStateAndSync();
}

void TimelineController::onSpeedMoved(const QString& id, int64_t startMs, int64_t endMs)
{
    auto state = history_->state();
    const auto idStr = id.toStdString();
    for (auto& s : state.speedRegions) {
        if (s.id == idStr) {
            s.startMs = static_cast<int>(startMs);
            s.endMs = static_cast<int>(endMs);
            break;
        }
    }
    history_->pushState(state);
    pushStateAndSync();
}

void TimelineController::onSpeedResized(const QString& id, int64_t startMs, int64_t endMs)
{
    auto state = history_->state();
    const auto idStr = id.toStdString();
    for (auto& s : state.speedRegions) {
        if (s.id == idStr) {
            s.startMs = static_cast<int>(startMs);
            s.endMs = static_cast<int>(endMs);
            break;
        }
    }
    history_->pushState(state);
    pushStateAndSync();
}

void TimelineController::onSpeedDeleted(const QString& id)
{
    auto state = history_->state();
    const auto idStr = id.toStdString();
    auto& v = state.speedRegions;
    v.erase(std::remove_if(v.begin(), v.end(),
        [&](const SpeedRegion& s) { return s.id == idStr; }),
        v.end());
    history_->pushState(state);
    pushStateAndSync();
}

// ---------------------------------------------------------------------------
// Annotation slots
// ---------------------------------------------------------------------------

void TimelineController::onAnnotationAdded(int64_t startMs, int64_t endMs)
{
    auto state = history_->state();
    AnnotationRegion region;
    region.id = generateId();
    region.startMs = static_cast<int>(startMs);
    region.endMs = static_cast<int>(endMs);
    region.type = AnnotationType::Text;
    region.content = "Text";
    region.position = DEFAULT_ANNOTATION_POSITION;
    region.size = DEFAULT_ANNOTATION_SIZE;
    region.style = DEFAULT_ANNOTATION_STYLE;
    state.annotationRegions.push_back(region);
    history_->pushState(state);
    pushStateAndSync();
}

void TimelineController::onAnnotationMoved(const QString& id, int64_t startMs, int64_t endMs)
{
    auto state = history_->state();
    const auto idStr = id.toStdString();
    for (auto& a : state.annotationRegions) {
        if (a.id == idStr) {
            a.startMs = static_cast<int>(startMs);
            a.endMs = static_cast<int>(endMs);
            break;
        }
    }
    history_->pushState(state);
    pushStateAndSync();
}

void TimelineController::onAnnotationResized(const QString& id, int64_t startMs, int64_t endMs)
{
    auto state = history_->state();
    const auto idStr = id.toStdString();
    for (auto& a : state.annotationRegions) {
        if (a.id == idStr) {
            a.startMs = static_cast<int>(startMs);
            a.endMs = static_cast<int>(endMs);
            break;
        }
    }
    history_->pushState(state);
    pushStateAndSync();
}

void TimelineController::onAnnotationDeleted(const QString& id)
{
    auto state = history_->state();
    const auto idStr = id.toStdString();
    auto& v = state.annotationRegions;
    v.erase(std::remove_if(v.begin(), v.end(),
        [&](const AnnotationRegion& a) { return a.id == idStr; }),
        v.end());
    history_->pushState(state);
    pushStateAndSync();
}

// ---------------------------------------------------------------------------
// Seek
// ---------------------------------------------------------------------------

void TimelineController::onSeekRequested(int64_t timeMs)
{
    engine_->seekTo(timeMs);
}

} // namespace openscreen
