#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace openscreen {

class ScreenCapture;
class AudioCapture;
class MicCapture;
class AudioMixer;
class WebcamCapture;
class RecordingEncoder;
struct CaptureSource;
struct CursorTelemetryPoint;

class RecordingSession : public QObject {
    Q_OBJECT

public:
    explicit RecordingSession(QObject* parent = nullptr);
    ~RecordingSession() override;

    // Source management
    std::vector<CaptureSource> enumerateSources();
    bool selectSource(const std::string& sourceId);

    // Recording control
    bool startRecording(const std::string& outputPath);
    void stopRecording();
    bool isRecording() const;

    // Audio options
    void setMicrophoneEnabled(bool enabled);
    void setMicrophoneDevice(const QString& deviceId);
    void setSystemAudioEnabled(bool enabled);

    // Webcam options
    void setWebcamEnabled(bool enabled);
    void setWebcamDevice(const QString& deviceId);

    // State queries
    int elapsedSeconds() const;
    float micLevel() const;
    std::string outputPath() const;
    std::vector<CursorTelemetryPoint> cursorTelemetry() const;

signals:
    void recordingStarted();
    void recordingStopped(const QString& outputPath);
    void recordingError(const QString& error);
    void elapsedTimeChanged(int seconds);

private slots:
    void onTimerTick();

private:
    std::unique_ptr<ScreenCapture> screenCapture_;
    std::unique_ptr<AudioCapture> systemAudio_;
    std::unique_ptr<MicCapture> micCapture_;
    std::unique_ptr<AudioMixer> audioMixer_;
    std::unique_ptr<WebcamCapture> webcamCapture_;
    std::unique_ptr<RecordingEncoder> encoder_;

    QTimer elapsedTimer_;
    QElapsedTimer elapsedClock_;

    bool micEnabled_ = false;
    bool systemAudioEnabled_ = false;
    bool webcamEnabled_ = false;
    QString micDeviceId_;
    QString webcamDeviceId_;
    std::string outputPath_;
    bool recording_ = false;

    std::vector<CursorTelemetryPoint> cursorTelemetry_;
    mutable std::mutex telemetryMutex_;
};

} // namespace openscreen
