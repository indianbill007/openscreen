#include <gtest/gtest.h>
#include "capture/ScreenCapture.h"
#include "capture/AudioCapture.h"
#include "capture/AudioMixer.h"
#include "capture/RecordingEncoder.h"

#include <cmath>
#include <cstdint>
#include <string>

namespace {

/// Helper to compute the expected high-fps bitrate: base * 1.7, rounded.
int64_t highFpsBitrate(int64_t baseBitrate) {
    return static_cast<int64_t>(std::round(baseBitrate * 1.7));
}

} // namespace

// ---------------------------------------------------------------------------
// CaptureFrame
// ---------------------------------------------------------------------------

TEST(CaptureStructsTest, CaptureFrame_DefaultValues) {
    openscreen::CaptureFrame frame{};

    EXPECT_EQ(frame.data, nullptr);
    EXPECT_EQ(frame.width, 0);
    EXPECT_EQ(frame.height, 0);
    EXPECT_EQ(frame.stride, 0);
    EXPECT_EQ(frame.timestampMs, 0);
}

TEST(CaptureStructsTest, CaptureFrame_CustomValues) {
    const uint8_t pixels[] = {0xFF, 0x00, 0x00, 0xFF};
    openscreen::CaptureFrame frame{};
    frame.data = pixels;
    frame.width = 1920;
    frame.height = 1080;
    frame.stride = 7680;
    frame.timestampMs = 123456;

    EXPECT_EQ(frame.data, pixels);
    EXPECT_EQ(frame.width, 1920);
    EXPECT_EQ(frame.height, 1080);
    EXPECT_EQ(frame.stride, 7680);
    EXPECT_EQ(frame.timestampMs, 123456);
}

// ---------------------------------------------------------------------------
// CaptureSource
// ---------------------------------------------------------------------------

TEST(CaptureStructsTest, CaptureSource_DefaultValues) {
    openscreen::CaptureSource source{};

    EXPECT_TRUE(source.id.empty());
    EXPECT_TRUE(source.name.empty());
    EXPECT_FALSE(source.isWindow);
    EXPECT_EQ(source.width, 0);
    EXPECT_EQ(source.height, 0);
}

TEST(CaptureStructsTest, CaptureSource_DisplaySource) {
    openscreen::CaptureSource source{};
    source.id = "monitor:0";
    source.name = "Primary Display";
    source.isWindow = false;
    source.width = 2560;
    source.height = 1440;

    EXPECT_EQ(source.id, "monitor:0");
    EXPECT_EQ(source.name, "Primary Display");
    EXPECT_FALSE(source.isWindow);
    EXPECT_EQ(source.width, 2560);
    EXPECT_EQ(source.height, 1440);
}

TEST(CaptureStructsTest, CaptureSource_WindowSource) {
    openscreen::CaptureSource source{};
    source.id = "window:12345";
    source.name = "Firefox - OpenScreen";
    source.isWindow = true;
    source.width = 1280;
    source.height = 720;

    EXPECT_EQ(source.id, "window:12345");
    EXPECT_EQ(source.name, "Firefox - OpenScreen");
    EXPECT_TRUE(source.isWindow);
    EXPECT_EQ(source.width, 1280);
    EXPECT_EQ(source.height, 720);
}

// ---------------------------------------------------------------------------
// AudioFormat
// ---------------------------------------------------------------------------

TEST(CaptureStructsTest, AudioFormat_DefaultValues) {
    openscreen::AudioFormat format{};

    EXPECT_EQ(format.sampleRate, 48000);
    EXPECT_EQ(format.channels, 2);
    EXPECT_EQ(format.bitsPerSample, 32);
    EXPECT_TRUE(format.isFloat);
}

TEST(CaptureStructsTest, AudioFormat_CustomValues) {
    openscreen::AudioFormat format{};
    format.sampleRate = 44100;
    format.channels = 1;
    format.bitsPerSample = 16;
    format.isFloat = false;

    EXPECT_EQ(format.sampleRate, 44100);
    EXPECT_EQ(format.channels, 1);
    EXPECT_EQ(format.bitsPerSample, 16);
    EXPECT_FALSE(format.isFloat);
}

// ---------------------------------------------------------------------------
// AudioBuffer
// ---------------------------------------------------------------------------

TEST(CaptureStructsTest, AudioBuffer_DefaultValues) {
    openscreen::AudioBuffer buffer{};

    EXPECT_EQ(buffer.data, nullptr);
    EXPECT_EQ(buffer.frameCount, 0);
    EXPECT_EQ(buffer.channels, 2);
    EXPECT_EQ(buffer.sampleRate, 48000);
    EXPECT_EQ(buffer.timestampMs, 0);
}

// ---------------------------------------------------------------------------
// EncoderConfig
// ---------------------------------------------------------------------------

