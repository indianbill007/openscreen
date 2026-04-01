#pragma once

#include "AudioCapture.h"

#include <QAudioDevice>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>
#include <QObject>

#include <memory>

namespace openscreen {

struct MicDevice {
    QString id;
    QString name;
};

class MicCapture : public QObject {
    Q_OBJECT

public:
    explicit MicCapture(QObject* parent = nullptr);
    ~MicCapture() override;

    MicCapture(const MicCapture&) = delete;
    MicCapture& operator=(const MicCapture&) = delete;

    /// List all available microphone devices
    static QList<MicDevice> enumerateDevices();

    /// Start capturing from the specified device (empty = default)
    bool start(const QString& deviceId, AudioCallback callback);

    /// Stop capturing
    void stop();

    /// Whether currently capturing
    bool isCapturing() const;

    /// Current audio format
    AudioFormat format() const;

    /// RMS-based audio level for UI meter (0-100)
    float currentLevel() const;

private slots:
    void onReadyRead();

private:
    QAudioDevice findDevice(const QString& deviceId) const;

    std::unique_ptr<QAudioSource> audioSource_;
    QIODevice* ioDevice_ = nullptr;
    AudioCallback callback_;
    AudioFormat format_;
    float currentLevel_ = 0.0f;
    double gainBoost_ = 1.4;  // MIC_GAIN_BOOST from Electron version
};

} // namespace openscreen
