#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <QPixmap>

namespace openscreen {

struct CaptureFrame {
    const uint8_t* data = nullptr;   // BGRA pixel data
    int width = 0;
    int height = 0;
    int stride = 0;                   // bytes per row
    int64_t timestampMs = 0;
};

struct CaptureSource {
    std::string id;
    std::string name;
    bool isWindow = false;           // true = window, false = display/monitor
    int width = 0;
    int height = 0;
    QPixmap thumbnail;               // for source selector UI
};

using FrameCallback = std::function<void(const CaptureFrame&)>;
using CursorCallback = std::function<void(double cx, double cy, int64_t timestampMs)>;

class ScreenCapture {
public:
    virtual ~ScreenCapture() = default;

    /// Enumerate available screens and windows
    virtual std::vector<CaptureSource> enumerateSources() = 0;

    /// Select a source to capture
    virtual bool selectSource(const std::string& sourceId) = 0;

    /// Start capturing frames. Calls frameCallback on each frame (from a worker thread).
    virtual bool start(FrameCallback frameCallback) = 0;

    /// Stop capturing
    virtual void stop() = 0;

    /// Whether currently capturing
    virtual bool isCapturing() const = 0;

    /// Set cursor position callback (called every ~100ms during capture)
    virtual void setCursorCallback(CursorCallback callback) = 0;

    /// Factory: creates the platform-appropriate implementation
    static std::unique_ptr<ScreenCapture> create();
};

} // namespace openscreen
