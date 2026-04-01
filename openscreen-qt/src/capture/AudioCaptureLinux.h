#pragma once
#ifdef __linux__

#include "AudioCapture.h"

#include <atomic>

namespace openscreen {

class AudioCaptureLinux : public AudioCapture {
public:
    AudioCaptureLinux() = default;
    ~AudioCaptureLinux() override = default;

    AudioCaptureLinux(const AudioCaptureLinux&) = delete;
    AudioCaptureLinux& operator=(const AudioCaptureLinux&) = delete;

    AudioFormat format() const override { return format_; }

    bool start(AudioCallback /*callback*/) override {
        // TODO: implement PulseAudio / PipeWire monitor source capture
        return false;
    }

    void stop() override {
        capturing_ = false;
    }

    bool isCapturing() const override {
        return capturing_;
    }

private:
    std::atomic<bool> capturing_{false};
    AudioFormat format_;
};

} // namespace openscreen

#endif // __linux__
