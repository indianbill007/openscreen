#include <gtest/gtest.h>
#include <QApplication>

#include "ui/timeline/TimelineRuler.h"
#include "ui/timeline/TimelineRow.h"
#include "ui/timeline/TimelineItem.h"
#include "ui/timeline/TimelineWidget.h"
#include "core/EditorState.h"
#include "render/PlaybackEngine.h"

using namespace openscreen;

// ===========================================================================
// QApplication fixture — QWidgets require a running QApplication instance
// ===========================================================================

class TimelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char* argv[] = {const_cast<char*>("test")};
            app_ = std::make_unique<QApplication>(argc, argv);
        }
    }
    std::unique_ptr<QApplication> app_;
};

// ===========================================================================
// TimelineRuler tests
// ===========================================================================

TEST_F(TimelineTest, Ruler_DefaultDuration) {
    TimelineRuler ruler;
    // positionToTime returns viewStartMs_ when range is 0, and viewStartMs_
    // defaults to 0, so the effective duration is 0.
    EXPECT_EQ(ruler.positionToTime(0), 0);
    EXPECT_EQ(ruler.timeToPosition(0), 0);
}

TEST_F(TimelineTest, Ruler_SetDuration) {
    TimelineRuler ruler;
    ruler.setDuration(10000);
    // Should not crash; view end is now 10000
    EXPECT_NO_FATAL_FAILURE(ruler.setDuration(10000));
}

TEST_F(TimelineTest, Ruler_PositionToTime_Linear) {
    TimelineRuler ruler;
    ruler.setDuration(10000);
    ruler.setViewRange(0, 10000);
    ruler.resize(500, 28);

    // Middle of a 500px widget mapping [0, 10000]ms should be ~5000ms
    const int64_t midTime = ruler.positionToTime(250);
    EXPECT_NEAR(midTime, 5000, 50);
}

TEST_F(TimelineTest, Ruler_TimeToPosition_Linear) {
    TimelineRuler ruler;
    ruler.setDuration(10000);
    ruler.setViewRange(0, 10000);
    ruler.resize(500, 28);

    // 5000ms in [0, 10000]ms on a 500px widget should be ~250px
    const int midPos = ruler.timeToPosition(5000);
    EXPECT_NEAR(midPos, 250, 2);
}

TEST_F(TimelineTest, Ruler_TimeToPosition_Zoomed) {
    TimelineRuler ruler;
    ruler.setDuration(10000);
    ruler.setViewRange(2000, 4000);
    ruler.resize(500, 28);

    // 3000ms is the midpoint of [2000, 4000]ms -> ~250px
    const int pos = ruler.timeToPosition(3000);
    EXPECT_NEAR(pos, 250, 2);
}

// ===========================================================================
// TimelineItem tests
// ===========================================================================

TEST_F(TimelineTest, Item_Creation) {
    TimelineItem item("zoom-1", TimelineItem::Variant::Zoom);
    EXPECT_EQ(item.id(), "zoom-1");
    EXPECT_EQ(item.variant(), TimelineItem::Variant::Zoom);
}

TEST_F(TimelineTest, Item_SetTimeRange) {
    TimelineItem item("trim-1", TimelineItem::Variant::Trim);
    item.setTimeRange(1000, 3000);
    EXPECT_EQ(item.startMs(), 1000);
    EXPECT_EQ(item.endMs(), 3000);
}

TEST_F(TimelineTest, Item_Selection) {
    TimelineItem item("speed-1", TimelineItem::Variant::Speed);
    EXPECT_FALSE(item.isSelected());

    item.setSelected(true);
    EXPECT_TRUE(item.isSelected());

    item.setSelected(false);
    EXPECT_FALSE(item.isSelected());
}

TEST_F(TimelineTest, Item_Label) {
    TimelineItem item("zoom-2", TimelineItem::Variant::Zoom);
    EXPECT_NO_FATAL_FAILURE(item.setLabel("1.8x"));
}

TEST_F(TimelineTest, Item_VariantColors) {
    // Each variant should construct without crashing
    EXPECT_NO_FATAL_FAILURE({
        TimelineItem zoom("z", TimelineItem::Variant::Zoom);
    });
    EXPECT_NO_FATAL_FAILURE({
        TimelineItem trim("t", TimelineItem::Variant::Trim);
    });
    EXPECT_NO_FATAL_FAILURE({
        TimelineItem speed("s", TimelineItem::Variant::Speed);
    });
    EXPECT_NO_FATAL_FAILURE({
        TimelineItem ann("a", TimelineItem::Variant::Annotation);
    });
}

