#include <gtest/gtest.h>
#include "core/TimelineModel.h"

#include <stdexcept>
#include <string>

using namespace openscreen;

// ---------------------------------------------------------------------------
// TimelineModelTest
// ---------------------------------------------------------------------------

TEST(TimelineModelTest, Empty) {
    TimelineModel model;

    EXPECT_TRUE(model.zoomRegions().empty());
    EXPECT_TRUE(model.trimRegions().empty());
    EXPECT_TRUE(model.speedRegions().empty());
    EXPECT_TRUE(model.annotationRegions().empty());
}

// -- Zoom -------------------------------------------------------------------

TEST(TimelineModelTest, AddZoomRegion) {
    TimelineModel model;
    auto& zr = model.addZoomRegion(1000, 2000, ZoomDepth::D3, ZoomFocus{0.5, 0.5});

    EXPECT_EQ(zr.startMs, 1000);
    EXPECT_EQ(zr.endMs, 2000);
    EXPECT_EQ(zr.depth, ZoomDepth::D3);
    EXPECT_NEAR(zr.focus.cx, 0.5, 1e-9);
    EXPECT_NEAR(zr.focus.cy, 0.5, 1e-9);
    EXPECT_EQ(zr.id.substr(0, 5), "zoom-");
    EXPECT_EQ(model.zoomRegions().size(), 1u);
}

TEST(TimelineModelTest, RemoveZoomRegion) {
    TimelineModel model;
    auto& zr = model.addZoomRegion(0, 1000, ZoomDepth::D1, ZoomFocus{0.5, 0.5});
    std::string id = zr.id;

    EXPECT_TRUE(model.removeZoomRegion(id));
    EXPECT_TRUE(model.zoomRegions().empty());
}

TEST(TimelineModelTest, ResizeZoomRegion) {
    TimelineModel model;
    auto& zr = model.addZoomRegion(0, 1000, ZoomDepth::D2, ZoomFocus{0.5, 0.5});
    std::string id = zr.id;

    EXPECT_TRUE(model.resizeZoomRegion(id, 500, 1500));
    EXPECT_EQ(model.zoomRegions()[0].startMs, 500);
    EXPECT_EQ(model.zoomRegions()[0].endMs, 1500);
}

TEST(TimelineModelTest, UpdateZoomDepth) {
    TimelineModel model;
    auto& zr = model.addZoomRegion(0, 1000, ZoomDepth::D1, ZoomFocus{0.5, 0.5});
    std::string id = zr.id;

    EXPECT_TRUE(model.updateZoomDepth(id, ZoomDepth::D5));
    EXPECT_EQ(model.zoomRegions()[0].depth, ZoomDepth::D5);
}

TEST(TimelineModelTest, UpdateZoomFocus) {
    TimelineModel model;
    auto& zr = model.addZoomRegion(0, 1000, ZoomDepth::D3, ZoomFocus{0.5, 0.5});
    std::string id = zr.id;

    EXPECT_TRUE(model.updateZoomFocus(id, ZoomFocus{0.2, 0.8}));
    EXPECT_NEAR(model.zoomRegions()[0].focus.cx, 0.2, 1e-9);
    EXPECT_NEAR(model.zoomRegions()[0].focus.cy, 0.8, 1e-9);
}

TEST(TimelineModelTest, ZoomOverlapAllowed) {
    TimelineModel model;
    model.addZoomRegion(0, 1000, ZoomDepth::D1, ZoomFocus{0.5, 0.5});
    model.addZoomRegion(500, 1500, ZoomDepth::D2, ZoomFocus{0.5, 0.5});

    EXPECT_EQ(model.zoomRegions().size(), 2u);
}

// -- Trim -------------------------------------------------------------------

TEST(TimelineModelTest, AddTrimRegion) {
    TimelineModel model;
    auto* tr = model.addTrimRegion(0, 1000);

    ASSERT_NE(tr, nullptr);
    EXPECT_EQ(tr->startMs, 0);
    EXPECT_EQ(tr->endMs, 1000);
    EXPECT_EQ(model.trimRegions().size(), 1u);
}

TEST(TimelineModelTest, TrimNoOverlap) {
    TimelineModel model;
    model.addTrimRegion(0, 1000);

    auto* overlap = model.addTrimRegion(500, 1500);
    EXPECT_EQ(overlap, nullptr);
}

TEST(TimelineModelTest, TrimNoOverlap_Adjacent) {
    TimelineModel model;
    model.addTrimRegion(0, 1000);

    auto* adjacent = model.addTrimRegion(1000, 2000);
    EXPECT_NE(adjacent, nullptr);
    EXPECT_EQ(model.trimRegions().size(), 2u);
}

TEST(TimelineModelTest, RemoveTrimRegion) {
    TimelineModel model;
    auto* tr = model.addTrimRegion(0, 1000);
    std::string id = tr->id;

    EXPECT_TRUE(model.removeTrimRegion(id));
    EXPECT_TRUE(model.trimRegions().empty());
}

TEST(TimelineModelTest, ResizeTrimRegion) {
    TimelineModel model;
    auto* tr = model.addTrimRegion(0, 1000);
    std::string id = tr->id;

    EXPECT_TRUE(model.resizeTrimRegion(id, 0, 1500));
    EXPECT_EQ(model.trimRegions()[0].endMs, 1500);
}

TEST(TimelineModelTest, ResizeTrimRegion_Overlap) {
    TimelineModel model;
    auto* t1 = model.addTrimRegion(0, 1000);
    model.addTrimRegion(2000, 3000);
    std::string id = t1->id;

    // Resize first region to overlap with second
    EXPECT_FALSE(model.resizeTrimRegion(id, 0, 2500));
    // Original size unchanged
    EXPECT_EQ(model.trimRegions()[0].endMs, 1000);
}

