#include <gtest/gtest.h>
#include "core/EditorState.h"

using namespace openscreen;

// ---------------------------------------------------------------------------
// EditorHistoryTest
// ---------------------------------------------------------------------------

TEST(EditorHistoryTest, InitialState) {
    EditorHistory history;
    const auto& s = history.state();

    EXPECT_TRUE(s.zoomRegions.empty());
    EXPECT_TRUE(s.trimRegions.empty());
    EXPECT_TRUE(s.speedRegions.empty());
    EXPECT_TRUE(s.annotationRegions.empty());
    EXPECT_NEAR(s.padding, 50.0, 1e-9);
    EXPECT_EQ(s.wallpaper, "/wallpapers/wallpaper1.jpg");
    EXPECT_EQ(s.aspectRatio, AspectRatio::R16_9);
}

TEST(EditorHistoryTest, PushState) {
    EditorHistory history;

    EditorState modified;
    modified.padding = 100.0;
    history.pushState(modified);

    EXPECT_NEAR(history.state().padding, 100.0, 1e-9);
    EXPECT_TRUE(history.canUndo());
    EXPECT_FALSE(history.canRedo());
}

TEST(EditorHistoryTest, UndoRedo) {
    EditorHistory history;

    EditorState modified;
    modified.padding = 100.0;
    history.pushState(modified);

    EXPECT_TRUE(history.undo());
    EXPECT_NEAR(history.state().padding, 50.0, 1e-9);

    EXPECT_TRUE(history.redo());
    EXPECT_NEAR(history.state().padding, 100.0, 1e-9);
}

TEST(EditorHistoryTest, UndoEmpty) {
    EditorHistory history;
    EXPECT_FALSE(history.undo());
}

TEST(EditorHistoryTest, RedoEmpty) {
    EditorHistory history;
    EXPECT_FALSE(history.redo());
}

TEST(EditorHistoryTest, PushClearsFuture) {
    EditorHistory history;

    EditorState s1;
    s1.padding = 10.0;
    history.pushState(s1);

    EditorState s2;
    s2.padding = 20.0;
    history.pushState(s2);

    history.undo();
    EXPECT_TRUE(history.canRedo());

    EditorState s3;
    s3.padding = 30.0;
    history.pushState(s3);

    EXPECT_FALSE(history.canRedo());
}

TEST(EditorHistoryTest, MaxHistory) {
    EditorHistory history;

    for (int i = 0; i < 100; ++i) {
        EditorState s;
        s.padding = static_cast<double>(i);
        history.pushState(s);
    }

    EXPECT_TRUE(history.canUndo());

    // Should succeed for 80 undos (MAX_HISTORY)
    int undoCount = 0;
    while (history.undo()) {
        ++undoCount;
    }

    EXPECT_EQ(undoCount, static_cast<int>(MAX_HISTORY));
}

TEST(EditorHistoryTest, UpdateState_FirstCallCheckpoints) {
    EditorHistory history;

    EditorState s;
    s.padding = 200.0;
    history.updateState(s);

    EXPECT_TRUE(history.undo());
    EXPECT_NEAR(history.state().padding, 50.0, 1e-9);
}

TEST(EditorHistoryTest, UpdateState_SubsequentNonCheckpoint) {
    EditorHistory history;

    // Simulate a slider drag: multiple updateState calls without commit
    EditorState s1;
    s1.padding = 60.0;
    history.updateState(s1);

    EditorState s2;
    s2.padding = 70.0;
    history.updateState(s2);

    EditorState s3;
    s3.padding = 80.0;
    history.updateState(s3);

    // Current state should be the last update
    EXPECT_NEAR(history.state().padding, 80.0, 1e-9);

    // One undo should go back to the pre-drag state (initial)
    EXPECT_TRUE(history.undo());
    EXPECT_NEAR(history.state().padding, 50.0, 1e-9);

    // No more undos
    EXPECT_FALSE(history.undo());
}

TEST(EditorHistoryTest, CommitState_ResetsForNextUpdate) {
    EditorHistory history;

    // First drag series
    EditorState s1;
    s1.padding = 100.0;
    history.updateState(s1);
    history.commitState();

    // Second drag series
    EditorState s2;
    s2.padding = 200.0;
    history.updateState(s2);
    history.commitState();

    // Undo second drag -> back to s1
    EXPECT_TRUE(history.undo());
    EXPECT_NEAR(history.state().padding, 100.0, 1e-9);

    // Undo first drag -> back to initial
    EXPECT_TRUE(history.undo());
    EXPECT_NEAR(history.state().padding, 50.0, 1e-9);
}

TEST(EditorHistoryTest, JsonRoundTrip) {
    EditorState original;
    original.padding = 75.0;
    original.wallpaper = "/wallpapers/custom.png";
    original.aspectRatio = AspectRatio::R4_3;
    original.shadowIntensity = 0.5;
    original.borderRadius = 12.0;

    ZoomRegion zr;
    zr.id = "zoom-1";
    zr.startMs = 100;
    zr.endMs = 500;
    zr.depth = ZoomDepth::D4;
    zr.focus = ZoomFocus{0.3, 0.7};
    original.zoomRegions.push_back(zr);

    TrimRegion tr;
    tr.id = "trim-1";
    tr.startMs = 1000;
    tr.endMs = 2000;
    original.trimRegions.push_back(tr);

    nlohmann::json j = original;
    EditorState restored = j.get<EditorState>();

    EXPECT_NEAR(restored.padding, original.padding, 1e-9);
    EXPECT_EQ(restored.wallpaper, original.wallpaper);
    EXPECT_EQ(restored.aspectRatio, original.aspectRatio);
    EXPECT_NEAR(restored.shadowIntensity, original.shadowIntensity, 1e-9);
    EXPECT_NEAR(restored.borderRadius, original.borderRadius, 1e-9);

    ASSERT_EQ(restored.zoomRegions.size(), 1u);
    EXPECT_EQ(restored.zoomRegions[0].id, "zoom-1");
    EXPECT_EQ(restored.zoomRegions[0].startMs, 100);
    EXPECT_EQ(restored.zoomRegions[0].endMs, 500);
    EXPECT_EQ(restored.zoomRegions[0].depth, ZoomDepth::D4);
    EXPECT_NEAR(restored.zoomRegions[0].focus.cx, 0.3, 1e-9);
    EXPECT_NEAR(restored.zoomRegions[0].focus.cy, 0.7, 1e-9);

    ASSERT_EQ(restored.trimRegions.size(), 1u);
    EXPECT_EQ(restored.trimRegions[0].id, "trim-1");
    EXPECT_EQ(restored.trimRegions[0].startMs, 1000);
    EXPECT_EQ(restored.trimRegions[0].endMs, 2000);
}
