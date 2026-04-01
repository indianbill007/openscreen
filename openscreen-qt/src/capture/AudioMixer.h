#pragma once
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace openscreen {

struct AudioBuffer;  // forward from AudioCapture.h

using MixedAudioCallback = std::function<void(const float* data, int frameCount, int channels, int sampleRate)>;

class AudioMixer {
public:
    explicit AudioMixer(int sampleRate = 48000, int channels = 2);

    /// Set output callback — called whenever mixed audio is ready
    void setOutputCallback(MixedAudioCallback callback);

    /// Feed system audio samples (interleaved float32)
    void pushSystemAudio(const float* data, int frameCount, int channels, int sampleRate);

    /// Feed microphone audio samples (interleaved float32)
    void pushMicAudio(const float* data, int frameCount, int channels, int sampleRate);

    /// Set gain for each source (default: system=1.0, mic=1.4)
    void setSystemGain(float gain);
    void setMicGain(float gain);

    /// Flush any remaining buffered audio
    void flush();

    /// Reset internal buffers
    void reset();

private:
    void mixAndOutput();

    int sampleRate_;
    int channels_;
    float systemGain_ = 1.0f;
    float micGain_ = 1.4f;  // MIC_GAIN_BOOST from Electron version

    std::vector<float> systemBuffer_;
    std::vector<float> micBuffer_;
    std::vector<float> mixBuffer_;

    MixedAudioCallback outputCallback_;
    std::mutex mutex_;

    static constexpr int kMixChunkFrames = 1024;  // mix in chunks of 1024 frames
};

} // namespace openscreen
