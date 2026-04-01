#include "capture/WebcamCapture.h"

#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVideoFrame>
#include <QVideoSink>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>

namespace openscreen {

WebcamCapture::WebcamCapture(QObject* parent)
    : QObject(parent)
{
}

WebcamCapture::~WebcamCapture()
{
    stop();
}

QList<WebcamDevice> WebcamCapture::enumerateDevices()
{
    QList<WebcamDevice> devices;
    const auto videoInputs = QMediaDevices::videoInputs();

    for (const auto& cameraDevice : videoInputs) {
        WebcamDevice device;
        device.id = QString::fromUtf8(cameraDevice.id());
        device.name = cameraDevice.description();
        devices.append(device);
    }

    return devices;
}

bool WebcamCapture::start(const QString& deviceId, WebcamFrameCallback callback)
{
    if (capturing_) {
        spdlog::warn("WebcamCapture::start called while already capturing");
        return false;
    }

    if (!callback) {
        spdlog::error("WebcamCapture::start called with null callback");
        return false;
    }

    // Find the requested camera device
    QCameraDevice selectedDevice;

    if (deviceId.isEmpty()) {
        selectedDevice = QMediaDevices::defaultVideoInput();
    } else {
        const auto videoInputs = QMediaDevices::videoInputs();
        for (const auto& device : videoInputs) {
            if (QString::fromUtf8(device.id()) == deviceId) {
                selectedDevice = device;
                break;
            }
        }
    }

    if (selectedDevice.isNull()) {
        spdlog::error("WebcamCapture: no camera found for deviceId='{}'",
                       deviceId.toStdString());
        return false;
    }

    frameCallback_ = std::move(callback);

    // Create camera
    camera_ = std::make_unique<QCamera>(selectedDevice);

    // Find the best matching format for target resolution and fps
    const auto formats = selectedDevice.videoFormats();
    QCameraFormat bestFormat;
    int bestScore = -1;

    for (const auto& format : formats) {
        const auto resolution = format.resolution();
        const float minFps = format.minFrameRate();
        const float maxFps = format.maxFrameRate();

        // Score: prefer closest resolution match, then closest fps match
        const int resDiff = std::abs(resolution.width() - targetWidth_)
                          + std::abs(resolution.height() - targetHeight_);
        const int fpsDiff = static_cast<int>(
            std::abs(maxFps - static_cast<float>(targetFps_)));

        // Lower score is better; invert for comparison
        const int score = 10000 - resDiff - fpsDiff * 10;

        if (score > bestScore) {
            bestScore = score;
            bestFormat = format;
        }
    }

    if (!bestFormat.isNull()) {
        camera_->setCameraFormat(bestFormat);
        spdlog::info("WebcamCapture: selected format {}x{} @ {} fps",
                      bestFormat.resolution().width(),
                      bestFormat.resolution().height(),
                      bestFormat.maxFrameRate());
    }

    // Create session and sink
    session_ = std::make_unique<QMediaCaptureSession>();
    sink_ = std::make_unique<QVideoSink>();

    connect(sink_.get(), &QVideoSink::videoFrameChanged,
            this, &WebcamCapture::onFrameAvailable);

    session_->setCamera(camera_.get());
    session_->setVideoSink(sink_.get());

    camera_->start();
    capturing_ = true;

    spdlog::info("WebcamCapture: started capturing from '{}'",
                  selectedDevice.description().toStdString());

    return true;
}

void WebcamCapture::stop()
{
    if (!capturing_) {
        return;
    }

    if (camera_) {
        camera_->stop();
    }

    // Disconnect before destroying to avoid dangling callbacks
    if (sink_) {
        disconnect(sink_.get(), &QVideoSink::videoFrameChanged,
                   this, &WebcamCapture::onFrameAvailable);
    }

    session_.reset();
    sink_.reset();
    camera_.reset();
    frameCallback_ = nullptr;
    capturing_ = false;

    spdlog::info("WebcamCapture: stopped");
}

bool WebcamCapture::isCapturing() const
{
    return capturing_;
}

void WebcamCapture::setTargetResolution(int width, int height)
{
    targetWidth_ = width;
    targetHeight_ = height;
}

void WebcamCapture::setTargetFrameRate(int fps)
{
    targetFps_ = fps;
}

void WebcamCapture::onFrameAvailable(const QVideoFrame& frame)
{
    if (!frameCallback_ || !frame.isValid()) {
        return;
    }

    QImage image = frame.toImage();
    if (image.isNull()) {
        return;
    }

    // Scale to target resolution if needed
    if (image.width() != targetWidth_ || image.height() != targetHeight_) {
        image = image.scaled(targetWidth_, targetHeight_,
                             Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    const int64_t timestampMs = frame.startTime() / 1000;  // QVideoFrame time is in microseconds
    frameCallback_(image, timestampMs);
}

} // namespace openscreen
