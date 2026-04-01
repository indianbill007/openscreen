#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace openscreen {

struct AudioFormat {
    int sampleRate = 48000;
    int channels = 2;
    int bitsPerSample = 32;    // float32
    bool isFloat = true;
};

struct AudioBuffer {
    const float* data = nullptr;    // interleaved float samples
    int frameCount = 0;             // number of frames (each frame = channels samples)
    int channels = 2;
    int sampleRate = 48000;
    int64_t timestampMs = 0;
};

using AudioCallback = std::function<void(const AudioBuffer&)>;

class AudioCapture {
public:
    virtual ~AudioCapture() = default;

    AudioCapture() = default;
    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    virtual AudioFormat format() const = 0;
    virtual bool start(AudioCallback callback) = 0;
    virtual void stop() = 0;
    virtual bool isCapturing() const = 0;

    /// Factory: creates the platform-appropriate system audio capture
    static std::unique_ptr<AudioCapture> createSystemCapture();
};

} // namespace openscreen
