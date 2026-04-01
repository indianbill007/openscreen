#pragma once
#ifdef __APPLE__

#include "AudioCapture.h"

#include <atomic>

namespace openscreen {

class AudioCaptureMac : public AudioCapture {
public:
    AudioCaptureMac() = default;
    ~AudioCaptureMac() override = default;

    AudioCaptureMac(const AudioCaptureMac&) = delete;
    AudioCaptureMac& operator=(const AudioCaptureMac&) = delete;

    AudioFormat format() const override { return format_; }

    bool start(AudioCallback /*callback*/) override {
        // TODO: implement CoreAudio tap / ScreenCaptureKit audio capture
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

#endif // __APPLE__