// ===========================================================================
// TimelineRow tests
// ===========================================================================

TEST_F(TimelineTest, Row_Creation) {
    EXPECT_NO_FATAL_FAILURE({
        TimelineRow row("ZOOM", QColor("#3b82f6"));
    });
}

TEST_F(TimelineTest, Row_AddItem) {
    TimelineRow row("ZOOM", QColor("#3b82f6"));
    auto* item = row.addItem("zoom-1", TimelineItem::Variant::Zoom, 0, 1000);
    EXPECT_NE(item, nullptr);
    EXPECT_EQ(row.items().size(), 1u);
}

TEST_F(TimelineTest, Row_RemoveItem) {
    TimelineRow row("TRIM", QColor("#ef4444"));
    row.addItem("trim-1", TimelineItem::Variant::Trim, 0, 1000);
    EXPECT_EQ(row.items().size(), 1u);

    EXPECT_TRUE(row.removeItem("trim-1"));
    EXPECT_TRUE(row.items().empty());
}

TEST_F(TimelineTest, Row_RemoveItem_NotFound) {
    TimelineRow row("TRIM", QColor("#ef4444"));
    EXPECT_FALSE(row.removeItem("nonexistent"));
}

TEST_F(TimelineTest, Row_ClearItems) {
    TimelineRow row("SPEED", QColor("#22c55e"));
    row.addItem("s1", TimelineItem::Variant::Speed, 0, 1000);
    row.addItem("s2", TimelineItem::Variant::Speed, 2000, 3000);
    row.addItem("s3", TimelineItem::Variant::Speed, 4000, 5000);
    EXPECT_EQ(row.items().size(), 3u);

    row.clearItems();
    EXPECT_TRUE(row.items().empty());
}

TEST_F(TimelineTest, Row_SelectItem) {
    TimelineRow row("ZOOM", QColor("#3b82f6"));
    row.addItem("zoom-1", TimelineItem::Variant::Zoom, 0, 1000);
    row.addItem("zoom-2", TimelineItem::Variant::Zoom, 2000, 3000);

    row.selectItem("zoom-1");
    EXPECT_TRUE(row.items()[0]->isSelected());
    EXPECT_FALSE(row.items()[1]->isSelected());

    // Selecting the second should deselect the first
    row.selectItem("zoom-2");
    EXPECT_FALSE(row.items()[0]->isSelected());
    EXPECT_TRUE(row.items()[1]->isSelected());
}

TEST_F(TimelineTest, Row_DeselectAll) {
    TimelineRow row("ZOOM", QColor("#3b82f6"));
    row.addItem("zoom-1", TimelineItem::Variant::Zoom, 0, 1000);
    row.addItem("zoom-2", TimelineItem::Variant::Zoom, 2000, 3000);

    row.selectItem("zoom-1");
    EXPECT_TRUE(row.items()[0]->isSelected());

    row.deselectAll();
    EXPECT_FALSE(row.items()[0]->isSelected());
    EXPECT_FALSE(row.items()[1]->isSelected());
}

// ===========================================================================
// TimelineWidget tests
// ===========================================================================

TEST_F(TimelineTest, Widget_Creation) {
    EXPECT_NO_FATAL_FAILURE({
        TimelineWidget widget;
    });
}

TEST_F(TimelineTest, Widget_SetDuration) {
    TimelineWidget widget;
    EXPECT_NO_FATAL_FAILURE(widget.setDuration(60000));
}

TEST_F(TimelineTest, Widget_SetPlayhead) {
    TimelineWidget widget;
    widget.setDuration(60000);
    EXPECT_NO_FATAL_FAILURE(widget.setPlayheadPosition(5000));
}

