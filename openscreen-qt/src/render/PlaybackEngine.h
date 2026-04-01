#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

#include <cstdint>
#include <memory>
#include <string>

namespace openscreen {

class VideoDecoder;
struct DecodedFrame;
struct VideoInfo;
struct EditorState;
struct ZoomRegion;
struct ZoomFocus;

class PlaybackEngine : public QObject {
    Q_OBJECT
public:
    explicit PlaybackEngine(QObject* parent = nullptr);
    ~PlaybackEngine() override;

    PlaybackEngine(const PlaybackEngine&) = delete;
    PlaybackEngine& operator=(const PlaybackEngine&) = delete;

    /// Load a video file for playback
    bool loadVideo(const std::string& filePath);

    /// Unload current video and reset state
    void unloadVideo();

    /// Whether a video is currently loaded
    bool isLoaded() const;

    /// Get video metadata (width, height, fps, duration, etc.)
    VideoInfo videoInfo() const;

    // -- Playback control -----------------------------------------------------

    void play();
    void pause();
    void togglePlayPause();
    bool isPlaying() const;

    /// Seek to a specific time in milliseconds
    void seekTo(int64_t timeMs);

    /// Get current playback position in milliseconds
    int64_t currentTimeMs() const;

    /// Get total duration in milliseconds
    int64_t durationMs() const;

    // -- Editor state ---------------------------------------------------------

    /// Set the editor state used for zoom/trim/speed lookups.
    /// The caller retains ownership; the pointer must remain valid while set.
    void setEditorState(const EditorState* state);

signals:
    void videoLoaded(const QString& filePath);
    void videoUnloaded();
    void frameReady(const uint8_t* bgraData, int width, int height, int stride);
    void positionChanged(int64_t timeMs);
    void playbackStateChanged(bool playing);
    void zoomChanged(double scale, double focusX, double focusY, double progress);
    void wallpaperChanged(const QString& wallpaper);

private slots:
    void onPlaybackTick();

private:
    void decodeAndEmitFrame(int64_t targetMs);
    void updateZoomFromState(int64_t timeMs);
    const ZoomRegion* findActiveZoomRegion(int64_t timeMs) const;
    bool isTimeTrimmed(int64_t timeMs) const;
    double getPlaybackSpeed(int64_t timeMs) const;

    std::unique_ptr<VideoDecoder> decoder_;
    const EditorState* editorState_ = nullptr;

    QTimer playbackTimer_;
    QElapsedTimer wallClock_;
    int64_t currentTimeMs_ = 0;
    int64_t durationMs_ = 0;
    bool playing_ = false;
    bool loaded_ = false;

    // Playback timing
    double baseIntervalMs_ = 16.667; // ~60 fps
    int64_t lastTickTimeMs_ = 0;
};

} // namespace openscreen
