#include "RecordingSession.h"

#include "AudioCapture.h"
#include "AudioMixer.h"
#include "MicCapture.h"
#include "RecordingEncoder.h"
#include "ScreenCapture.h"
#include "WebcamCapture.h"
#include "core/types.h"

#include <spdlog/spdlog.h>

namespace openscreen {

RecordingSession::RecordingSession(QObject* parent)
    : QObject(parent)
    , screenCapture_(ScreenCapture::create())
{
    elapsedTimer_.setInterval(1000);
    connect(&elapsedTimer_, &QTimer::timeout, this, &RecordingSession::onTimerTick);
}

RecordingSession::~RecordingSession()
{
    if (recording_) {
        stopRecording();
    }
}

// ---------------------------------------------------------------------------
// Source management
// ---------------------------------------------------------------------------

std::vector<CaptureSource> RecordingSession::enumerateSources()
{
    if (!screenCapture_) {
        spdlog::error("RecordingSession: screenCapture_ is null");
        return {};
    }
    return screenCapture_->enumerateSources();
}

bool RecordingSession::selectSource(const std::string& sourceId)
{
    if (!screenCapture_) {
        spdlog::error("RecordingSession: screenCapture_ is null");
        return false;
    }
    return screenCapture_->selectSource(sourceId);
}

// ---------------------------------------------------------------------------
// Recording control
// ---------------------------------------------------------------------------

bool RecordingSession::startRecording(const std::string& outputPath)
{
    if (recording_) {
        spdlog::warn("RecordingSession: already recording");
        return false;
    }

    if (!screenCapture_) {
        emit recordingError("Screen capture not available");
        return false;
    }

    outputPath_ = outputPath;

    // Clear previous telemetry
    {
        std::lock_guard<std::mutex> lock(telemetryMutex_);
        cursorTelemetry_.clear();
    }

    // Determine capture resolution for encoder config
    // Use a reasonable default; the first frame will confirm actual size
    const auto sources = screenCapture_->enumerateSources();
    int width = 1920;
    int height = 1080;
    // If a source is already selected, try to get its dimensions from the list
    // (ScreenCapture doesn't expose selected source info directly)
    if (!sources.empty()) {
        // Use first source dimensions as fallback
        if (sources.front().width > 0 && sources.front().height > 0) {
            width = sources.front().width;
            height = sources.front().height;
        }
    }

    constexpr int kFps = 60;

    EncoderConfig config;
    config.width = width;
    config.height = height;
    config.fps = kFps;
    config.videoBitrate = computeBitrate(width, height, kFps);
    config.outputPath = outputPath;

    encoder_ = std::make_unique<RecordingEncoder>(config);
    if (!encoder_->open()) {
        emit recordingError("Failed to open encoder");
        encoder_.reset();
        return false;
    }

    // Set up audio mixer
    audioMixer_ = std::make_unique<AudioMixer>();
    audioMixer_->setMicGain(1.4f);
    audioMixer_->setOutputCallback(
        [this](const float* data, int frameCount, int channels, int sampleRate) {
            if (encoder_ && encoder_->isOpen()) {
                encoder_->pushAudioSamples(data, frameCount, channels, sampleRate);
            }
        });

    // Start screen capture with frame callback
    bool captureStarted = screenCapture_->start(
        [this](const CaptureFrame& frame) {
            if (encoder_ && encoder_->isOpen()) {
                encoder_->pushVideoFrame(
                    frame.data, frame.width, frame.height,
                    frame.stride, frame.timestampMs);
            }
        });

    if (!captureStarted) {
        emit recordingError("Failed to start screen capture");
        encoder_->close();
        encoder_.reset();
        audioMixer_.reset();
        return false;
    }

    // Set cursor callback for telemetry
    screenCapture_->setCursorCallback(
        [this](double cx, double cy, int64_t timestampMs) {
            std::lock_guard<std::mutex> lock(telemetryMutex_);
            cursorTelemetry_.push_back(CursorTelemetryPoint{
                static_cast<int>(timestampMs),
                cx,
                cy});
        });

    // Start system audio capture if enabled
    if (systemAudioEnabled_) {
        systemAudio_ = AudioCapture::createSystemCapture();
        if (systemAudio_) {
            systemAudio_->start([this](const AudioBuffer& buf) {
                if (audioMixer_) {
                    audioMixer_->pushSystemAudio(
                        buf.data, buf.frameCount, buf.channels, buf.sampleRate);
                }
            });
        } else {
            spdlog::warn("RecordingSession: system audio capture unavailable");
        }
    }

    // Start microphone capture if enabled
    if (micEnabled_) {
        micCapture_ = std::make_unique<MicCapture>(this);
        bool micStarted = micCapture_->start(micDeviceId_,
            [this](const AudioBuffer& buf) {
                if (audioMixer_) {
                    audioMixer_->pushMicAudio(
                        buf.data, buf.frameCount, buf.channels, buf.sampleRate);
                }
            });
        if (!micStarted) {
            spdlog::warn("RecordingSession: microphone capture failed to start");
            micCapture_.reset();
        }
    }

    // Start webcam capture if enabled
    if (webcamEnabled_) {
        webcamCapture_ = std::make_unique<WebcamCapture>(this);
        bool webcamStarted = webcamCapture_->start(webcamDeviceId_,
            [](const QImage& /*frame*/, int64_t /*timestampMs*/) {
                // Webcam frames stored separately for now
                // Future: encode into PiP overlay or second file
            });
        if (!webcamStarted) {
            spdlog::warn("RecordingSession: webcam capture failed to start");
            webcamCapture_.reset();
        }
    }

    // Start elapsed timer
    elapsedClock_.start();
    elapsedTimer_.start();

    recording_ = true;
    emit recordingStarted();
    spdlog::info("RecordingSession: recording started -> {}", outputPath);
    return true;
}

void RecordingSession::stopRecording()
{
    if (!recording_) {
        return;
    }

    spdlog::info("RecordingSession: stopping recording");

    // Stop captures
    if (screenCapture_) {
        screenCapture_->stop();
    }
    if (systemAudio_) {
        systemAudio_->stop();
        systemAudio_.reset();
    }
    if (micCapture_) {
        micCapture_->stop();
        micCapture_.reset();
    }
    if (webcamCapture_) {
        webcamCapture_->stop();
        webcamCapture_.reset();
    }

    // Flush audio mixer
    if (audioMixer_) {
        audioMixer_->flush();
        audioMixer_.reset();
    }

    // Close encoder
    std::string finalPath;
    if (encoder_) {
        finalPath = encoder_->close();
        encoder_.reset();
    }

    // Stop timer
    elapsedTimer_.stop();

    recording_ = false;
    emit recordingStopped(QString::fromStdString(
        finalPath.empty() ? outputPath_ : finalPath));
    spdlog::info("RecordingSession: recording stopped");
}

bool RecordingSession::isRecording() const
{
    return recording_;
}

// ---------------------------------------------------------------------------
// Audio options
// ---------------------------------------------------------------------------

void RecordingSession::setMicrophoneEnabled(bool enabled)
{
    micEnabled_ = enabled;
}

void RecordingSession::setMicrophoneDevice(const QString& deviceId)
{
    micDeviceId_ = deviceId;
}

void RecordingSession::setSystemAudioEnabled(bool enabled)
{
    systemAudioEnabled_ = enabled;
}

// ---------------------------------------------------------------------------
// Webcam options
// ---------------------------------------------------------------------------

void RecordingSession::setWebcamEnabled(bool enabled)
{
    webcamEnabled_ = enabled;
}

void RecordingSession::setWebcamDevice(const QString& deviceId)
{
    webcamDeviceId_ = deviceId;
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

int RecordingSession::elapsedSeconds() const
{
    if (!recording_) {
        return 0;
    }
    return static_cast<int>(elapsedClock_.elapsed() / 1000);
}

float RecordingSession::micLevel() const
{
    if (micCapture_) {
        return micCapture_->currentLevel();
    }
    return 0.0f;
}

std::string RecordingSession::outputPath() const
{
    return outputPath_;
}

std::vector<CursorTelemetryPoint> RecordingSession::cursorTelemetry() const
{
    std::lock_guard<std::mutex> lock(telemetryMutex_);
    return cursorTelemetry_;
}

// ---------------------------------------------------------------------------
// Timer slot
// ---------------------------------------------------------------------------

void RecordingSession::onTimerTick()
{
    emit elapsedTimeChanged(static_cast<int>(elapsedClock_.elapsed() / 1000));
}

} // namespace openscreen
