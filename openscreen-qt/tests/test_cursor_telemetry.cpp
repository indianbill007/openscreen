#include <gtest/gtest.h>
#include "core/CursorTelemetry.h"

#include <cmath>
#include <limits>
#include <vector>

using namespace openscreen;

// ---------------------------------------------------------------------------
// Helper: generate N evenly-spaced CursorTelemetryPoint samples.
// ---------------------------------------------------------------------------

static std::vector<CursorTelemetryPoint> makeSamples(
    int count, double cx, double cy, int startMs, int endMs) {
    std::vector<CursorTelemetryPoint> samples;
    samples.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        const double t = (count <= 1)
            ? 0.0
            : static_cast<double>(i) / static_cast<double>(count - 1);
        CursorTelemetryPoint pt;
        pt.timeMs = startMs + static_cast<int>(std::round(t * (endMs - startMs)));
        pt.cx = cx;
        pt.cy = cy;
        samples.push_back(pt);
    }
    return samples;
}

// ---------------------------------------------------------------------------
// normalizeTelemetrySample
// ---------------------------------------------------------------------------

TEST(CursorTelemetryTest, NormalizeSample_Clamps) {
    CursorTelemetryPoint sample;
    sample.timeMs = -100;
    sample.cx = 1.5;
    sample.cy = -0.5;

    auto result = normalizeTelemetrySample(sample, 5000);
    EXPECT_EQ(result.timeMs, 0);
    EXPECT_NEAR(result.cx, 1.0, 1e-9);
    EXPECT_NEAR(result.cy, 0.0, 1e-9);
}

TEST(CursorTelemetryTest, NormalizeSample_InRange) {
    CursorTelemetryPoint sample;
    sample.timeMs = 2500;
    sample.cx = 0.5;
    sample.cy = 0.3;

    auto result = normalizeTelemetrySample(sample, 5000);
    EXPECT_EQ(result.timeMs, 2500);
    EXPECT_NEAR(result.cx, 0.5, 1e-9);
    EXPECT_NEAR(result.cy, 0.3, 1e-9);
}

// ---------------------------------------------------------------------------
// normalizeCursorTelemetry
// ---------------------------------------------------------------------------

TEST(CursorTelemetryTest, NormalizeTelemetry_SortsByTime) {
    std::vector<CursorTelemetryPoint> telemetry;

    CursorTelemetryPoint p1; p1.timeMs = 300; p1.cx = 0.1; p1.cy = 0.1;
    CursorTelemetryPoint p2; p2.timeMs = 100; p2.cx = 0.2; p2.cy = 0.2;
    CursorTelemetryPoint p3; p3.timeMs = 200; p3.cx = 0.3; p3.cy = 0.3;
    telemetry.push_back(p1);
    telemetry.push_back(p2);
    telemetry.push_back(p3);

    auto result = normalizeCursorTelemetry(telemetry, 5000);
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0].timeMs, 100);
    EXPECT_EQ(result[1].timeMs, 200);
    EXPECT_EQ(result[2].timeMs, 300);
}

TEST(CursorTelemetryTest, NormalizeTelemetry_FiltersInvalid) {
    std::vector<CursorTelemetryPoint> telemetry;

    CursorTelemetryPoint valid; valid.timeMs = 100; valid.cx = 0.5; valid.cy = 0.5;
    CursorTelemetryPoint invalid;
    invalid.timeMs = 200;
    invalid.cx = std::numeric_limits<double>::quiet_NaN();
    invalid.cy = 0.5;

    telemetry.push_back(valid);
    telemetry.push_back(invalid);

    auto result = normalizeCursorTelemetry(telemetry, 5000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].timeMs, 100);
}

TEST(CursorTelemetryTest, NormalizeTelemetry_Empty) {
    std::vector<CursorTelemetryPoint> empty;
    auto result = normalizeCursorTelemetry(empty, 5000);
    EXPECT_TRUE(result.empty());
}

// ---------------------------------------------------------------------------
// detectZoomDwellCandidates
// ---------------------------------------------------------------------------

