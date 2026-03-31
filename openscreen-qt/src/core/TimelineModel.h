#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "types.h"

namespace openscreen {

class TimelineModel {
public:
    // -- Zoom regions (overlaps allowed) ------------------------------------

    const std::vector<ZoomRegion>& zoomRegions() const;

    ZoomRegion& addZoomRegion(int64_t startMs, int64_t endMs,
                              ZoomDepth depth, ZoomFocus focus);

    bool removeZoomRegion(const std::string& id);

    bool resizeZoomRegion(const std::string& id,
                          int64_t newStartMs, int64_t newEndMs);

    bool updateZoomDepth(const std::string& id, ZoomDepth depth);

    bool updateZoomFocus(const std::string& id, ZoomFocus focus);

    // -- Trim regions (NO overlap allowed) ----------------------------------

    const std::vector<TrimRegion>& trimRegions() const;

    TrimRegion* addTrimRegion(int64_t startMs, int64_t endMs);

    bool removeTrimRegion(const std::string& id);

    bool resizeTrimRegion(const std::string& id,
                          int64_t newStartMs, int64_t newEndMs);

    // -- Speed regions (NO overlap allowed) ---------------------------------

    const std::vector<SpeedRegion>& speedRegions() const;

    SpeedRegion* addSpeedRegion(int64_t startMs, int64_t endMs,
                                PlaybackSpeed speed);

    bool removeSpeedRegion(const std::string& id);

    bool resizeSpeedRegion(const std::string& id,
                           int64_t newStartMs, int64_t newEndMs);

    bool updateSpeedValue(const std::string& id, PlaybackSpeed speed);

    // -- Annotation regions (overlaps allowed) ------------------------------

    const std::vector<AnnotationRegion>& annotationRegions() const;

    AnnotationRegion& addAnnotationRegion(int64_t startMs, int64_t endMs,
                                          AnnotationType type);

    bool removeAnnotationRegion(const std::string& id);

    bool resizeAnnotationRegion(const std::string& id,
                                int64_t newStartMs, int64_t newEndMs);

    // -- Queries ------------------------------------------------------------

    std::vector<const ZoomRegion*> activeZoomRegions(int64_t timeMs) const;

    std::vector<const TrimRegion*> activeTrimRegions(int64_t timeMs) const;

    bool isTimeTrimmed(int64_t timeMs) const;

    const SpeedRegion* activeSpeedRegion(int64_t timeMs) const;

    std::vector<const AnnotationRegion*> activeAnnotationRegions(
        int64_t timeMs) const;

    // -- Bulk operations ----------------------------------------------------

    void setZoomRegions(std::vector<ZoomRegion> regions);
    void setTrimRegions(std::vector<TrimRegion> regions);
    void setSpeedRegions(std::vector<SpeedRegion> regions);
    void setAnnotationRegions(std::vector<AnnotationRegion> regions);

    void clear();

private:
    // -- Helpers ------------------------------------------------------------

    bool hasOverlap(const std::vector<TrimRegion>& regions,
                    int64_t start, int64_t end,
                    const std::string& excludeId = "") const;

    bool hasOverlap(const std::vector<SpeedRegion>& regions,
                    int64_t start, int64_t end,
                    const std::string& excludeId = "") const;

    std::string generateId(const std::string& prefix, int& counter);

    // -- Data ---------------------------------------------------------------

    std::vector<ZoomRegion> zoomRegions_;
    std::vector<TrimRegion> trimRegions_;
    std::vector<SpeedRegion> speedRegions_;
    std::vector<AnnotationRegion> annotationRegions_;

    int nextZoomId_ = 1;
    int nextTrimId_ = 1;
    int nextSpeedId_ = 1;
    int nextAnnotationId_ = 1;
};

} // namespace openscreen
