#include <gtest/gtest.h>

#include "core/ZoomTransform.h"

using namespace openscreen;

// ---------------------------------------------------------------------------
// computeZoomTransform
// ---------------------------------------------------------------------------

TEST(ZoomTransform, ComputeZoomTransform_NoZoom) {
    const Size stage{800.0, 600.0};
    const Rect mask{0.0, 0.0, 800.0, 600.0};
    const auto t = computeZoomTransform(stage, mask, 1.0, 1.0, 0.5, 0.5);

    EXPECT_NEAR(t.scale, 1.0, 1e-6);
    EXPECT_NEAR(t.x, 0.0, 1e-6);
    EXPECT_NEAR(t.y, 0.0, 1e-6);
}

TEST(ZoomTransform, ComputeZoomTransform_Centered) {
    const Size stage{800.0, 600.0};
    const Rect mask{0.0, 0.0, 800.0, 600.0};
    const auto t = computeZoomTransform(stage, mask, 2.0, 1.0, 0.5, 0.5);

    // focusStagePxX = 0 + 0.5*800 = 400; stageCenterX = 400
    // finalX = 400 - 400*2 = -400; returned x = -400*1 = -400
    EXPECT_NEAR(t.scale, 2.0, 1e-6);
    EXPECT_NEAR(t.x, -400.0, 1e-6);
    EXPECT_NEAR(t.y, -300.0, 1e-6);
}

TEST(ZoomTransform, ComputeZoomTransform_ZeroSize) {
    const Size stage{0.0, 0.0};
    const Rect mask{0.0, 0.0, 800.0, 600.0};
    const auto t = computeZoomTransform(stage, mask, 2.0, 1.0, 0.5, 0.5);

    EXPECT_NEAR(t.scale, 1.0, 1e-6);
    EXPECT_NEAR(t.x, 0.0, 1e-6);
    EXPECT_NEAR(t.y, 0.0, 1e-6);
}

TEST(ZoomTransform, ComputeZoomTransform_Progress) {
    const Size stage{800.0, 600.0};
    const Rect mask{0.0, 0.0, 800.0, 600.0};
    const auto t = computeZoomTransform(stage, mask, 2.0, 0.5, 0.5, 0.5);

    // scale = 1 + (2-1)*0.5 = 1.5
    // finalX = 400 - 400*2 = -400; returned x = -400*0.5 = -200
    // finalY = 300 - 300*2 = -300; returned y = -300*0.5 = -150
    EXPECT_NEAR(t.scale, 1.5, 1e-6);
    EXPECT_NEAR(t.x, -200.0, 1e-6);
    EXPECT_NEAR(t.y, -150.0, 1e-6);
}

// ---------------------------------------------------------------------------
// computeFocusFromTransform
// ---------------------------------------------------------------------------

TEST(ZoomTransform, ComputeFocusFromTransform_Inverse) {
    const Size stage{800.0, 600.0};
    const Rect mask{0.0, 0.0, 800.0, 600.0};
    const double zoomScale = 2.0;
    const double focusX = 0.5;
    const double focusY = 0.5;

    const auto t = computeZoomTransform(stage, mask, zoomScale, 1.0, focusX, focusY);
    const auto recovered = computeFocusFromTransform(stage, mask, zoomScale, t.x, t.y);

    EXPECT_NEAR(recovered.cx, focusX, 1e-6);
    EXPECT_NEAR(recovered.cy, focusY, 1e-6);
}

TEST(ZoomTransform, ComputeFocusFromTransform_ZeroScale) {
    const Size stage{800.0, 600.0};
    const Rect mask{0.0, 0.0, 800.0, 600.0};
    const auto focus = computeFocusFromTransform(stage, mask, 0.0, 100.0, 100.0);

    EXPECT_NEAR(focus.cx, 0.5, 1e-6);
    EXPECT_NEAR(focus.cy, 0.5, 1e-6);
}

// ---------------------------------------------------------------------------
// getMotionBlurAmountResponse
// ---------------------------------------------------------------------------

TEST(ZoomTransform, MotionBlurAmountResponse_Zero) {
    EXPECT_NEAR(getMotionBlurAmountResponse(0.0), 0.0, 1e-6);
}

TEST(ZoomTransform, MotionBlurAmountResponse_One) {
    // 1 * (1 + (2.2 - 1) * 1) = 1 * 2.2 = 2.2
    EXPECT_NEAR(getMotionBlurAmountResponse(1.0), 2.2, 1e-6);
}

TEST(ZoomTransform, MotionBlurAmountResponse_Clamped) {
    // Input 1.5 is clamped to 1.0, so result == response(1.0) = 2.2
    EXPECT_NEAR(getMotionBlurAmountResponse(1.5), 2.2, 1e-6);
}

// ---------------------------------------------------------------------------
// computeMotionBlur
// ---------------------------------------------------------------------------

TEST(ZoomTransform, ComputeMotionBlur_FirstFrame) {
    MotionBlurState state{};
    const AppliedTransform transform{2.0, -400.0, -300.0};
    const Size stage{800.0, 600.0};

    const auto result = computeMotionBlur(state, transform, stage, 1.0, 16.0);

    EXPECT_TRUE(state.initialized);
    EXPECT_NEAR(result.velocityX, 0.0, 1e-6);
    EXPECT_NEAR(result.velocityY, 0.0, 1e-6);
    EXPECT_NEAR(result.targetBlur, 0.0, 1e-6);
    EXPECT_NEAR(result.kernelSize, 0.0, 1e-6);
}

