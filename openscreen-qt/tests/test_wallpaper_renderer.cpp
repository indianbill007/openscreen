#include <gtest/gtest.h>
#include <QCoreApplication>
#include "render/WallpaperRenderer.h"

using namespace openscreen;

// ---------------------------------------------------------------------------
// QCoreApplication fixture (needed for QImage / QColor)
// ---------------------------------------------------------------------------

class WallpaperRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char* argv[] = {(char*)"test"};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
    std::unique_ptr<QCoreApplication> app_;
};

// ---------------------------------------------------------------------------
// classify()
// ---------------------------------------------------------------------------

TEST_F(WallpaperRendererTest, Classify_SolidColor_Hex) {
    EXPECT_EQ(WallpaperRenderer::classify("#ff0000"),
              WallpaperRenderer::WallpaperType::SolidColor);
}

TEST_F(WallpaperRendererTest, Classify_SolidColor_Named) {
    // QColor::isValidColorName("red") returns true, so classify should
    // recognise named CSS colours as SolidColor.
    EXPECT_EQ(WallpaperRenderer::classify("red"),
              WallpaperRenderer::WallpaperType::SolidColor);
}

TEST_F(WallpaperRendererTest, Classify_Gradient_Linear) {
    EXPECT_EQ(WallpaperRenderer::classify("linear-gradient(to right, #000, #fff)"),
              WallpaperRenderer::WallpaperType::Gradient);
}

TEST_F(WallpaperRendererTest, Classify_Gradient_Radial) {
    EXPECT_EQ(WallpaperRenderer::classify("radial-gradient(circle, #000, #fff)"),
              WallpaperRenderer::WallpaperType::Gradient);
}

TEST_F(WallpaperRendererTest, Classify_Image_Path) {
    EXPECT_EQ(WallpaperRenderer::classify("/wallpapers/wallpaper1.jpg"),
              WallpaperRenderer::WallpaperType::Image);
}

TEST_F(WallpaperRendererTest, Classify_Image_Resource) {
    EXPECT_EQ(WallpaperRenderer::classify(":/wallpapers/wallpaper1.jpg"),
              WallpaperRenderer::WallpaperType::Image);
}

// ---------------------------------------------------------------------------
// parseColor()
// ---------------------------------------------------------------------------

TEST_F(WallpaperRendererTest, ParseColor_Hex) {
    const QColor c = WallpaperRenderer::parseColor("#ff0000");
    EXPECT_EQ(c.red(),   255);
    EXPECT_EQ(c.green(), 0);
    EXPECT_EQ(c.blue(),  0);
}

TEST_F(WallpaperRendererTest, ParseColor_Hex_Short) {
    const QColor c = WallpaperRenderer::parseColor("#f00");
    EXPECT_EQ(c.red(),   255);
    EXPECT_EQ(c.green(), 0);
    EXPECT_EQ(c.blue(),  0);
}

// ---------------------------------------------------------------------------
// parseGradient()
// ---------------------------------------------------------------------------

TEST_F(WallpaperRendererTest, ParseGradient_Linear_ToRight) {
    auto result = WallpaperRenderer::parseGradient(
        "linear-gradient(to right, #000, #fff)");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ParsedGradient::Linear);
    EXPECT_NEAR(result->angleDeg, 90.0, 1e-3);
    EXPECT_EQ(result->stops.size(), 2u);
}

TEST_F(WallpaperRendererTest, ParseGradient_Linear_Degrees) {
    auto result = WallpaperRenderer::parseGradient(
        "linear-gradient(45deg, #000, #fff)");
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->angleDeg, 45.0, 1e-3);
}

TEST_F(WallpaperRendererTest, ParseGradient_Linear_DefaultAngle) {
    auto result = WallpaperRenderer::parseGradient(
        "linear-gradient(#000, #fff)");
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->angleDeg, 180.0, 1e-3);
}

TEST_F(WallpaperRendererTest, ParseGradient_Radial) {
    auto result = WallpaperRenderer::parseGradient(
        "radial-gradient(circle, #000, #fff)");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, ParsedGradient::Radial);
    EXPECT_EQ(result->stops.size(), 2u);
}

TEST_F(WallpaperRendererTest, ParseGradient_WithPercentages) {
    auto result = WallpaperRenderer::parseGradient(
        "linear-gradient(#000 0%, #fff 100%)");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->stops.size(), 2u);
    EXPECT_NEAR(result->stops[0].offset, 0.0, 1e-3);
    EXPECT_NEAR(result->stops[1].offset, 1.0, 1e-3);
}

TEST_F(WallpaperRendererTest, ParseGradient_ThreeStops) {
    auto result = WallpaperRenderer::parseGradient(
        "linear-gradient(#000, #888, #fff)");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->stops.size(), 3u);
    // Middle stop should be auto-distributed at ~0.5
    EXPECT_NEAR(result->stops[1].offset, 0.5, 0.05);
}

TEST_F(WallpaperRendererTest, ParseGradient_Invalid) {
    auto result = WallpaperRenderer::parseGradient("not-a-gradient");
    EXPECT_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// renderSolidColor()
// ---------------------------------------------------------------------------

TEST_F(WallpaperRendererTest, RenderSolidColor) {
    const QImage img = WallpaperRenderer::renderSolidColor(Qt::red, 100, 100);
    EXPECT_FALSE(img.isNull());
    EXPECT_EQ(img.width(),  100);
    EXPECT_EQ(img.height(), 100);
}

// ---------------------------------------------------------------------------
// renderGradient()
// ---------------------------------------------------------------------------

TEST_F(WallpaperRendererTest, RenderGradient_NotNull) {
    ParsedGradient grad;
    grad.type = ParsedGradient::Linear;
    grad.angleDeg = 90.0;
    grad.stops = {{Qt::black, 0.0}, {Qt::white, 1.0}};

    const QImage img = WallpaperRenderer::renderGradient(grad, 200, 100);
    EXPECT_FALSE(img.isNull());
    EXPECT_EQ(img.width(),  200);
    EXPECT_EQ(img.height(), 100);
}

// ---------------------------------------------------------------------------
// builtinWallpapers()
// ---------------------------------------------------------------------------

TEST_F(WallpaperRendererTest, BuiltinWallpapers_Count) {
    const auto wallpapers = WallpaperRenderer::builtinWallpapers();
    EXPECT_EQ(wallpapers.size(), 18u);
}

TEST_F(WallpaperRendererTest, BuiltinWallpapers_Paths) {
    const auto wallpapers = WallpaperRenderer::builtinWallpapers();
    ASSERT_GE(wallpapers.size(), 18u);
    EXPECT_EQ(wallpapers.front(), ":/wallpapers/wallpaper1.jpg");
    EXPECT_EQ(wallpapers.back(),  ":/wallpapers/wallpaper18.jpg");
}
