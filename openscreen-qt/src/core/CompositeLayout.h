#pragma once

#include <optional>
#include <string>

#include "types.h"

namespace openscreen {

struct Size; // forward-declared from ZoomTransform.h

// ---------------------------------------------------------------------------
// Layout geometry types
// ---------------------------------------------------------------------------

struct RenderRect {
    double x      = 0.0;
    double y      = 0.0;
    double width  = 0.0;
    double height = 0.0;
};

struct StyledRenderRect : RenderRect {
    double borderRadius = 0.0;
};

// ---------------------------------------------------------------------------
// Preset definition types
// ---------------------------------------------------------------------------

struct WebcamLayoutShadow {
    std::string color;
    double blur    = 0.0;
    double offsetX = 0.0;
    double offsetY = 0.0;
};

struct BorderRadiusRule {
    double max      = 0.0;
    double min      = 0.0;
    double fraction = 0.0;
};

struct OverlayTransform {
    double maxStageFraction = 0.0;
    double marginFraction   = 0.0;
    double minMargin        = 0.0;
    double minSize          = 0.0;
};

struct StackTransform {
    double gap = 0.0;
};

struct WebcamLayoutPresetDefinition {
    std::string label;
    bool isStack = false; // true = stack, false = overlay
    OverlayTransform overlay;
    StackTransform stack;
    BorderRadiusRule borderRadius;
    std::optional<WebcamLayoutShadow> shadow;
};

// ---------------------------------------------------------------------------
// Composite layout result
// ---------------------------------------------------------------------------

struct WebcamCompositeLayout {
    RenderRect screenRect;
    std::optional<StyledRenderRect> webcamRect;
    bool screenCover = false;
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Returns the preset definition for a given webcam layout preset.
const WebcamLayoutPresetDefinition& getWebcamLayoutPresetDefinition(
    WebcamLayoutPreset preset);

/// Computes the composite layout for screen + optional webcam rendering.
/// Returns std::nullopt when inputs are invalid (zero/negative dimensions).
std::optional<WebcamCompositeLayout> computeCompositeLayout(
    const Size& canvasSize,
    const Size& screenSize,
    const std::optional<Size>& webcamSize,
    WebcamLayoutPreset preset = WebcamLayoutPreset::PictureInPicture,
    const std::optional<WebcamPosition>& webcamPosition = std::nullopt,
    const std::optional<Size>& maxContentSize = std::nullopt);

} // namespace openscreen
