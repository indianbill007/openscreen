#pragma once

#include <deque>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "types.h"

namespace openscreen {

// ---------------------------------------------------------------------------
// EditorState — immutable snapshot of the entire editor configuration
// ---------------------------------------------------------------------------

struct EditorState {
    std::vector<ZoomRegion> zoomRegions;
    std::vector<TrimRegion> trimRegions;
    std::vector<SpeedRegion> speedRegions;
    std::vector<AnnotationRegion> annotationRegions;
    CropRegion cropRegion{0.0, 0.0, 1.0, 1.0};
    std::string wallpaper = "/wallpapers/wallpaper1.jpg";
    double shadowIntensity = 0.0;
    bool showBlur = false;
    double motionBlurAmount = 0.0;
    double borderRadius = 0.0;
    double padding = 50.0;
    AspectRatio aspectRatio = AspectRatio::R16_9;
    WebcamLayoutPreset webcamLayoutPreset = WebcamLayoutPreset::PictureInPicture;
    std::optional<WebcamPosition> webcamPosition;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EditorState,
    zoomRegions, trimRegions, speedRegions, annotationRegions,
    cropRegion, wallpaper, shadowIntensity, showBlur,
    motionBlurAmount, borderRadius, padding,
    aspectRatio, webcamLayoutPreset, webcamPosition)

// ---------------------------------------------------------------------------
// EditorHistory — undo/redo stack storing immutable state snapshots
// ---------------------------------------------------------------------------

inline constexpr std::size_t MAX_HISTORY = 80;

class EditorHistory {
public:
    explicit EditorHistory(EditorState initial = EditorState{});

    /// Current editor state (read-only).
    const EditorState& state() const;

    /// Save a full checkpoint: pushes current state to past, sets new present,
    /// and clears the redo stack.
    void pushState(const EditorState& newState);

    /// Live-update variant: the first call after construction or after
    /// commitState() saves a checkpoint; subsequent calls only replace present.
    void updateState(const EditorState& newState);

    /// Marks the end of a live-update series (e.g. slider drag finished).
    void commitState();

    /// Move one step back. Returns false if nothing to undo.
    bool undo();

    /// Move one step forward. Returns false if nothing to redo.
    bool redo();

    bool canUndo() const;
    bool canRedo() const;

private:
    void checkpoint();

    std::deque<EditorState> past_;
    EditorState present_;
    std::deque<EditorState> future_;
    bool dirty_ = false;
};

} // namespace openscreen
