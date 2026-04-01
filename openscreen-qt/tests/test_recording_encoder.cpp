#include <gtest/gtest.h>
#include "capture/RecordingEncoder.h"

#include <cmath>

namespace {

/// Helper to compute the expected high-fps bitrate: base * 1.7, rounded.
int64_t highFpsBitrate(int64_t baseBitrate) {
    return static_cast<int64_t>(std::round(baseBitrate * 1.7));
}

} // namespace

TEST(RecordingEncoderTest, ComputeBitrate_4K) {
    EXPECT_EQ(openscreen::computeBitrate(3840, 2160, 30), 45'000'000);
}

TEST(RecordingEncoderTest, ComputeBitrate_4K_HighFps) {
    EXPECT_EQ(openscreen::computeBitrate(3840, 2160, 60), highFpsBitrate(45'000'000));
}

TEST(RecordingEncoderTest, ComputeBitrate_QHD) {
    EXPECT_EQ(openscreen::computeBitrate(2560, 1440, 30), 28'000'000);
}

TEST(RecordingEncoderTest, ComputeBitrate_QHD_HighFps) {
    EXPECT_EQ(openscreen::computeBitrate(2560, 1440, 60), highFpsBitrate(28'000'000));
}

TEST(RecordingEncoderTest, ComputeBitrate_1080p) {
    EXPECT_EQ(openscreen::computeBitrate(1920, 1080, 30), 18'000'000);
}

TEST(RecordingEncoderTest, ComputeBitrate_1080p_HighFps) {
    EXPECT_EQ(openscreen::computeBitrate(1920, 1080, 60), highFpsBitrate(18'000'000));
}

TEST(RecordingEncoderTest, ComputeBitrate_720p) {
    EXPECT_EQ(openscreen::computeBitrate(1280, 720, 30), 18'000'000);
}

TEST(RecordingEncoderTest, DefaultConfig) {
    openscreen::EncoderConfig config;

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
