#include <gtest/gtest.h>
#include <QCoreApplication>
#include "render/PlaybackEngine.h"
#include "render/VideoDecoder.h"  // for VideoInfo

using namespace openscreen;

// ---------------------------------------------------------------------------
// QCoreApplication fixture (needed for QObject / QTimer internals)
// ---------------------------------------------------------------------------

class PlaybackEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char* argv[] = {(char*)"test"};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
        engine_ = std::make_unique<PlaybackEngine>();
    }

    void TearDown() override {
        engine_.reset();
    }

    std::unique_ptr<QCoreApplication> app_;
    std::unique_ptr<PlaybackEngine> engine_;
};

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST_F(PlaybackEngineTest, InitialState_NotLoaded) {
    EXPECT_FALSE(engine_->isLoaded());
}

TEST_F(PlaybackEngineTest, InitialState_NotPlaying) {
    EXPECT_FALSE(engine_->isPlaying());
}

TEST_F(PlaybackEngineTest, InitialState_ZeroPosition) {
    EXPECT_EQ(engine_->currentTimeMs(), 0);
}

TEST_F(PlaybackEngineTest, InitialState_ZeroDuration) {
    EXPECT_EQ(engine_->durationMs(), 0);
}

// ---------------------------------------------------------------------------
// Operations without a loaded video (should not crash)
// ---------------------------------------------------------------------------

TEST_F(PlaybackEngineTest, Play_WithoutVideo) {
    engine_->play();
    EXPECT_FALSE(engine_->isPlaying());
}

TEST_F(PlaybackEngineTest, Pause_WithoutVideo) {
    engine_->pause();
    EXPECT_FALSE(engine_->isPlaying());
}

TEST_F(PlaybackEngineTest, SeekTo_WithoutVideo) {
    engine_->seekTo(1000);
    EXPECT_EQ(engine_->currentTimeMs(), 0);
}

TEST_F(PlaybackEngineTest, TogglePlayPause_WithoutVideo) {
    engine_->togglePlayPause();
    EXPECT_FALSE(engine_->isPlaying());
}

TEST_F(PlaybackEngineTest, UnloadVideo_WhenNotLoaded) {
    engine_->unloadVideo();
    EXPECT_FALSE(engine_->isLoaded());
}

// ---------------------------------------------------------------------------
// Loading an invalid video
// ---------------------------------------------------------------------------

TEST_F(PlaybackEngineTest, LoadVideo_InvalidPath) {
    const bool ok = engine_->loadVideo("nonexistent.mp4");
    EXPECT_FALSE(ok);
    EXPECT_FALSE(engine_->isLoaded());
}

// ---------------------------------------------------------------------------
// Editor state
// ---------------------------------------------------------------------------

TEST_F(PlaybackEngineTest, SetEditorState_Null) {
    engine_->setEditorState(nullptr);
    // Should not crash; no assertion beyond survival
    EXPECT_FALSE(engine_->isLoaded());
}

// ---------------------------------------------------------------------------
// VideoInfo when no video loaded
// ---------------------------------------------------------------------------

TEST_F(PlaybackEngineTest, VideoInfo_WhenNotLoaded) {
    const VideoInfo info = engine_->videoInfo();
    EXPECT_EQ(info.width,      0);
    EXPECT_EQ(info.height,     0);
    EXPECT_DOUBLE_EQ(info.fps, 0.0);
    EXPECT_EQ(info.durationMs, 0);
    EXPECT_EQ(info.totalFrames, 0);
    EXPECT_TRUE(info.codecName.empty());
}
