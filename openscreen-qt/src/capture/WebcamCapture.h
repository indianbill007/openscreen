#pragma once
#include <QObject>
#include <QImage>
#include <functional>
#include <memory>

class QCamera;
class QMediaCaptureSession;
class QVideoSink;
class QVideoFrame;

namespace openscreen {

struct WebcamDevice {
    QString id;
    QString name;
};

using WebcamFrameCallback = std::function<void(const QImage& frame, int64_t timestampMs)>;

class WebcamCapture : public QObject {
    Q_OBJECT

public:
    explicit WebcamCapture(QObject* parent = nullptr);
    ~WebcamCapture() override;

    /// List available webcam devices
    static QList<WebcamDevice> enumerateDevices();

    /// Start capturing from the specified device (empty = default camera)
    bool start(const QString& deviceId, WebcamFrameCallback callback);

    /// Stop capturing
    void stop();

    /// Whether currently capturing
    bool isCapturing() const;

    /// Target resolution (default: 1280x720)
    void setTargetResolution(int width, int height);

    /// Target frame rate (default: 30)
    void setTargetFrameRate(int fps);

private slots:
    void onFrameAvailable(const QVideoFrame& frame);

private:
    std::unique_ptr<QCamera> camera_;
    std::unique_ptr<QMediaCaptureSession> session_;
    std::unique_ptr<QVideoSink> sink_;
    WebcamFrameCallback frameCallback_;
    bool capturing_ = false;
    int targetWidth_ = 1280;
    int targetHeight_ = 720;
    int targetFps_ = 30;
};

} // namespace openscreen
