#include <gtest/gtest.h>
#include "core/CompositeLayout.h"
#include "core/ZoomTransform.h" // for Size

using namespace openscreen;

// ---------------------------------------------------------------------------
// computeCompositeLayout — PiP tests
// ---------------------------------------------------------------------------

TEST(CompositeLayoutTest, InvalidCanvas_ReturnsNullopt) {
    const Size canvas{0, 0};
    const Size screen{1920, 1080};
    auto result = computeCompositeLayout(canvas, screen, std::nullopt);
    EXPECT_FALSE(result.has_value());
}

TEST(CompositeLayoutTest, InvalidScreen_ReturnsNullopt) {
    const Size canvas{1920, 1080};
    const Size screen{0, 0};
    auto result = computeCompositeLayout(canvas, screen, std::nullopt);
    EXPECT_FALSE(result.has_value());
}

TEST(CompositeLayoutTest, PiP_NoWebcam) {
    const Size canvas{1920, 1080};
    const Size screen{1920, 1080};
    auto result = computeCompositeLayout(canvas, screen, std::nullopt);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->webcamRect.has_value());

    // Screen rect should cover the canvas area.
    const auto& sr = result->screenRect;
    EXPECT_NEAR(sr.x + sr.width, canvas.width, 1.0);
    EXPECT_NEAR(sr.y + sr.height, canvas.height, 1.0);
}

TEST(CompositeLayoutTest, PiP_WithWebcam) {
    const Size canvas{1920, 1080};
    const Size screen{1920, 1080};
    const Size webcam{1280, 720};
    auto result = computeCompositeLayout(
        canvas, screen, webcam, WebcamLayoutPreset::PictureInPicture);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->webcamRect.has_value());

    const auto& wr = result->webcamRect.value();
    // Positioned in the bottom-right area of the canvas.
    EXPECT_GT(wr.x, canvas.width / 2.0);
    EXPECT_GT(wr.y, canvas.height / 2.0);
    // Has a non-zero border radius.
    EXPECT_GT(wr.borderRadius, 0.0);
}

TEST(CompositeLayoutTest, PiP_WebcamSizeRespected) {
    const Size canvas{1920, 1080};
    const Size screen{1920, 1080};
    const Size webcam{1280, 720}; // 16:9
    auto result = computeCompositeLayout(
        canvas, screen, webcam, WebcamLayoutPreset::PictureInPicture);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->webcamRect.has_value());

    const auto& wr = result->webcamRect.value();
    const double webcamAspect = 1280.0 / 720.0;
    const double renderedAspect = wr.width / wr.height;
    EXPECT_NEAR(renderedAspect, webcamAspect, 0.05);
}

TEST(CompositeLayoutTest, PiP_CustomPosition) {
    const Size canvas{1920, 1080};
    const Size screen{1920, 1080};
    const Size webcam{1280, 720};
    const WebcamPosition pos{0.5, 0.5};
    auto result = computeCompositeLayout(
        canvas, screen, webcam, WebcamLayoutPreset::PictureInPicture, pos);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->webcamRect.has_value());

    const auto& wr = result->webcamRect.value();
    // The webcam center should be near the center of the canvas.
    const double centerX = wr.x + wr.width / 2.0;
    const double centerY = wr.y + wr.height / 2.0;
    EXPECT_NEAR(centerX, canvas.width / 2.0, 2.0);
    EXPECT_NEAR(centerY, canvas.height / 2.0, 2.0);
}

TEST(CompositeLayoutTest, PiP_CustomPosition_Clamped) {
    const Size canvas{1920, 1080};
    const Size screen{1920, 1080};
    const Size webcam{1280, 720};
    const WebcamPosition pos{0.0, 0.0};
    auto result = computeCompositeLayout(
        canvas, screen, webcam, WebcamLayoutPreset::PictureInPicture, pos);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->webcamRect.has_value());

    const auto& wr = result->webcamRect.value();
    EXPECT_GE(wr.x, 0.0);
    EXPECT_GE(wr.y, 0.0);
}

// ---------------------------------------------------------------------------
// computeCompositeLayout — Stack tests
// ---------------------------------------------------------------------------

TEST(CompositeLayoutTest, Stack_NoWebcam) {
    const Size canvas{1920, 1080};
    const Size screen{1920, 1080};
    auto result = computeCompositeLayout(
        canvas, screen, std::nullopt, WebcamLayoutPreset::VerticalStack);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->webcamRect.has_value());
    EXPECT_TRUE(result->screenCover);

    const auto& sr = result->screenRect;
    EXPECT_NEAR(sr.x, 0.0, 1.0);
    EXPECT_NEAR(sr.y, 0.0, 1.0);
    EXPECT_NEAR(sr.width, canvas.width, 1.0);
    EXPECT_NEAR(sr.height, canvas.height, 1.0);
}

TEST(CompositeLayoutTest, Stack_WithWebcam) {
    const Size canvas{1920, 1080};
    const Size screen{1920, 1080};
    const Size webcam{1280, 720};
    auto result = computeCompositeLayout(
        canvas, screen, webcam, WebcamLayoutPreset::VerticalStack);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->webcamRect.has_value());
    EXPECT_TRUE(result->screenCover);

    const auto& sr = result->screenRect;
    const auto& wr = result->webcamRect.value();

    // Screen fills top, webcam at bottom.
    EXPECT_NEAR(sr.x, 0.0, 1.0);
    EXPECT_NEAR(sr.y, 0.0, 1.0);
    EXPECT_NEAR(wr.x, 0.0, 1.0);
    EXPECT_NEAR(wr.width, canvas.width, 1.0);
    // Webcam y starts at the bottom of the screen rect.
    EXPECT_NEAR(wr.y, sr.height, 1.0);
}

TEST(CompositeLayoutTest, Stack_WebcamHeight) {
    const Size canvas{1920, 1080};
    const Size screen{1920, 1080};
    const Size webcam{1280, 720}; // 16:9
    auto result = computeCompositeLayout(
        canvas, screen, webcam, WebcamLayoutPreset::VerticalStack);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->webcamRect.has_value());

    const auto& wr = result->webcamRect.value();
    // Webcam height = canvas width / webcam aspect ratio.
    const double expectedHeight = std::round(canvas.width / (1280.0 / 720.0));
    EXPECT_NEAR(wr.height, expectedHeight, 1.0);
}

// ---------------------------------------------------------------------------
// getWebcamLayoutPresetDefinition
// ---------------------------------------------------------------------------

TEST(CompositeLayoutTest, GetPresetDefinition_PiP) {
    const auto& def = getWebcamLayoutPresetDefinition(
        WebcamLayoutPreset::PictureInPicture);
    EXPECT_EQ(def.label, "Picture in Picture");
    EXPECT_FALSE(def.isStack);
    EXPECT_TRUE(def.shadow.has_value());
}

TEST(CompositeLayoutTest, GetPresetDefinition_Stack) {
    const auto& def = getWebcamLayoutPresetDefinition(
        WebcamLayoutPreset::VerticalStack);
    EXPECT_EQ(def.label, "Vertical Stack");
    EXPECT_TRUE(def.isStack);
    EXPECT_FALSE(def.shadow.has_value());
}