// -- Speed ------------------------------------------------------------------

TEST(TimelineModelTest, AddSpeedRegion) {
    TimelineModel model;
    auto* sr = model.addSpeedRegion(0, 1000, PlaybackSpeed::X2_00);

    ASSERT_NE(sr, nullptr);
    EXPECT_EQ(sr->startMs, 0);
    EXPECT_EQ(sr->endMs, 1000);
    EXPECT_EQ(sr->speed, PlaybackSpeed::X2_00);
}

TEST(TimelineModelTest, SpeedNoOverlap) {
    TimelineModel model;
    model.addSpeedRegion(0, 1000, PlaybackSpeed::X1_50);

    auto* overlap = model.addSpeedRegion(500, 1500, PlaybackSpeed::X2_00);
    EXPECT_EQ(overlap, nullptr);
}

TEST(TimelineModelTest, UpdateSpeedValue) {
    TimelineModel model;
    auto* sr = model.addSpeedRegion(0, 1000, PlaybackSpeed::X1_50);
    std::string id = sr->id;

    EXPECT_TRUE(model.updateSpeedValue(id, PlaybackSpeed::X0_75));
    EXPECT_EQ(model.speedRegions()[0].speed, PlaybackSpeed::X0_75);
}

// -- Annotation -------------------------------------------------------------

TEST(TimelineModelTest, AddAnnotationRegion) {
    TimelineModel model;
    auto& ar = model.addAnnotationRegion(0, 1000, AnnotationType::Text);

    EXPECT_EQ(ar.startMs, 0);
    EXPECT_EQ(ar.endMs, 1000);
    EXPECT_EQ(ar.type, AnnotationType::Text);
    // Default position and size from struct defaults (0,0 and 0,0)
    EXPECT_NEAR(ar.position.x, 0.0, 1e-9);
    EXPECT_NEAR(ar.position.y, 0.0, 1e-9);
    EXPECT_NEAR(ar.size.width, 0.0, 1e-9);
    EXPECT_NEAR(ar.size.height, 0.0, 1e-9);
    // Default style
    EXPECT_EQ(ar.style.color, "#ffffff");
    EXPECT_EQ(ar.style.fontWeight, FontWeight::Bold);
}

TEST(TimelineModelTest, AnnotationOverlapAllowed) {
    TimelineModel model;
    model.addAnnotationRegion(0, 1000, AnnotationType::Text);
    model.addAnnotationRegion(500, 1500, AnnotationType::Image);

    EXPECT_EQ(model.annotationRegions().size(), 2u);
}

// -- Queries ----------------------------------------------------------------

TEST(TimelineModelTest, ActiveZoomRegions) {
    TimelineModel model;
    model.addZoomRegion(1000, 2000, ZoomDepth::D3, ZoomFocus{0.5, 0.5});

    auto active = model.activeZoomRegions(1500);
    EXPECT_EQ(active.size(), 1u);

    auto none = model.activeZoomRegions(500);
    EXPECT_TRUE(none.empty());
}

TEST(TimelineModelTest, IsTimeTrimmed) {
    TimelineModel model;
    model.addTrimRegion(1000, 2000);

    EXPECT_TRUE(model.isTimeTrimmed(1500));
    EXPECT_FALSE(model.isTimeTrimmed(500));
}

TEST(TimelineModelTest, ActiveSpeedRegion) {
    TimelineModel model;
    model.addSpeedRegion(1000, 2000, PlaybackSpeed::X1_50);

    auto* active = model.activeSpeedRegion(1500);
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->speed, PlaybackSpeed::X1_50);

    auto* none = model.activeSpeedRegion(500);
    EXPECT_EQ(none, nullptr);
}

// -- Bulk operations --------------------------------------------------------

TEST(TimelineModelTest, Clear) {
    TimelineModel model;
    model.addZoomRegion(0, 1000, ZoomDepth::D1, ZoomFocus{0.5, 0.5});
    model.addTrimRegion(0, 1000);
    model.addSpeedRegion(2000, 3000, PlaybackSpeed::X1_50);
    model.addAnnotationRegion(0, 1000, AnnotationType::Text);

    model.clear();

    EXPECT_TRUE(model.zoomRegions().empty());
    EXPECT_TRUE(model.trimRegions().empty());
    EXPECT_TRUE(model.speedRegions().empty());
    EXPECT_TRUE(model.annotationRegions().empty());
}

TEST(TimelineModelTest, BulkSet) {
    TimelineModel model;

    std::vector<ZoomRegion> regions;
    ZoomRegion zr;
    zr.id = "zoom-custom-1";
    zr.startMs = 100;
    zr.endMs = 200;
    zr.depth = ZoomDepth::D2;
    zr.focus = ZoomFocus{0.3, 0.7};
    regions.push_back(zr);

    model.setZoomRegions(regions);

    ASSERT_EQ(model.zoomRegions().size(), 1u);
    EXPECT_EQ(model.zoomRegions()[0].id, "zoom-custom-1");
    EXPECT_EQ(model.zoomRegions()[0].startMs, 100);
}

// -- Invalid range ----------------------------------------------------------

TEST(TimelineModelTest, InvalidRange) {
    TimelineModel model;

    EXPECT_THROW(
        model.addZoomRegion(1000, 500, ZoomDepth::D1, ZoomFocus{0.5, 0.5}),
        std::invalid_argument
    );
}
