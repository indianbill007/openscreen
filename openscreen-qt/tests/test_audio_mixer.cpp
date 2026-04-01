#include <gtest/gtest.h>
#include "capture/AudioMixer.h"

#include <vector>

namespace {

/// Fill a vector with a constant value for the given number of frames and channels.
std::vector<float> makeConstBuffer(float value, int frameCount, int channels) {
    return std::vector<float>(static_cast<size_t>(frameCount) * channels, value);
}

constexpr float kTolerance = 0.05f;

} // namespace

class AudioMixerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mixer_ = std::make_unique<openscreen::AudioMixer>(48000, 2);
    }

    std::unique_ptr<openscreen::AudioMixer> mixer_;
};

TEST_F(AudioMixerTest, SystemAudioOnly) {
    std::vector<float> output;
    int callbackCount = 0;

    mixer_->setOutputCallback(
        [&](const float* data, int frameCount, int channels, int /*sampleRate*/) {
            ++callbackCount;
            output.assign(data, data + static_cast<size_t>(frameCount) * channels);
        });

    auto buf = makeConstBuffer(0.5f, 1024, 2);
    mixer_->pushSystemAudio(buf.data(), 1024, 2, 48000);

    ASSERT_GT(callbackCount, 0);
    for (const auto& sample : output) {
        EXPECT_NEAR(sample, 0.5f, kTolerance);
    }
}

TEST_F(AudioMixerTest, MicAudioOnly) {
    std::vector<float> output;
    int callbackCount = 0;

    mixer_->setOutputCallback(
        [&](const float* data, int frameCount, int channels, int /*sampleRate*/) {
            ++callbackCount;
            output.assign(data, data + static_cast<size_t>(frameCount) * channels);
        });

    auto buf = makeConstBuffer(0.3f, 1024, 2);
    mixer_->pushMicAudio(buf.data(), 1024, 2, 48000);

    ASSERT_GT(callbackCount, 0);
    // Default mic gain is 1.4, so 0.3 * 1.4 = 0.42
    for (const auto& sample : output) {
        EXPECT_NEAR(sample, 0.3f * 1.4f, kTolerance);
    }
}

TEST_F(AudioMixerTest, MixedAudio) {
    std::vector<float> output;
    int callbackCount = 0;

    mixer_->setOutputCallback(
        [&](const float* data, int frameCount, int channels, int /*sampleRate*/) {
            ++callbackCount;
            output.assign(data, data + static_cast<size_t>(frameCount) * channels);
        });

    auto sysBuf = makeConstBuffer(0.4f, 1024, 2);
    auto micBuf = makeConstBuffer(0.3f, 1024, 2);
    mixer_->pushSystemAudio(sysBuf.data(), 1024, 2, 48000);
    mixer_->pushMicAudio(micBuf.data(), 1024, 2, 48000);

    ASSERT_GT(callbackCount, 0);
    // Expected: 0.4 + 0.3 * 1.4 = 0.82
    for (const auto& sample : output) {
        EXPECT_NEAR(sample, 0.82f, kTolerance);
    }
}

TEST_F(AudioMixerTest, Clamping) {
    std::vector<float> output;
    int callbackCount = 0;

    mixer_->setOutputCallback(
        [&](const float* data, int frameCount, int channels, int /*sampleRate*/) {
            ++callbackCount;
            output.assign(data, data + static_cast<size_t>(frameCount) * channels);
        });

    auto sysBuf = makeConstBuffer(0.9f, 1024, 2);
    auto micBuf = makeConstBuffer(0.9f, 1024, 2);
    mixer_->pushSystemAudio(sysBuf.data(), 1024, 2, 48000);
    mixer_->pushMicAudio(micBuf.data(), 1024, 2, 48000);

    ASSERT_GT(callbackCount, 0);
    // 0.9 + 0.9 * 1.4 = 2.16, should be clamped to 1.0
    for (const auto& sample : output) {
        EXPECT_NEAR(sample, 1.0f, kTolerance);
    }
}

