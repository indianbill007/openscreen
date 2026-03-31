#include <gtest/gtest.h>

#include "core/AspectRatio.h"
#include "core/ZoomTransform.h" // for Size

using namespace openscreen;

// ---------------------------------------------------------------------------
// getAspectRatioValue
// ---------------------------------------------------------------------------

TEST(AspectRatio, GetAspectRatioValue_16_9) {
    EXPECT_NEAR(getAspectRatioValue(AspectRatio::R16_9), 16.0 / 9.0, 1e-6);
}

TEST(AspectRatio, GetAspectRatioValue_9_16) {
    EXPECT_NEAR(getAspectRatioValue(AspectRatio::R9_16), 9.0 / 16.0, 1e-6);
}

TEST(AspectRatio, GetAspectRatioValue_1_1) {
    EXPECT_NEAR(getAspectRatioValue(AspectRatio::R1_1), 1.0, 1e-6);
}

TEST(AspectRatio, GetAspectRatioValue_Native) {
    // Native falls back to 16:9
    EXPECT_NEAR(getAspectRatioValue(AspectRatio::Native), 16.0 / 9.0, 1e-6);
}

// ---------------------------------------------------------------------------
// getNativeAspectRatioValue
// ---------------------------------------------------------------------------

TEST(AspectRatio, GetNativeAspectRatioValue_NoCrop) {
    const double result = getNativeAspectRatioValue(1920.0, 1080.0);
    EXPECT_NEAR(result, 16.0 / 9.0, 1e-6);
}

TEST(AspectRatio, GetNativeAspectRatioValue_WithCrop) {
    const CropRegion crop{0.0, 0.0, 0.5, 1.0};
    const double result = getNativeAspectRatioValue(1920.0, 1080.0, &crop);
    // (1920 * 0.5) / (1080 * 1.0) = 960 / 1080
    EXPECT_NEAR(result, 960.0 / 1080.0, 1e-6);
}

// ---------------------------------------------------------------------------
// getAspectRatioDimensions
// ---------------------------------------------------------------------------

TEST(AspectRatio, GetAspectRatioDimensions) {
    const auto dims = getAspectRatioDimensions(AspectRatio::R16_9, 1920.0);
    EXPECT_NEAR(dims.width, 1920.0, 1e-6);
    EXPECT_NEAR(dims.height, 1080.0, 1e-6);
}

// ---------------------------------------------------------------------------
// getAspectRatioLabel
// ---------------------------------------------------------------------------

TEST(AspectRatio, GetAspectRatioLabel_Native) {
    EXPECT_EQ(getAspectRatioLabel(AspectRatio::Native), "Native");
}

TEST(AspectRatio, GetAspectRatioLabel_16_9) {
    EXPECT_EQ(getAspectRatioLabel(AspectRatio::R16_9), "16:9");
}

// ---------------------------------------------------------------------------
// isPortraitAspectRatio
// ---------------------------------------------------------------------------

TEST(AspectRatio, IsPortraitAspectRatio_16_9) {
    EXPECT_FALSE(isPortraitAspectRatio(AspectRatio::R16_9));
}

TEST(AspectRatio, IsPortraitAspectRatio_9_16) {
    EXPECT_TRUE(isPortraitAspectRatio(AspectRatio::R9_16));
}

TEST(AspectRatio, IsPortraitAspectRatio_1_1) {
    // 1.0 is not < 1.0, so not portrait
    EXPECT_FALSE(isPortraitAspectRatio(AspectRatio::R1_1));
}