TEST_F(TimelineTest, Widget_LoadEditorState) {
    TimelineWidget widget;
    widget.setDuration(60000);

    EditorState state;

    ZoomRegion zr1;
    zr1.id = "zoom-1";
    zr1.startMs = 1000;
    zr1.endMs = 3000;
    zr1.depth = ZoomDepth::D3;
    state.zoomRegions.push_back(zr1);

    ZoomRegion zr2;
    zr2.id = "zoom-2";
    zr2.startMs = 5000;
    zr2.endMs = 8000;
    zr2.depth = ZoomDepth::D4;
    state.zoomRegions.push_back(zr2);

    TrimRegion tr1;
    tr1.id = "trim-1";
    tr1.startMs = 10000;
    tr1.endMs = 15000;
    state.trimRegions.push_back(tr1);

    EXPECT_NO_FATAL_FAILURE(widget.loadFromEditorState(state));
}

// ===========================================================================
// TimelineController tests (via EditorHistory state mutations)
//
// TimelineController does not exist as a class yet. These tests exercise
// the controller-level logic — adding, deleting, and syncing zoom/trim/speed
// regions — through EditorHistory::pushState, which is the same mechanism
// a controller will use.
// ===========================================================================

TEST_F(TimelineTest, Controller_SyncFromState) {
    EditorHistory history;

    EditorState state;
    ZoomRegion zr;
    zr.id = "zoom-1";
    zr.startMs = 1000;
    zr.endMs = 3000;
    state.zoomRegions.push_back(zr);

    history.pushState(state);
    EXPECT_EQ(history.state().zoomRegions.size(), 1u);
    EXPECT_EQ(history.state().zoomRegions[0].id, "zoom-1");
}

TEST_F(TimelineTest, Controller_ZoomAdd) {
    EditorHistory history;

    // Simulate onZoomAdded(1000, 3000): create a new state with the region
    EditorState state = history.state();
    ZoomRegion zr;
    zr.id = "zoom-new";
    zr.startMs = 1000;
    zr.endMs = 3000;
    zr.depth = ZoomDepth::D3;
    state.zoomRegions.push_back(zr);
    history.pushState(state);

    EXPECT_EQ(history.state().zoomRegions.size(), 1u);
    EXPECT_EQ(history.state().zoomRegions[0].startMs, 1000);
    EXPECT_EQ(history.state().zoomRegions[0].endMs, 3000);
}

TEST_F(TimelineTest, Controller_ZoomDelete) {
    EditorHistory history;

    // Add a zoom region
    EditorState state = history.state();
    ZoomRegion zr;
    zr.id = "zoom-del";
    zr.startMs = 500;
    zr.endMs = 2000;
    state.zoomRegions.push_back(zr);
    history.pushState(state);
    EXPECT_EQ(history.state().zoomRegions.size(), 1u);

    // Delete: push state without the region
    EditorState deleted = history.state();
    deleted.zoomRegions.clear();
    history.pushState(deleted);
    EXPECT_TRUE(history.state().zoomRegions.empty());

    // Undo should restore the region
    EXPECT_TRUE(history.undo());
    EXPECT_EQ(history.state().zoomRegions.size(), 1u);
    EXPECT_EQ(history.state().zoomRegions[0].id, "zoom-del");
}

TEST_F(TimelineTest, Controller_TrimAdd) {
    EditorHistory history;

    EditorState state = history.state();
    TrimRegion tr;
    tr.id = "trim-1";
    tr.startMs = 2000;
    tr.endMs = 4000;
    state.trimRegions.push_back(tr);
    history.pushState(state);

    EXPECT_EQ(history.state().trimRegions.size(), 1u);
    EXPECT_EQ(history.state().trimRegions[0].id, "trim-1");
    EXPECT_EQ(history.state().trimRegions[0].startMs, 2000);
    EXPECT_EQ(history.state().trimRegions[0].endMs, 4000);
}

TEST_F(TimelineTest, Controller_SpeedAdd) {
    EditorHistory history;

    EditorState state = history.state();
    SpeedRegion sr;
    sr.id = "speed-1";
    sr.startMs = 3000;
    sr.endMs = 6000;
    sr.speed = PlaybackSpeed::X2_00;
    state.speedRegions.push_back(sr);
    history.pushState(state);

    EXPECT_EQ(history.state().speedRegions.size(), 1u);
    EXPECT_EQ(history.state().speedRegions[0].id, "speed-1");
    EXPECT_EQ(history.state().speedRegions[0].startMs, 3000);
    EXPECT_EQ(history.state().speedRegions[0].endMs, 6000);
    EXPECT_EQ(history.state().speedRegions[0].speed, PlaybackSpeed::X2_00);
}
