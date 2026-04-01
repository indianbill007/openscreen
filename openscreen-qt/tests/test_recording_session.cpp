#include <gtest/gtest.h>
#include <QCoreApplication>

#include "capture/RecordingSession.h"
#include "core/types.h"

using namespace openscreen;

// ---------------------------------------------------------------------------
// RecordingSessionTest — state management tests
//
// RecordingSession's constructor calls ScreenCapture::create() which returns
// a platform-specific implementation.  These tests focus on initial state,
// option setters, and safe stop-without-start behaviour — none of which
// require actual hardware or a running capture pipeline.
// ---------------------------------------------------------------------------

class RecordingSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // QCoreApplication is required for Qt's event loop, timers, and
        // signal/slot mechanism used by RecordingSession.
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char* argv[] = {const_cast<char*>("test")};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

    std::unique_ptr<QCoreApplication> app_;
};

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST_F(RecordingSessionTest, InitialState_NotRecording) {
    RecordingSession session;
    EXPECT_FALSE(session.isRecording());
}

TEST_F(RecordingSessionTest, InitialState_ZeroElapsed) {
    RecordingSession session;
    EXPECT_EQ(session.elapsedSeconds(), 0);
}

TEST_F(RecordingSessionTest, InitialState_EmptyOutput) {
    RecordingSession session;
    EXPECT_TRUE(session.outputPath().empty());
}

TEST_F(RecordingSessionTest, InitialState_EmptyTelemetry) {
    RecordingSession session;
    EXPECT_TRUE(session.cursorTelemetry().empty());
}

// ---------------------------------------------------------------------------
// Option setters — verify no crash and no unintended side-effects
// ---------------------------------------------------------------------------

TEST_F(RecordingSessionTest, SetMicrophoneEnabled) {
    RecordingSession session;
    session.setMicrophoneEnabled(true);
    session.setMicrophoneEnabled(false);
    // No crash; recording state unchanged.
    EXPECT_FALSE(session.isRecording());
}

TEST_F(RecordingSessionTest, SetSystemAudioEnabled) {
    RecordingSession session;
    session.setSystemAudioEnabled(true);
    session.setSystemAudioEnabled(false);
    EXPECT_FALSE(session.isRecording());
}

TEST_F(RecordingSessionTest, SetWebcamEnabled) {
    RecordingSession session;
    session.setWebcamEnabled(true);
    session.setWebcamEnabled(false);
    EXPECT_FALSE(session.isRecording());
}

TEST_F(RecordingSessionTest, SetMicrophoneDevice) {
    RecordingSession session;
    session.setMicrophoneDevice("test-device");
    EXPECT_FALSE(session.isRecording());
}

TEST_F(RecordingSessionTest, SetWebcamDevice) {
    RecordingSession session;
    session.setWebcamDevice("test-camera");
    EXPECT_FALSE(session.isRecording());
}

// ---------------------------------------------------------------------------
// Stop without start — must not crash or change state
// ---------------------------------------------------------------------------

TEST_F(RecordingSessionTest, StopWithoutStart) {
    RecordingSession session;
    session.stopRecording();
    EXPECT_FALSE(session.isRecording());
}

TEST_F(RecordingSessionTest, DoubleStop) {
    RecordingSession session;
    session.stopRecording();
    session.stopRecording();
    EXPECT_FALSE(session.isRecording());
}

// ---------------------------------------------------------------------------
// Combined option setting while not recording
// ---------------------------------------------------------------------------

TEST_F(RecordingSessionTest, SetOptionsWhileNotRecording) {
    RecordingSession session;

    session.setMicrophoneEnabled(true);
    session.setMicrophoneDevice("mic-1");
    session.setSystemAudioEnabled(true);
    session.setWebcamEnabled(true);
    session.setWebcamDevice("cam-1");

    // All options set without crash; session still idle.
    EXPECT_FALSE(session.isRecording());
    EXPECT_EQ(session.elapsedSeconds(), 0);
    EXPECT_TRUE(session.outputPath().empty());
    EXPECT_TRUE(session.cursorTelemetry().empty());
}
