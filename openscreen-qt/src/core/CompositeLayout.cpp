#include "CompositeLayout.h"

#include <algorithm>
#include <cmath>

#include "ZoomTransform.h" // Size

namespace openscreen {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr double MAX_STAGE_FRACTION = 0.18;
static constexpr double MARGIN_FRACTION    = 0.02;
static constexpr double MAX_BORDER_RADIUS  = 24.0;

// ---------------------------------------------------------------------------
// Preset map (static, lazily initialised on first call)
// ---------------------------------------------------------------------------

static const WebcamLayoutPresetDefinition& pipPreset() {
    static const WebcamLayoutPresetDefinition def{
        "Picture in Picture",
        false, // isStack
        OverlayTransform{MAX_STAGE_FRACTION, MARGIN_FRACTION, 0.0, 0.0},
        StackTransform{0.0},
        BorderRadiusRule{MAX_BORDER_RADIUS, 12.0, 0.12},
        WebcamLayoutShadow{"rgba(0,0,0,0.35)", 24.0, 0.0, 10.0},
    };
    return def;
}

static const WebcamLayoutPresetDefinition& verticalStackPreset() {
    static const WebcamLayoutPresetDefinition def{
        "Vertical Stack",
        true, // isStack
        OverlayTransform{},
        StackTransform{0.0},
        BorderRadiusRule{0.0, 0.0, 0.0},
        std::nullopt, // no shadow
    };
    return def;
}

const WebcamLayoutPresetDefinition& getWebcamLayoutPresetDefinition(
    WebcamLayoutPreset preset) {
    switch (preset) {
        case WebcamLayoutPreset::VerticalStack: return verticalStackPreset();
        case WebcamLayoutPreset::PictureInPicture:
        default: return pipPreset();
    }
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static RenderRect centerRect(const Size& canvasSize,
                             const Size& size,
                             const Size& maxSize) {
    const double canvasWidth  = canvasSize.width;
    const double canvasHeight = canvasSize.height;
    const double w = size.width;
    const double h = size.height;
    const double maxW = maxSize.width;
    const double maxH = maxSize.height;

    const double scale = std::min({maxW / w, maxH / h, 1.0});
    const double resolvedWidth  = std::round(w * scale);
    const double resolvedHeight = std::round(h * scale);

    return RenderRect{
        std::max(0.0, std::floor((canvasWidth  - resolvedWidth)  / 2.0)),
        std::max(0.0, std::floor((canvasHeight - resolvedHeight) / 2.0)),
        resolvedWidth,
        resolvedHeight,
    };
}

// ---------------------------------------------------------------------------
// computeCompositeLayout
// ---------------------------------------------------------------------------

std::optional<WebcamCompositeLayout> computeCompositeLayout(
    const Size& canvasSize,
    const Size& screenSize,
    const std::optional<Size>& webcamSize,
    WebcamLayoutPreset preset,
    const std::optional<WebcamPosition>& webcamPosition,
    const std::optional<Size>& maxContentSize) {

    const double canvasWidth  = canvasSize.width;
    const double canvasHeight = canvasSize.height;
    const double screenWidth  = screenSize.width;
    const double screenHeight = screenSize.height;

    if (canvasWidth <= 0 || canvasHeight <= 0 ||
        screenWidth <= 0 || screenHeight <= 0) {
        return std::nullopt;
    }

    const auto& presetDef = getWebcamLayoutPresetDefinition(preset);

    const bool hasWebcam = webcamSize.has_value() &&
                           webcamSize->width > 0 &&
                           webcamSize->height > 0;

    // ----- Stack layout -----
    if (presetDef.isStack) {
        if (!hasWebcam) {
            // No webcam -- screen fills the entire canvas (cover mode)
            return WebcamCompositeLayout{
                RenderRect{0.0, 0.0, canvasWidth, canvasHeight},
                std::nullopt,
                true,
            };
        }

        const double webcamAspect = webcamSize->width / webcamSize->height;
        const double resolvedWebcamWidth  = canvasWidth;
        const double resolvedWebcamHeight = std::round(canvasWidth / webcamAspect);

        const double screenRectHeight = canvasHeight - resolvedWebcamHeight;

        StyledRenderRect webcamRect;
        webcamRect.x            = 0.0;
        webcamRect.y            = std::max(0.0, screenRectHeight);
        webcamRect.width        = resolvedWebcamWidth;
        webcamRect.height       = resolvedWebcamHeight;
        webcamRect.borderRadius = 0.0;

        return WebcamCompositeLayout{
            RenderRect{0.0, 0.0, canvasWidth, std::max(0.0, screenRectHeight)},
            webcamRect,
            true,
        };
    }

    // ----- Overlay layout -----
    const Size effectiveMaxSize = maxContentSize.value_or(canvasSize);

    const RenderRect screenRect = centerRect(canvasSize, screenSize, effectiveMaxSize);

    if (!hasWebcam) {
        return WebcamCompositeLayout{screenRect, std::nullopt, false};
    }

    const auto& transform = presetDef.overlay;
    const double webcamW = webcamSize->width;
    const double webcamH = webcamSize->height;

    const double margin = std::max(
        transform.minMargin,
        static_cast<double>(std::round(
            std::min(canvasWidth, canvasHeight) * transform.marginFraction)));

    const double maxWidth  = std::max(transform.minSize,
                                      canvasWidth * transform.maxStageFraction);
    const double maxHeight = std::max(transform.minSize,
                                      canvasHeight * transform.maxStageFraction);

    const double scale = std::min(maxWidth / webcamW, maxHeight / webcamH);
    const double width  = std::round(webcamW * scale);
    const double height = std::round(webcamH * scale);

    double webcamX = 0.0;
    double webcamY = 0.0;

    if (webcamPosition.has_value()) {
        // Custom position: cx/cy represent the center of the webcam as a
        // fraction of the canvas.
        webcamX = std::round(webcamPosition->cx * canvasWidth  - width  / 2.0);
        webcamY = std::round(webcamPosition->cy * canvasHeight - height / 2.0);
        // Clamp to stay within canvas bounds
        webcamX = std::max(0.0, std::min(canvasWidth  - width,  webcamX));
        webcamY = std::max(0.0, std::min(canvasHeight - height, webcamY));
    } else {
        // Default: bottom-right with margin
        webcamX = std::max(0.0, std::round(canvasWidth  - margin - width));
        webcamY = std::max(0.0, std::round(canvasHeight - margin - height));
    }

    const double borderRadius = std::min(
        presetDef.borderRadius.max,
        std::max(
            presetDef.borderRadius.min,
            std::round(std::min(width, height) * presetDef.borderRadius.fraction)));

    StyledRenderRect webcamRect;
    webcamRect.x            = webcamX;
    webcamRect.y            = webcamY;
    webcamRect.width        = width;
    webcamRect.height       = height;
    webcamRect.borderRadius = borderRadius;

    return WebcamCompositeLayout{screenRect, webcamRect, false};
}

} // namespace openscreen
