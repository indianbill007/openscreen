#include "CursorTelemetry.h"

#include <algorithm>
#include <cmath>

namespace openscreen {

// ---------------------------------------------------------------------------
// normalizeTelemetrySample
// ---------------------------------------------------------------------------

CursorTelemetryPoint normalizeTelemetrySample(const CursorTelemetryPoint& sample,
                                               int64_t totalMs) {
    CursorTelemetryPoint result;
    result.timeMs = static_cast<int>(clamp(
        static_cast<double>(sample.timeMs), 0.0, static_cast<double>(totalMs)));
    result.cx = clamp(sample.cx, 0.0, 1.0);
    result.cy = clamp(sample.cy, 0.0, 1.0);
    return result;
}

// ---------------------------------------------------------------------------
// normalizeCursorTelemetry
// ---------------------------------------------------------------------------

std::vector<CursorTelemetryPoint> normalizeCursorTelemetry(
    std::vector<CursorTelemetryPoint> telemetry,
    int64_t totalMs) {

    // Filter out samples with non-finite values.
    std::vector<CursorTelemetryPoint> filtered;
    filtered.reserve(telemetry.size());
    for (const auto& sample : telemetry) {
        if (std::isfinite(static_cast<double>(sample.timeMs)) &&
            std::isfinite(sample.cx) &&
            std::isfinite(sample.cy)) {
            filtered.push_back(sample);
        }
    }

    // Sort by timeMs.
    std::sort(filtered.begin(), filtered.end(),
              [](const CursorTelemetryPoint& a, const CursorTelemetryPoint& b) {
                  return a.timeMs < b.timeMs;
              });

    // Normalize each sample.
    std::vector<CursorTelemetryPoint> result;
    result.reserve(filtered.size());
    for (const auto& sample : filtered) {
        result.push_back(normalizeTelemetrySample(sample, totalMs));
    }

    return result;
}

// ---------------------------------------------------------------------------
// detectZoomDwellCandidates
// ---------------------------------------------------------------------------

std::vector<ZoomDwellCandidate> detectZoomDwellCandidates(
    const std::vector<CursorTelemetryPoint>& samples) {

    if (samples.size() < 2) {
        return {};
    }

    std::vector<ZoomDwellCandidate> dwellCandidates;
    size_t runStart = 0;

    auto pushRunIfDwell = [&](size_t startIndex, size_t endIndexExclusive) {
        if (endIndexExclusive - startIndex < 2) {
            return;
        }

        const auto& start = samples[startIndex];
        const auto& end = samples[endIndexExclusive - 1];
        int64_t runDuration = static_cast<int64_t>(end.timeMs) -
                              static_cast<int64_t>(start.timeMs);

        if (runDuration < MIN_DWELL_DURATION_MS || runDuration > MAX_DWELL_DURATION_MS) {
            return;
        }

        // Compute average cx/cy over the run.
        double sumCx = 0.0;
        double sumCy = 0.0;
        size_t count = endIndexExclusive - startIndex;
        for (size_t i = startIndex; i < endIndexExclusive; ++i) {
            sumCx += samples[i].cx;
            sumCy += samples[i].cy;
        }
        double avgCx = sumCx / static_cast<double>(count);
        double avgCy = sumCy / static_cast<double>(count);

        int64_t centerTime = static_cast<int64_t>(
            std::round((static_cast<double>(start.timeMs) +
                        static_cast<double>(end.timeMs)) / 2.0));

        ZoomDwellCandidate candidate;
        candidate.centerTimeMs = centerTime;
        candidate.focus = ZoomFocus{avgCx, avgCy};
        candidate.strength = static_cast<double>(runDuration);
        dwellCandidates.push_back(candidate);
    };

    for (size_t i = 1; i < samples.size(); ++i) {
        const auto& prev = samples[i - 1];
        const auto& curr = samples[i];
        double dx = curr.cx - prev.cx;
        double dy = curr.cy - prev.cy;
        double distance = std::hypot(dx, dy);

        if (distance > DWELL_MOVE_THRESHOLD) {
            pushRunIfDwell(runStart, i);
            runStart = i;
        }
    }

    // Process the final run.
    pushRunIfDwell(runStart, samples.size());

    return dwellCandidates;
}

} // namespace openscreen