TEST(CursorTelemetryTest, DetectDwell_Empty) {
    std::vector<CursorTelemetryPoint> empty;
    auto result = detectZoomDwellCandidates(empty);
    EXPECT_TRUE(result.empty());
}

TEST(CursorTelemetryTest, DetectDwell_SingleSample) {
    auto samples = makeSamples(1, 0.5, 0.5, 0, 0);
    auto result = detectZoomDwellCandidates(samples);
    EXPECT_TRUE(result.empty());
}

TEST(CursorTelemetryTest, DetectDwell_StationaryCursor) {
    // 10 samples at the same position over 500ms (> MIN_DWELL=450).
    auto samples = makeSamples(10, 0.5, 0.5, 0, 500);
    auto result = detectZoomDwellCandidates(samples);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_NEAR(result[0].focus.cx, 0.5, 0.01);
    EXPECT_NEAR(result[0].focus.cy, 0.5, 0.01);
}

TEST(CursorTelemetryTest, DetectDwell_MovingCursor) {
    // Samples that move > DWELL_MOVE_THRESHOLD between each pair.
    std::vector<CursorTelemetryPoint> samples;
    for (int i = 0; i < 10; ++i) {
        CursorTelemetryPoint pt;
        pt.timeMs = i * 100;
        pt.cx = static_cast<double>(i) * 0.05; // 0.05 > 0.02 threshold
        pt.cy = 0.5;
        samples.push_back(pt);
    }
    auto result = detectZoomDwellCandidates(samples);
    EXPECT_EQ(result.size(), 0u);
}

TEST(CursorTelemetryTest, DetectDwell_TooShort) {
    // Stationary run of 300ms (< MIN_DWELL=450).
    auto samples = makeSamples(10, 0.5, 0.5, 0, 300);
    auto result = detectZoomDwellCandidates(samples);
    EXPECT_EQ(result.size(), 0u);
}

TEST(CursorTelemetryTest, DetectDwell_TooLong) {
    // Stationary run of 3000ms (> MAX_DWELL=2600).
    auto samples = makeSamples(10, 0.5, 0.5, 0, 3000);
    auto result = detectZoomDwellCandidates(samples);
    EXPECT_EQ(result.size(), 0u);
}

TEST(CursorTelemetryTest, DetectDwell_MultipleDwells) {
    // Two stationary runs separated by a large movement.
    auto run1 = makeSamples(10, 0.2, 0.2, 0, 500);
    auto run2 = makeSamples(10, 0.8, 0.8, 1000, 1500);

    std::vector<CursorTelemetryPoint> samples;
    samples.insert(samples.end(), run1.begin(), run1.end());
    // Insert a jump point between the two runs.
    CursorTelemetryPoint jump;
    jump.timeMs = 750;
    jump.cx = 0.5;
    jump.cy = 0.5;
    samples.push_back(jump);
    samples.insert(samples.end(), run2.begin(), run2.end());

    auto result = detectZoomDwellCandidates(samples);
    EXPECT_EQ(result.size(), 2u);
}

TEST(CursorTelemetryTest, DetectDwell_StrengthIsDuration) {
    // Stationary over exactly 600ms.
    auto samples = makeSamples(10, 0.5, 0.5, 100, 700);
    auto result = detectZoomDwellCandidates(samples);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_NEAR(result[0].strength, 600.0, 1e-9);
}

TEST(CursorTelemetryTest, DetectDwell_FocusIsAverage) {
    // Samples clustered around (0.5, 0.5) with small perturbations.
    std::vector<CursorTelemetryPoint> samples;
    const double offsets[] = {-0.005, 0.005, -0.003, 0.003, 0.0,
                              -0.004, 0.004, -0.002, 0.002, 0.001};
    for (int i = 0; i < 10; ++i) {
        CursorTelemetryPoint pt;
        pt.timeMs = i * 60; // 540ms total (> 450 threshold)
        pt.cx = 0.5 + offsets[i];
        pt.cy = 0.5 + offsets[i];
        samples.push_back(pt);
    }

    auto result = detectZoomDwellCandidates(samples);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_NEAR(result[0].focus.cx, 0.5, 0.01);
    EXPECT_NEAR(result[0].focus.cy, 0.5, 0.01);
}
