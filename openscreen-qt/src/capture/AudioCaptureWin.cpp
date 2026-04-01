#ifdef _WIN32

#include "AudioCaptureWin.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstring>

namespace openscreen {

namespace {

// RAII wrapper for COM initialization per-thread
class ComInitializer {
public:
    ComInitializer() {
        result_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }
    ~ComInitializer() {
        if (SUCCEEDED(result_)) {
            CoUninitialize();
        }
    }
    ComInitializer(const ComInitializer&) = delete;
    ComInitializer& operator=(const ComInitializer&) = delete;
    bool succeeded() const { return SUCCEEDED(result_); }
private:
    HRESULT result_ = E_FAIL;
};

constexpr REFERENCE_TIME kRequestedDuration = 100000;  // 10ms in 100-ns units
constexpr int kPollSleepMs = 10;

} // namespace

AudioCaptureWin::~AudioCaptureWin() {
    stop();
}

AudioFormat AudioCaptureWin::format() const {
    return format_;
}

bool AudioCaptureWin::initDevice() {
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        &enumerator
    );
    if (FAILED(hr)) {
        spdlog::error("AudioCaptureWin: failed to create device enumerator (hr=0x{:08x})", hr);
        return false;
    }

    // eRender + loopback = capture system audio output
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    if (FAILED(hr)) {
        spdlog::error("AudioCaptureWin: failed to get default audio endpoint (hr=0x{:08x})", hr);
        return false;
    }
    return true;
}

bool AudioCaptureWin::initAudioClient() {
    HRESULT hr = device_->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        &audioClient_
    );
    if (FAILED(hr)) {
        spdlog::error("AudioCaptureWin: failed to activate audio client (hr=0x{:08x})", hr);
        return false;
    }

    // Get the native mix format
    WAVEFORMATEX* mixFormat = nullptr;
    hr = audioClient_->GetMixFormat(&mixFormat);
    if (FAILED(hr) || !mixFormat) {
        spdlog::error("AudioCaptureWin: failed to get mix format (hr=0x{:08x})", hr);
        return false;
    }

    // Record the actual format for consumers
    format_.sampleRate = static_cast<int>(mixFormat->nSamplesPerSec);
    format_.channels = static_cast<int>(mixFormat->nChannels);
    format_.bitsPerSample = static_cast<int>(mixFormat->wBitsPerSample);
    format_.isFloat = (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);

    // Check for WAVEFORMATEXTENSIBLE
    if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE && mixFormat->cbSize >= 22) {
        auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat);
        format_.isFloat = (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    }

    // Initialize in shared mode with loopback
    hr = audioClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        kRequestedDuration,
        0,
        mixFormat,
        nullptr
    );
    CoTaskMemFree(mixFormat);

    if (FAILED(hr)) {
        spdlog::error("AudioCaptureWin: failed to initialize audio client (hr=0x{:08x})", hr);
        return false;
    }

    hr = audioClient_->GetService(__uuidof(IAudioCaptureClient), &captureClient_);
    if (FAILED(hr)) {
        spdlog::error("AudioCaptureWin: failed to get capture client (hr=0x{:08x})", hr);
        return false;
    }

    return true;
}

void AudioCaptureWin::releaseResources() {
    captureClient_.Reset();
    audioClient_.Reset();
    device_.Reset();
}

void AudioCaptureWin::captureLoop() {
    ComInitializer com;
    if (!com.succeeded()) {
        spdlog::error("AudioCaptureWin: COM init failed in capture thread");
        capturing_ = false;
        return;
    }

    if (!initDevice() || !initAudioClient()) {
        spdlog::error("AudioCaptureWin: device/client init failed");
        capturing_ = false;
        releaseResources();
        return;
    }

    HRESULT hr = audioClient_->Start();
    if (FAILED(hr)) {
        spdlog::error("AudioCaptureWin: failed to start audio client (hr=0x{:08x})", hr);
        capturing_ = false;
        releaseResources();
        return;
    }

    spdlog::info("AudioCaptureWin: capture started ({}Hz, {}ch, {}bit {})",
                 format_.sampleRate, format_.channels, format_.bitsPerSample,
                 format_.isFloat ? "float" : "int");

    // Temporary buffer for int-to-float conversion
    std::vector<float> conversionBuffer;

    while (capturing_) {
        UINT32 packetLength = 0;
        hr = captureClient_->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) {
            spdlog::warn("AudioCaptureWin: GetNextPacketSize failed (hr=0x{:08x})", hr);
            break;
        }

        while (packetLength > 0 && capturing_) {
            BYTE* rawData = nullptr;
            UINT32 numFrames = 0;
            DWORD flags = 0;

            hr = captureClient_->GetBuffer(&rawData, &numFrames, &flags, nullptr, nullptr);
            if (FAILED(hr)) {
                spdlog::warn("AudioCaptureWin: GetBuffer failed (hr=0x{:08x})", hr);
                break;
            }

            auto now = std::chrono::steady_clock::now();
            auto timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();

            const float* floatData = nullptr;

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                // Silent buffer — provide zeros
                const int totalSamples = static_cast<int>(numFrames) * format_.channels;
                conversionBuffer.assign(static_cast<size_t>(totalSamples), 0.0f);
                floatData = conversionBuffer.data();
            } else if (format_.isFloat && format_.bitsPerSample == 32) {
                // Already float32 — use directly
                floatData = reinterpret_cast<const float*>(rawData);
            } else if (!format_.isFloat && format_.bitsPerSample == 16) {
                // Convert int16 to float
                const auto* int16Data = reinterpret_cast<const int16_t*>(rawData);
                const int totalSamples = static_cast<int>(numFrames) * format_.channels;
                conversionBuffer.resize(static_cast<size_t>(totalSamples));
                for (int i = 0; i < totalSamples; ++i) {
                    conversionBuffer[static_cast<size_t>(i)] =
                        static_cast<float>(int16Data[i]) / 32768.0f;
                }
                floatData = conversionBuffer.data();
            } else {
                // Unsupported format — skip
                captureClient_->ReleaseBuffer(numFrames);
                captureClient_->GetNextPacketSize(&packetLength);
                continue;
            }

            AudioBuffer buffer;
            buffer.data = floatData;
            buffer.frameCount = static_cast<int>(numFrames);
            buffer.channels = format_.channels;
            buffer.sampleRate = format_.sampleRate;
            buffer.timestampMs = timestampMs;

            {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (callback_) {
                    callback_(buffer);
                }
            }

            captureClient_->ReleaseBuffer(numFrames);

            hr = captureClient_->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) {
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kPollSleepMs));
    }

    audioClient_->Stop();
    releaseResources();
    spdlog::info("AudioCaptureWin: capture stopped");
}

bool AudioCaptureWin::start(AudioCallback callback) {
    if (capturing_) {
        spdlog::warn("AudioCaptureWin: already capturing");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback_ = std::move(callback);
    }

    capturing_ = true;
    captureThread_ = std::thread(&AudioCaptureWin::captureLoop, this);
    return true;
}

void AudioCaptureWin::stop() {
    if (!capturing_) {
        return;
    }
    capturing_ = false;
    if (captureThread_.joinable()) {
        captureThread_.join();
    }
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback_ = nullptr;
    }
}

bool AudioCaptureWin::isCapturing() const {
    return capturing_;
}

// Factory implementation
std::unique_ptr<AudioCapture> AudioCapture::createSystemCapture() {
    return std::make_unique<AudioCaptureWin>();
}

} // namespace openscreen

#endif // _WIN32