TEST_F(AudioMixerTest, GainControl) {
    std::vector<float> output;
    int callbackCount = 0;

    mixer_->setOutputCallback(
        [&](const float* data, int frameCount, int channels, int /*sampleRate*/) {
            ++callbackCount;
            output.assign(data, data + static_cast<size_t>(frameCount) * channels);
        });

    mixer_->setSystemGain(0.5f);

    auto buf = makeConstBuffer(1.0f, 1024, 2);
    mixer_->pushSystemAudio(buf.data(), 1024, 2, 48000);

    ASSERT_GT(callbackCount, 0);
    for (const auto& sample : output) {
        EXPECT_NEAR(sample, 0.5f, kTolerance);
    }
}

TEST_F(AudioMixerTest, NoCallbackNoOutput) {
    // No callback set — should not crash
    auto buf = makeConstBuffer(0.5f, 1024, 2);
    mixer_->pushSystemAudio(buf.data(), 1024, 2, 48000);
    mixer_->pushMicAudio(buf.data(), 1024, 2, 48000);
    mixer_->flush();
    // If we reach here without crashing, the test passes
}

TEST_F(AudioMixerTest, Reset) {
    std::vector<float> output;
    int callbackCount = 0;

    mixer_->setOutputCallback(
        [&](const float* data, int frameCount, int channels, int /*sampleRate*/) {
            ++callbackCount;
            output.assign(data, data + static_cast<size_t>(frameCount) * channels);
        });

    // Push partial data, then reset
    auto buf = makeConstBuffer(0.5f, 500, 2);
    mixer_->pushSystemAudio(buf.data(), 500, 2, 48000);
    mixer_->reset();

    callbackCount = 0;
    output.clear();

    // Push a full chunk after reset
    auto buf2 = makeConstBuffer(0.7f, 1024, 2);
    mixer_->pushSystemAudio(buf2.data(), 1024, 2, 48000);

    ASSERT_GT(callbackCount, 0);
    for (const auto& sample : output) {
        EXPECT_NEAR(sample, 0.7f, kTolerance);
    }
}

TEST_F(AudioMixerTest, Flush) {
    std::vector<float> output;
    int callbackCount = 0;

    mixer_->setOutputCallback(
        [&](const float* data, int frameCount, int channels, int /*sampleRate*/) {
            ++callbackCount;
            output.assign(data, data + static_cast<size_t>(frameCount) * channels);
        });

    // Push less than a full chunk
    auto buf = makeConstBuffer(0.6f, 500, 2);
    mixer_->pushSystemAudio(buf.data(), 500, 2, 48000);

    EXPECT_EQ(callbackCount, 0);  // Not enough for a full chunk

    mixer_->flush();

    ASSERT_GT(callbackCount, 0);  // Flush should trigger the callback
    for (const auto& sample : output) {
        EXPECT_NEAR(sample, 0.6f, kTolerance);
    }
}

TEST_F(AudioMixerTest, EmptyFlush) {
    int callbackCount = 0;

    mixer_->setOutputCallback(
        [&](const float* /*data*/, int /*frameCount*/, int /*channels*/, int /*sampleRate*/) {
            ++callbackCount;
        });

    mixer_->flush();

    EXPECT_EQ(callbackCount, 0);
}

TEST_F(AudioMixerTest, MonoToStereo) {
    std::vector<float> output;
    int outputChannels = 0;
    int callbackCount = 0;

    mixer_->setOutputCallback(
        [&](const float* data, int frameCount, int channels, int /*sampleRate*/) {
            ++callbackCount;
            outputChannels = channels;
            output.assign(data, data + static_cast<size_t>(frameCount) * channels);
        });

    // Push mono mic audio (1 channel)
    auto monoBuf = makeConstBuffer(0.5f, 1024, 1);
    mixer_->pushMicAudio(monoBuf.data(), 1024, 1, 48000);

    ASSERT_GT(callbackCount, 0);
    EXPECT_EQ(outputChannels, 2);  // Output should be stereo

    // Each stereo pair should have the same value (duplicated mono)
    float expected = 0.5f * 1.4f;  // mic gain applied
    for (const auto& sample : output) {
        EXPECT_NEAR(sample, expected, kTolerance);
    }
}