TEST(ZoomTransform, ComputeMotionBlur_Stationary) {
    MotionBlurState state{};
    const AppliedTransform transform{2.0, -400.0, -300.0};
    const Size stage{800.0, 600.0};

    // First frame initializes state
    computeMotionBlur(state, transform, stage, 1.0, 0.0);

    // Second frame with same transform
    const auto result = computeMotionBlur(state, transform, stage, 1.0, 16.0);

    EXPECT_NEAR(result.targetBlur, 0.0, 1e-6);
}

TEST(ZoomTransform, ComputeMotionBlur_Moving) {
    MotionBlurState state{};
    const Size stage{800.0, 600.0};

    // First frame
    const AppliedTransform t1{2.0, 0.0, 0.0};
    computeMotionBlur(state, t1, stage, 1.0, 0.0);

    // Second frame with large displacement
    const AppliedTransform t2{2.0, 500.0, 500.0};
    const auto result = computeMotionBlur(state, t2, stage, 1.0, 16.0);

    EXPECT_GT(result.targetBlur, 0.0);
}

// ---------------------------------------------------------------------------
// easeIntoBoundary
// ---------------------------------------------------------------------------

TEST(ZoomTransform, EaseIntoBoundary_Zero) {
    EXPECT_NEAR(easeIntoBoundary(0.0), 0.0, 1e-6);
}

TEST(ZoomTransform, EaseIntoBoundary_One) {
    // -1 + 2 = 1
    EXPECT_NEAR(easeIntoBoundary(1.0), 1.0, 1e-6);
}

TEST(ZoomTransform, EaseIntoBoundary_Half) {
    // -0.5^3 + 2*0.5^2 = -0.125 + 0.5 = 0.375
    EXPECT_NEAR(easeIntoBoundary(0.5), 0.375, 1e-6);
}

// ---------------------------------------------------------------------------
// getFocusBoundsForScale
// ---------------------------------------------------------------------------

TEST(ZoomTransform, GetFocusBoundsForScale_2x) {
    // marginX = 1/(2*2) = 0.25
    const auto bounds = getFocusBoundsForScale(2.0);

    EXPECT_NEAR(bounds.minX, 0.25, 1e-6);
    EXPECT_NEAR(bounds.maxX, 0.75, 1e-6);
    EXPECT_NEAR(bounds.minY, 0.25, 1e-6);
    EXPECT_NEAR(bounds.maxY, 0.75, 1e-6);
}

// ---------------------------------------------------------------------------
// clampFocusToStage
// ---------------------------------------------------------------------------

TEST(ZoomTransform, ClampFocusToStage_InBounds) {
    const ZoomFocus focus{0.5, 0.5};
    const auto result = clampFocusToStage(focus, ZoomDepth::D3);

    EXPECT_NEAR(result.cx, 0.5, 1e-6);
    EXPECT_NEAR(result.cy, 0.5, 1e-6);
}

TEST(ZoomTransform, ClampFocusToStage_OutOfBounds) {
    const ZoomFocus focus{0.0, 0.0};
    const auto result = clampFocusToStage(focus, ZoomDepth::D3);

    // D3 scale = 1.80; bounds margin = 1/(2*1.8) ≈ 0.2778
    const double margin = 1.0 / (2.0 * 1.80);
    EXPECT_NEAR(result.cx, margin, 1e-6);
    EXPECT_NEAR(result.cy, margin, 1e-6);
}

// ---------------------------------------------------------------------------
// clampFocusToScale
// ---------------------------------------------------------------------------

TEST(ZoomTransform, ClampFocusToScale_InBounds) {
    const ZoomFocus focus{0.5, 0.5};
    const auto result = clampFocusToScale(focus, 2.0);

    EXPECT_NEAR(result.cx, 0.5, 1e-6);
    EXPECT_NEAR(result.cy, 0.5, 1e-6);
}

// ---------------------------------------------------------------------------
// softenFocusToScale
// ---------------------------------------------------------------------------

TEST(ZoomTransform, SoftenFocusToScale) {
    // At scale 2, bounds are [0.25, 0.75].  Place focus right at the boundary.
    const ZoomFocus focus{0.25, 0.75};
    const auto result = softenFocusToScale(focus, 2.0);

    // The softening should push values inward from the boundary edges.
    EXPECT_GT(result.cx, 0.25);
    EXPECT_LT(result.cy, 0.75);
}

// ---------------------------------------------------------------------------
// stageFocusToVideoSpace
// ---------------------------------------------------------------------------

TEST(ZoomTransform, StageFocusToVideoSpace_Identity) {
    const ZoomFocus focus{0.5, 0.5};
    const Size stage{1920.0, 1080.0};
    const Size video{1920.0, 1080.0};

    // Identity: scale=1, offset=0 → same focus
    const auto result = stageFocusToVideoSpace(focus, stage, video, 1.0, 0.0, 0.0);

    EXPECT_NEAR(result.cx, 0.5, 1e-6);
    EXPECT_NEAR(result.cy, 0.5, 1e-6);
}

TEST(ZoomTransform, StageFocusToVideoSpace_ZeroSize) {
    const ZoomFocus focus{0.3, 0.7};
    const Size stage{0.0, 0.0};
    const Size video{1920.0, 1080.0};

    const auto result = stageFocusToVideoSpace(focus, stage, video, 1.0, 0.0, 0.0);

    // Returns original focus when stage size is zero
    EXPECT_NEAR(result.cx, 0.3, 1e-6);
    EXPECT_NEAR(result.cy, 0.7, 1e-6);
}
