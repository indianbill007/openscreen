#include "render/PlaybackEngine.h"
#include "render/VideoDecoder.h"
#include "core/EditorState.h"
#include "core/ZoomTransform.h"
#include "core/types.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>

namespace openscreen {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

PlaybackEngine::PlaybackEngine(QObject* parent)
    : QObject(parent) {
    playbackTimer_.setTimerType(Qt::PreciseTimer);
    playbackTimer_.setInterval(static_cast<int>(std::round(baseIntervalMs_)));
    connect(&playbackTimer_, &QTimer::timeout, this, &PlaybackEngine::onPlaybackTick);
}

PlaybackEngine::~PlaybackEngine() {
    if (playing_) {
        playbackTimer_.stop();
    }
}

// ---------------------------------------------------------------------------
// Video loading
// ---------------------------------------------------------------------------

bool PlaybackEngine::loadVideo(const std::string& filePath) {
    if (loaded_) {
        unloadVideo();
    }

    auto newDecoder = std::make_unique<VideoDecoder>();
    if (!newDecoder->open(filePath)) {
        spdlog::error("PlaybackEngine: failed to open '{}'", filePath);
        return false;
    }

    decoder_ = std::move(newDecoder);
    const auto info = decoder_->info();
    durationMs_ = info.durationMs;
    currentTimeMs_ = 0;

    // Decode the first frame so the preview has something to show immediately.
    decodeAndEmitFrame(0);

    loaded_ = true;
    emit videoLoaded(QString::fromStdString(filePath));
    spdlog::info("PlaybackEngine: loaded '{}' ({}x{}, {:.1f} fps, {} ms)",
                 filePath, info.width, info.height, info.fps, info.durationMs);
    return true;
}

void PlaybackEngine::unloadVideo() {
    if (playing_) {
        pause();
    }

    if (decoder_) {
        decoder_->close();
        decoder_.reset();
    }

    currentTimeMs_ = 0;
    durationMs_ = 0;
    loaded_ = false;

    emit videoUnloaded();
}

bool PlaybackEngine::isLoaded() const {
    return loaded_;
}

VideoInfo PlaybackEngine::videoInfo() const {
    if (decoder_) {
        return decoder_->info();
    }
    return VideoInfo{};
}

// ---------------------------------------------------------------------------
// Playback control
// ---------------------------------------------------------------------------

void PlaybackEngine::play() {
    if (!loaded_ || playing_) {
        return;
    }

    playing_ = true;
    wallClock_.start();
    lastTickTimeMs_ = wallClock_.elapsed();
    playbackTimer_.start();
    emit playbackStateChanged(true);
}

void PlaybackEngine::pause() {
    if (!playing_) {
        return;
    }

    playing_ = false;
    playbackTimer_.stop();
    emit playbackStateChanged(false);
}

void PlaybackEngine::togglePlayPause() {
    if (playing_) {
        pause();
    } else {
        play();
    }
}

bool PlaybackEngine::isPlaying() const {
    return playing_;
}

void PlaybackEngine::seekTo(int64_t timeMs) {
    if (!loaded_ || !decoder_) {
        return;
    }

    // Clamp to valid range
    const int64_t clamped = std::clamp(timeMs, int64_t{0}, durationMs_);

    auto frame = decoder_->seekAndDecode(clamped);
    if (frame) {
        currentTimeMs_ = frame->timestampMs;
        emit frameReady(frame->data.data(), frame->width, frame->height, frame->stride);
    } else {
        currentTimeMs_ = clamped;
    }

    emit positionChanged(currentTimeMs_);
    updateZoomFromState(currentTimeMs_);
}

int64_t PlaybackEngine::currentTimeMs() const {
    return currentTimeMs_;
}

int64_t PlaybackEngine::durationMs() const {
    return durationMs_;
}

// ---------------------------------------------------------------------------
// Editor state
// ---------------------------------------------------------------------------

void PlaybackEngine::setEditorState(const EditorState* state) {
    editorState_ = state;

    if (state) {
        emit wallpaperChanged(QString::fromStdString(state->wallpaper));
    }

    // Re-apply zoom for the current position when the state changes.
    updateZoomFromState(currentTimeMs_);
}

// ---------------------------------------------------------------------------
// Tick — the heartbeat of playback
// ---------------------------------------------------------------------------

void PlaybackEngine::onPlaybackTick() {
    if (!playing_ || !loaded_ || !decoder_) {
        return;
    }

    const int64_t nowMs = wallClock_.elapsed();
    const int64_t elapsedMs = nowMs - lastTickTimeMs_;
    lastTickTimeMs_ = nowMs;

    const double speed = getPlaybackSpeed(currentTimeMs_);
    int64_t advanceMs = static_cast<int64_t>(std::round(elapsedMs * speed));
    int64_t targetMs = currentTimeMs_ + advanceMs;

    // Skip over trimmed regions
    if (editorState_) {
        // Keep jumping forward while the target falls inside a trimmed region.
        bool jumped = true;
        while (jumped) {
            jumped = false;
            for (const auto& trim : editorState_->trimRegions) {
                if (targetMs >= trim.startMs && targetMs < trim.endMs) {
                    targetMs = trim.endMs;
                    jumped = true;
                }
            }
        }
    }

    // End-of-video handling
    if (targetMs >= durationMs_) {
        pause();
        seekTo(0);
        return;
    }

    // Decode and emit the frame
    decodeAndEmitFrame(targetMs);
    currentTimeMs_ = targetMs;

    updateZoomFromState(currentTimeMs_);
    emit positionChanged(currentTimeMs_);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void PlaybackEngine::decodeAndEmitFrame(int64_t targetMs) {
    if (!decoder_) {
        return;
    }

    auto frame = decoder_->seekAndDecode(targetMs);
    if (frame) {
        emit frameReady(frame->data.data(), frame->width, frame->height, frame->stride);
    }
}

void PlaybackEngine::updateZoomFromState(int64_t timeMs) {
    const ZoomRegion* region = findActiveZoomRegion(timeMs);
    if (region) {
        const int64_t regionLength = region->endMs - region->startMs;
        const double progress = (regionLength > 0)
            ? static_cast<double>(timeMs - region->startMs) / regionLength
            : 0.0;
        const double scale = zoomDepthScale(region->depth);
        emit zoomChanged(scale, region->focus.cx, region->focus.cy, progress);
    } else {
        // No active zoom — reset to identity (1x, centred, 0 progress)
        emit zoomChanged(1.0, 0.5, 0.5, 0.0);
    }
}

const ZoomRegion* PlaybackEngine::findActiveZoomRegion(int64_t timeMs) const {
    if (!editorState_) {
        return nullptr;
    }

    // If multiple regions overlap, pick the one that started most recently.
    const ZoomRegion* best = nullptr;
    for (const auto& region : editorState_->zoomRegions) {
        if (timeMs >= region.startMs && timeMs < region.endMs) {
            if (!best || region.startMs > best->startMs) {
                best = &region;
            }
        }
    }
    return best;
}

bool PlaybackEngine::isTimeTrimmed(int64_t timeMs) const {
    if (!editorState_) {
        return false;
    }

    for (const auto& trim : editorState_->trimRegions) {
        if (timeMs >= trim.startMs && timeMs < trim.endMs) {
            return true;
        }
    }
    return false;
}

double PlaybackEngine::getPlaybackSpeed(int64_t timeMs) const {
    if (!editorState_) {
        return 1.0;
    }

    for (const auto& speedRegion : editorState_->speedRegions) {
        if (timeMs >= speedRegion.startMs && timeMs < speedRegion.endMs) {
            return playbackSpeedValue(speedRegion.speed);
        }
    }
    return 1.0;
}

} // namespace openscreen
