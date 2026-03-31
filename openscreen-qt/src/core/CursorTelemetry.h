#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "types.h"

namespace openscreen {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

inline constexpr int64_t MIN_DWELL_DURATION_MS = 450;
inline constexpr int64_t MAX_DWELL_DURATION_MS = 2600;
inline constexpr double DWELL_MOVE_THRESHOLD = 0.02;

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

struct ZoomDwellCandidate {
    int64_t centerTimeMs = 0;
    ZoomFocus focus;
    double strength = 0.0;
};

// ---------------------------------------------------------------------------
// Functions
// ---------------------------------------------------------------------------

/// Clamp a single telemetry sample's values to valid ranges.
CursorTelemetryPoint normalizeTelemetrySample(const CursorTelemetryPoint& sample,
                                               int64_t totalMs);

/// Filter out non-finite samples, sort by time, and normalize each sample.
std::vector<CursorTelemetryPoint> normalizeCursorTelemetry(
    std::vector<CursorTelemetryPoint> telemetry,
    int64_t totalMs);

/// Detect dwell candidates from cursor telemetry samples.
/// Identifies runs of closely-spaced samples (distance < DWELL_MOVE_THRESHOLD)
/// with duration in [MIN_DWELL_DURATION_MS, MAX_DWELL_DURATION_MS].
std::vector<ZoomDwellCandidate> detectZoomDwellCandidates(
    const std::vector<CursorTelemetryPoint>& samples);

} // namespace openscreen
