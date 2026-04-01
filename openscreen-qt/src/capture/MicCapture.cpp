#include "MicCapture.h"

#include <spdlog/spdlog.h>

#include <QAudioFormat>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace openscreen {

namespace {

constexpr int kTargetSampleRate = 48000;
constexpr int kTargetChannels = 2;
constexpr int kTargetBitsPerSample = 32;
constexpr float kLevelScale = 2.0f;
constexpr float kMaxLevel = 100.0f;

QAudioFormat makeDesiredFormat() {
    QAudioFormat fmt;
    fmt.setSampleRate(kTargetSampleRate);
    fmt.setChannelCount(kTargetChannels);
    fmt.setSampleFormat(QAudioFormat::Float);
    return fmt;
}

} // namespace

MicCapture::MicCapture(QObject* parent)
    : QObject(parent) {
    format_.sampleRate = kTargetSampleRate;
    format_.channels = kTargetChannels;
    format_.bitsPerSample = kTargetBitsPerSample;
    format_.isFloat = true;
}

MicCapture::~MicCapture() {
    stop();
}

QList<MicDevice> MicCapture::enumerateDevices() {
    QList<MicDevice> devices;
    const auto inputs = QMediaDevices::audioInputs();
    devices.reserve(inputs.size());
    for (const auto& dev : inputs) {
        MicDevice mic;
        mic.id = dev.id();
        mic.name = dev.description();
        devices.append(mic);
    }
    return devices;
}

QAudioDevice MicCapture::findDevice(const QString& deviceId) const {
    if (deviceId.isEmpty()) {
        return QMediaDevices::defaultAudioInput();
    }
    const auto inputs = QMediaDevices::audioInputs();
    for (const auto& dev : inputs) {
        if (dev.id() == deviceId) {
            return dev;
        }
    }
    spdlog::warn("MicCapture: device '{}' not found, using default",
                 deviceId.toStdString());
    return QMediaDevices::defaultAudioInput();
}

bool MicCapture::start(const QString& deviceId, AudioCallback callback) {
    if (audioSource_) {
        spdlog::warn("MicCapture: already capturing");
        return false;
    }

    const QAudioDevice device = findDevice(deviceId);
    if (device.isNull()) {
        spdlog::error("MicCapture: no audio input device available");
        return false;
    }

    const QAudioFormat desiredFormat = makeDesiredFormat();
    if (!device.isFormatSupported(desiredFormat)) {
        spdlog::error("MicCapture: desired audio format not supported by device '{}'",
                      device.description().toStdString());
        return false;
    }

    callback_ = std::move(callback);
    audioSource_ = std::make_unique<QAudioSource>(device, desiredFormat);
    ioDevice_ = audioSource_->start();

    if (!ioDevice_) {
        spdlog::error("MicCapture: failed to start audio source");
        audioSource_.reset();
        callback_ = nullptr;
        return false;
    }

    connect(ioDevice_, &QIODevice::readyRead, this, &MicCapture::onReadyRead);

    spdlog::info("MicCapture: started on '{}' ({}Hz, {}ch)",
                 device.description().toStdString(),
                 format_.sampleRate, format_.channels);
    return true;
}

void MicCapture::stop() {
    if (!audioSource_) {
        return;
    }

    if (ioDevice_) {
        disconnect(ioDevice_, &QIODevice::readyRead, this, &MicCapture::onReadyRead);
        ioDevice_ = nullptr;
    }

    audioSource_->stop();
    audioSource_.reset();
    callback_ = nullptr;
    currentLevel_ = 0.0f;

    spdlog::info("MicCapture: stopped");
}

bool MicCapture::isCapturing() const {
    return audioSource_ != nullptr && ioDevice_ != nullptr;
}

AudioFormat MicCapture::format() const {
    return format_;
}

float MicCapture::currentLevel() const {
    return currentLevel_;
}

void MicCapture::onReadyRead() {
    if (!ioDevice_) {
        return;
    }

    const QByteArray rawData = ioDevice_->readAll();
    if (rawData.isEmpty()) {
        return;
    }

    const auto totalBytes = static_cast<size_t>(rawData.size());
    const size_t sampleSize = sizeof(float);
    const auto totalSamples = totalBytes / sampleSize;

    if (totalSamples == 0) {
        return;
    }

    // Apply gain boost — create a new buffer (immutable pattern)
    std::vector<float> boostedData(totalSamples);
    const auto* srcSamples = reinterpret_cast<const float*>(rawData.constData());

    for (size_t i = 0; i < totalSamples; ++i) {
        boostedData[i] = static_cast<float>(
            static_cast<double>(srcSamples[i]) * gainBoost_);
    }

    // Compute RMS level for UI meter
    double sumSquares = 0.0;
    for (size_t i = 0; i < totalSamples; ++i) {
        const double sample = static_cast<double>(boostedData[i]);
        sumSquares += sample * sample;
    }
    const double rms = std::sqrt(sumSquares / static_cast<double>(totalSamples));
    const float level = std::min(
        static_cast<float>(rms * kLevelScale) * kMaxLevel,
        kMaxLevel);
    currentLevel_ = level;

    // Build AudioBuffer and invoke callback
    const int frameCount = static_cast<int>(totalSamples)
                           / format_.channels;

    auto now = std::chrono::steady_clock::now();
    auto timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    AudioBuffer buffer;
    buffer.data = boostedData.data();
    buffer.frameCount = frameCount;
    buffer.channels = format_.channels;
    buffer.sampleRate = format_.sampleRate;
    buffer.timestampMs = timestampMs;

    if (callback_) {
        callback_(buffer);
    }
}

} // namespace openscreen
