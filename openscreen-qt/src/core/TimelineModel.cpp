#include "TimelineModel.h"

#include <algorithm>
#include <stdexcept>

namespace openscreen {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string TimelineModel::generateId(const std::string& prefix, int& counter) {
    return prefix + "-" + std::to_string(counter++);
}

bool TimelineModel::hasOverlap(const std::vector<TrimRegion>& regions,
                               int64_t start, int64_t end,
                               const std::string& excludeId) const {
    return std::any_of(regions.begin(), regions.end(),
        [&](const TrimRegion& r) {
            if (r.id == excludeId) return false;
            return start < r.endMs && end > r.startMs;
        });
}

bool TimelineModel::hasOverlap(const std::vector<SpeedRegion>& regions,
                               int64_t start, int64_t end,
                               const std::string& excludeId) const {
    return std::any_of(regions.begin(), regions.end(),
        [&](const SpeedRegion& r) {
            if (r.id == excludeId) return false;
            return start < r.endMs && end > r.startMs;
        });
}

// ---------------------------------------------------------------------------
// Zoom regions
// ---------------------------------------------------------------------------

const std::vector<ZoomRegion>& TimelineModel::zoomRegions() const {
    return zoomRegions_;
}

ZoomRegion& TimelineModel::addZoomRegion(int64_t startMs, int64_t endMs,
                                         ZoomDepth depth, ZoomFocus focus) {
    if (startMs >= endMs) {
        throw std::invalid_argument("startMs must be less than endMs");
    }

    ZoomRegion region;
    region.id = generateId("zoom", nextZoomId_);
    region.startMs = static_cast<int>(startMs);
    region.endMs = static_cast<int>(endMs);
    region.depth = depth;
    region.focus = focus;

    zoomRegions_.push_back(std::move(region));
    return zoomRegions_.back();
}

bool TimelineModel::removeZoomRegion(const std::string& id) {
    auto it = std::find_if(zoomRegions_.begin(), zoomRegions_.end(),
        [&](const ZoomRegion& r) { return r.id == id; });

    if (it == zoomRegions_.end()) return false;

    zoomRegions_.erase(it);
    return true;
}

bool TimelineModel::resizeZoomRegion(const std::string& id,
                                     int64_t newStartMs, int64_t newEndMs) {
    if (newStartMs >= newEndMs) return false;

    auto it = std::find_if(zoomRegions_.begin(), zoomRegions_.end(),
        [&](const ZoomRegion& r) { return r.id == id; });

    if (it == zoomRegions_.end()) return false;

    it->startMs = static_cast<int>(newStartMs);
    it->endMs = static_cast<int>(newEndMs);
    return true;
}

bool TimelineModel::updateZoomDepth(const std::string& id, ZoomDepth depth) {
    auto it = std::find_if(zoomRegions_.begin(), zoomRegions_.end(),
        [&](const ZoomRegion& r) { return r.id == id; });

    if (it == zoomRegions_.end()) return false;

    it->depth = depth;
    return true;
}

bool TimelineModel::updateZoomFocus(const std::string& id, ZoomFocus focus) {
    auto it = std::find_if(zoomRegions_.begin(), zoomRegions_.end(),
        [&](const ZoomRegion& r) { return r.id == id; });

    if (it == zoomRegions_.end()) return false;

    it->focus = focus;
    return true;
}

// ---------------------------------------------------------------------------
// Trim regions
// ---------------------------------------------------------------------------

const std::vector<TrimRegion>& TimelineModel::trimRegions() const {
    return trimRegions_;
}

TrimRegion* TimelineModel::addTrimRegion(int64_t startMs, int64_t endMs) {
    if (startMs >= endMs) return nullptr;

    if (hasOverlap(trimRegions_, startMs, endMs)) return nullptr;

    TrimRegion region;
    region.id = generateId("trim", nextTrimId_);
    region.startMs = static_cast<int>(startMs);
    region.endMs = static_cast<int>(endMs);

    trimRegions_.push_back(std::move(region));
    return &trimRegions_.back();
}

bool TimelineModel::removeTrimRegion(const std::string& id) {
    auto it = std::find_if(trimRegions_.begin(), trimRegions_.end(),
        [&](const TrimRegion& r) { return r.id == id; });

    if (it == trimRegions_.end()) return false;

    trimRegions_.erase(it);
    return true;
}

bool TimelineModel::resizeTrimRegion(const std::string& id,
                                     int64_t newStartMs, int64_t newEndMs) {
    if (newStartMs >= newEndMs) return false;

    auto it = std::find_if(trimRegions_.begin(), trimRegions_.end(),
        [&](const TrimRegion& r) { return r.id == id; });

    if (it == trimRegions_.end()) return false;

    if (hasOverlap(trimRegions_, newStartMs, newEndMs, id)) return false;

    it->startMs = static_cast<int>(newStartMs);
    it->endMs = static_cast<int>(newEndMs);
    return true;
}

// ---------------------------------------------------------------------------
// Speed regions
// ---------------------------------------------------------------------------

const std::vector<SpeedRegion>& TimelineModel::speedRegions() const {
    return speedRegions_;
}

SpeedRegion* TimelineModel::addSpeedRegion(int64_t startMs, int64_t endMs,
                                           PlaybackSpeed speed) {
    if (startMs >= endMs) return nullptr;

    if (hasOverlap(speedRegions_, startMs, endMs)) return nullptr;

    SpeedRegion region;
    region.id = generateId("speed", nextSpeedId_);
    region.startMs = static_cast<int>(startMs);
    region.endMs = static_cast<int>(endMs);
    region.speed = speed;

    speedRegions_.push_back(std::move(region));
    return &speedRegions_.back();
}

bool TimelineModel::removeSpeedRegion(const std::string& id) {
    auto it = std::find_if(speedRegions_.begin(), speedRegions_.end(),
        [&](const SpeedRegion& r) { return r.id == id; });

    if (it == speedRegions_.end()) return false;

    speedRegions_.erase(it);
    return true;
}

bool TimelineModel::resizeSpeedRegion(const std::string& id,
                                      int64_t newStartMs, int64_t newEndMs) {
    if (newStartMs >= newEndMs) return false;

    auto it = std::find_if(speedRegions_.begin(), speedRegions_.end(),
        [&](const SpeedRegion& r) { return r.id == id; });

    if (it == speedRegions_.end()) return false;

    if (hasOverlap(speedRegions_, newStartMs, newEndMs, id)) return false;

    it->startMs = static_cast<int>(newStartMs);
    it->endMs = static_cast<int>(newEndMs);
    return true;
}

bool TimelineModel::updateSpeedValue(const std::string& id,
                                     PlaybackSpeed speed) {
    auto it = std::find_if(speedRegions_.begin(), speedRegions_.end(),
        [&](const SpeedRegion& r) { return r.id == id; });

    if (it == speedRegions_.end()) return false;

    it->speed = speed;
    return true;
}

// ---------------------------------------------------------------------------
// Annotation regions
// ---------------------------------------------------------------------------

const std::vector<AnnotationRegion>& TimelineModel::annotationRegions() const {
    return annotationRegions_;
}

AnnotationRegion& TimelineModel::addAnnotationRegion(int64_t startMs,
                                                     int64_t endMs,
                                                     AnnotationType type) {
    if (startMs >= endMs) {
        throw std::invalid_argument("startMs must be less than endMs");
    }

    AnnotationRegion region;
    region.id = generateId("annotation", nextAnnotationId_);
    region.startMs = static_cast<int>(startMs);
    region.endMs = static_cast<int>(endMs);
    region.type = type;

    annotationRegions_.push_back(std::move(region));
    return annotationRegions_.back();
}

bool TimelineModel::removeAnnotationRegion(const std::string& id) {
    auto it = std::find_if(annotationRegions_.begin(),
                           annotationRegions_.end(),
        [&](const AnnotationRegion& r) { return r.id == id; });

    if (it == annotationRegions_.end()) return false;

    annotationRegions_.erase(it);
    return true;
}

bool TimelineModel::resizeAnnotationRegion(const std::string& id,
                                           int64_t newStartMs,
                                           int64_t newEndMs) {
    if (newStartMs >= newEndMs) return false;

    auto it = std::find_if(annotationRegions_.begin(),
                           annotationRegions_.end(),
        [&](const AnnotationRegion& r) { return r.id == id; });

    if (it == annotationRegions_.end()) return false;

    it->startMs = static_cast<int>(newStartMs);
    it->endMs = static_cast<int>(newEndMs);
    return true;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

std::vector<const ZoomRegion*> TimelineModel::activeZoomRegions(
    int64_t timeMs) const {
    std::vector<const ZoomRegion*> result;
    for (const auto& r : zoomRegions_) {
        if (timeMs >= r.startMs && timeMs < r.endMs) {
            result.push_back(&r);
        }
    }
    return result;
}

std::vector<const TrimRegion*> TimelineModel::activeTrimRegions(
    int64_t timeMs) const {
    std::vector<const TrimRegion*> result;
    for (const auto& r : trimRegions_) {
        if (timeMs >= r.startMs && timeMs < r.endMs) {
            result.push_back(&r);
        }
    }
    return result;
}

bool TimelineModel::isTimeTrimmed(int64_t timeMs) const {
    return std::any_of(trimRegions_.begin(), trimRegions_.end(),
        [&](const TrimRegion& r) {
            return timeMs >= r.startMs && timeMs < r.endMs;
        });
}

const SpeedRegion* TimelineModel::activeSpeedRegion(int64_t timeMs) const {
    auto it = std::find_if(speedRegions_.begin(), speedRegions_.end(),
        [&](const SpeedRegion& r) {
            return timeMs >= r.startMs && timeMs < r.endMs;
        });

    return (it != speedRegions_.end()) ? &(*it) : nullptr;
}

std::vector<const AnnotationRegion*> TimelineModel::activeAnnotationRegions(
    int64_t timeMs) const {
    std::vector<const AnnotationRegion*> result;
    for (const auto& r : annotationRegions_) {
        if (timeMs >= r.startMs && timeMs < r.endMs) {
            result.push_back(&r);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Bulk operations
// ---------------------------------------------------------------------------

void TimelineModel::setZoomRegions(std::vector<ZoomRegion> regions) {
    zoomRegions_ = std::move(regions);
}

void TimelineModel::setTrimRegions(std::vector<TrimRegion> regions) {
    trimRegions_ = std::move(regions);
}

void TimelineModel::setSpeedRegions(std::vector<SpeedRegion> regions) {
    speedRegions_ = std::move(regions);
}

void TimelineModel::setAnnotationRegions(
    std::vector<AnnotationRegion> regions) {
    annotationRegions_ = std::move(regions);
}

void TimelineModel::clear() {
    zoomRegions_.clear();
    trimRegions_.clear();
    speedRegions_.clear();
    annotationRegions_.clear();

    nextZoomId_ = 1;
    nextTrimId_ = 1;
    nextSpeedId_ = 1;
    nextAnnotationId_ = 1;
}

} // namespace openscreen
