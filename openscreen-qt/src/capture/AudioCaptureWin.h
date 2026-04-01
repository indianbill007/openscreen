#pragma once
#ifdef _WIN32

#include "AudioCapture.h"

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace openscreen {

class AudioCaptureWin : public AudioCapture {
public:
    AudioCaptureWin() = default;
    ~AudioCaptureWin() override;

    AudioCaptureWin(const AudioCaptureWin&) = delete;
    AudioCaptureWin& operator=(const AudioCaptureWin&) = delete;

    AudioFormat format() const override;
    bool start(AudioCallback callback) override;
    void stop() override;
    bool isCapturing() const override;

private:
    bool initDevice();
    bool initAudioClient();
    void captureLoop();
    void releaseResources();

    Microsoft::WRL::ComPtr<IMMDevice> device_;
    Microsoft::WRL::ComPtr<IAudioClient> audioClient_;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> captureClient_;

    std::thread captureThread_;
    std::atomic<bool> capturing_{false};

    AudioCallback callback_;
    std::mutex callbackMutex_;
    AudioFormat format_;
};

} // namespace openscreen

#endif // _WIN32