TEST(CaptureStructsTest, EncoderConfig_DefaultValues) {
    openscreen::EncoderConfig config{};

    EXPECT_EQ(config.width, 1920);
    EXPECT_EQ(config.height, 1080);
    EXPECT_EQ(config.fps, 60);
    EXPECT_EQ(config.videoBitrate, 18'000'000);
    EXPECT_EQ(config.videoCodec, "h264");
    EXPECT_TRUE(config.useHardwareAccel);
    EXPECT_EQ(config.audioSampleRate, 48000);
    EXPECT_EQ(config.audioChannels, 2);
    EXPECT_EQ(config.audioBitrate, 192'000);
    EXPECT_TRUE(config.outputPath.empty());
}

TEST(CaptureStructsTest, EncoderConfig_CustomValues) {
    openscreen::EncoderConfig config{};
    config.width = 3840;
    config.height = 2160;
    config.fps = 30;
    config.videoBitrate = 45'000'000;
    config.videoCodec = "h265";
    config.useHardwareAccel = false;
    config.audioSampleRate = 44100;
    config.audioChannels = 1;
    config.audioBitrate = 128'000;
    config.outputPath = "/tmp/recording.mp4";

    EXPECT_EQ(config.width, 3840);
    EXPECT_EQ(config.height, 2160);
    EXPECT_EQ(config.fps, 30);
    EXPECT_EQ(config.videoBitrate, 45'000'000);
    EXPECT_EQ(config.videoCodec, "h265");
    EXPECT_FALSE(config.useHardwareAccel);
    EXPECT_EQ(config.audioSampleRate, 44100);
    EXPECT_EQ(config.audioChannels, 1);
    EXPECT_EQ(config.audioBitrate, 128'000);
    EXPECT_EQ(config.outputPath, "/tmp/recording.mp4");
}

// ---------------------------------------------------------------------------
// computeBitrate
// ---------------------------------------------------------------------------

TEST(CaptureStructsTest, ComputeBitrate_AllTiers) {
    // 4K at 30fps
    EXPECT_EQ(openscreen::computeBitrate(3840, 2160, 30), 45'000'000);
    // 4K at 60fps
    EXPECT_EQ(openscreen::computeBitrate(3840, 2160, 60), highFpsBitrate(45'000'000));

    // QHD at 30fps
    EXPECT_EQ(openscreen::computeBitrate(2560, 1440, 30), 28'000'000);
    // QHD at 60fps
    EXPECT_EQ(openscreen::computeBitrate(2560, 1440, 60), highFpsBitrate(28'000'000));

    // 1080p at 30fps
    EXPECT_EQ(openscreen::computeBitrate(1920, 1080, 30), 18'000'000);
    // 1080p at 60fps
    EXPECT_EQ(openscreen::computeBitrate(1920, 1080, 60), highFpsBitrate(18'000'000));

    // 720p at 30fps (falls through to base tier)
    EXPECT_EQ(openscreen::computeBitrate(1280, 720, 30), 18'000'000);
    // 720p at 60fps
    EXPECT_EQ(openscreen::computeBitrate(1280, 720, 60), highFpsBitrate(18'000'000));
}

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------

TEST(CaptureStructsTest, FrameCallback_Callable) {
    bool invoked = false;
    int receivedWidth = 0;

    openscreen::FrameCallback callback = [&](const openscreen::CaptureFrame& frame) {
        invoked = true;
        receivedWidth = frame.width;
    };

    openscreen::CaptureFrame frame{};
    frame.width = 1920;
    frame.height = 1080;

    callback(frame);

    EXPECT_TRUE(invoked);
    EXPECT_EQ(receivedWidth, 1920);
}

TEST(CaptureStructsTest, CursorCallback_Callable) {
    bool invoked = false;
    double receivedCx = 0.0;
    double receivedCy = 0.0;
    int64_t receivedTimestamp = 0;

    openscreen::CursorCallback callback = [&](double cx, double cy, int64_t timestampMs) {
        invoked = true;
        receivedCx = cx;
        receivedCy = cy;
        receivedTimestamp = timestampMs;
    };

    callback(0.5, 0.3, 1000);

    EXPECT_TRUE(invoked);
    EXPECT_NEAR(receivedCx, 0.5, 1e-9);
    EXPECT_NEAR(receivedCy, 0.3, 1e-9);
    EXPECT_EQ(receivedTimestamp, 1000);
}

TEST(CaptureStructsTest, AudioCallback_Callable) {
    bool invoked = false;
    int receivedFrameCount = 0;
    int receivedChannels = 0;

    openscreen::AudioCallback callback = [&](const openscreen::AudioBuffer& buffer) {
        invoked = true;
        receivedFrameCount = buffer.frameCount;
        receivedChannels = buffer.channels;
    };

    const float samples[] = {0.1f, 0.2f, 0.3f, 0.4f};
    openscreen::AudioBuffer buffer{};
    buffer.data = samples;
    buffer.frameCount = 2;
    buffer.channels = 2;
    buffer.sampleRate = 48000;
    buffer.timestampMs = 500;

    callback(buffer);

    EXPECT_TRUE(invoked);
    EXPECT_EQ(receivedFrameCount, 2);
    EXPECT_EQ(receivedChannels, 2);
}

TEST(CaptureStructsTest, MixedAudioCallback_Callable) {
    bool invoked = false;
    int receivedFrameCount = 0;
    int receivedChannels = 0;
    int receivedSampleRate = 0;
    float receivedFirstSample = 0.0f;

    openscreen::MixedAudioCallback callback =
        [&](const float* data, int frameCount, int channels, int sampleRate) {
            invoked = true;
            receivedFirstSample = data[0];
            receivedFrameCount = frameCount;
            receivedChannels = channels;
            receivedSampleRate = sampleRate;
        };

    const float mixedData[] = {0.75f, -0.25f, 0.5f, -0.5f};
    callback(mixedData, 2, 2, 48000);

    EXPECT_TRUE(invoked);
    EXPECT_NEAR(receivedFirstSample, 0.75f, 1e-6f);
    EXPECT_EQ(receivedFrameCount, 2);
    EXPECT_EQ(receivedChannels, 2);
    EXPECT_EQ(receivedSampleRate, 48000);
}
