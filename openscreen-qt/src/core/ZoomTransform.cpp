#include "ZoomTransform.h"

#include <algorithm>
#include <cmath>

namespace openscreen {

// ---------------------------------------------------------------------------
// Zoom transform functions
// ---------------------------------------------------------------------------

double getMotionBlurAmountResponse(double motionBlurAmount) {
    const double clampedAmount = clamp(motionBlurAmount, 0.0, 1.0);
    return clampedAmount * (1.0 + (MAX_AMOUNT_BOOST - 1.0) * clampedAmount);
}

AppliedTransform computeZoomTransform(const Size& stageSize,
                                      const Rect& baseMask,
                                      double zoomScale,
                                      double zoomProgress,
                                      double focusX,
                                      double focusY) {
    if (stageSize.width <= 0.0 || stageSize.height <= 0.0 ||
        baseMask.width <= 0.0 || baseMask.height <= 0.0) {
        return {1.0, 0.0, 0.0};
    }

    const double progress = clamp(zoomProgress, 0.0, 1.0);
    const double focusStagePxX = baseMask.x + focusX * baseMask.width;
    const double focusStagePxY = baseMask.y + focusY * baseMask.height;
    const double stageCenterX = stageSize.width / 2.0;
    const double stageCenterY = stageSize.height / 2.0;
    const double scale = 1.0 + (zoomScale - 1.0) * progress;
    const double finalX = stageCenterX - focusStagePxX * zoomScale;
    const double finalY = stageCenterY - focusStagePxY * zoomScale;

    return {scale, finalX * progress, finalY * progress};
}

ZoomFocus computeFocusFromTransform(const Size& stageSize,
                                    const Rect& baseMask,
                                    double zoomScale,
                                    double x,
                                    double y) {
    if (stageSize.width <= 0.0 || stageSize.height <= 0.0 ||
        baseMask.width <= 0.0 || baseMask.height <= 0.0 || zoomScale <= 0.0) {
        return {0.5, 0.5};
    }

    const double stageCenterX = stageSize.width / 2.0;
    const double stageCenterY = stageSize.height / 2.0;
    const double focusStagePxX = (stageCenterX - x) / zoomScale;
    const double focusStagePxY = (stageCenterY - y) / zoomScale;

    return {
        (focusStagePxX - baseMask.x) / baseMask.width,
        (focusStagePxY - baseMask.y) / baseMask.height,
    };
}

MotionBlurResult computeMotionBlur(MotionBlurState& state,
                                   const AppliedTransform& transform,
                                   const Size& stageSize,
                                   double motionBlurAmount,
                                   double frameTimeMs) {
    if (!state.initialized) {
        state.lastFrameTimeMs = frameTimeMs;
        state.prevCamX = transform.x;
        state.prevCamY = transform.y;
        state.prevCamScale = transform.scale;
        state.initialized = true;
        return {0.0, 0.0, 0.0, 0.0};
    }

    const double dtMs = clamp(frameTimeMs - state.lastFrameTimeMs, 1.0, 80.0);
    const double dtSeconds = dtMs / 1000.0;

    const double dx = transform.x - state.prevCamX;
    const double dy = transform.y - state.prevCamY;
    const double dScale = transform.scale - state.prevCamScale;

    const double velocityX = dx / dtSeconds;
    const double velocityY = dy / dtSeconds;
    const double scaleVelocity =
        std::abs(dScale / dtSeconds) *
        std::max(stageSize.width, stageSize.height) * 0.5;

    const double speed =
        std::sqrt(velocityX * velocityX + velocityY * velocityY) +
        scaleVelocity;

    const double normalised =
        std::min(1.0, speed / PEAK_VELOCITY_PPS);

    const double amountResponse = getMotionBlurAmountResponse(motionBlurAmount);

    const double targetBlur =
        (speed < VELOCITY_THRESHOLD_PPS)
            ? 0.0
            : normalised * normalised * MAX_BLUR_PX * amountResponse;

    // Update state for next frame
    state.lastFrameTimeMs = frameTimeMs;
    state.prevCamX = transform.x;
    state.prevCamY = transform.y;
    state.prevCamScale = transform.scale;

    return {velocityX, velocityY, targetBlur, targetBlur};
}

// ---------------------------------------------------------------------------
// Focus utility functions
// ---------------------------------------------------------------------------

double easeIntoBoundary(double normalized) {
    const double t = clamp(normalized, 0.0, 1.0);
    return -t * t * t + 2.0 * t * t;
}

double softClampToRange(double value, double min, double max, double softness) {
    const double clamped = clamp(value, min, max);
    if (softness <= 0.0 || max <= min) {
        return clamped;
    }
    if (clamped < min + softness) {
        const double normalized = (clamped - min) / softness;
        return min + softness * easeIntoBoundary(normalized);
    }
    if (clamped > max - softness) {
        const double normalized = (max - clamped) / softness;
        return max - softness * easeIntoBoundary(normalized);
    }
    return clamped;
}

FocusBounds getFocusBoundsForScale(double zoomScale) {
    const double marginX = 1.0 / (2.0 * zoomScale);
    const double marginY = 1.0 / (2.0 * zoomScale);
    return {marginX, 1.0 - marginX, marginY, 1.0 - marginY};
}

ZoomFocus clampFocusToStage(const ZoomFocus& focus, ZoomDepth depth) {
    const ZoomFocus baseFocus = clampFocusToDepth(focus, depth);
    const double scale = zoomDepthScale(depth);
    const FocusBounds bounds = getFocusBoundsForScale(scale);
    return {
        clamp(baseFocus.cx, bounds.minX, bounds.maxX),
        clamp(baseFocus.cy, bounds.minY, bounds.maxY),
    };
}

ZoomFocus clampFocusToScale(const ZoomFocus& focus, double zoomScale) {
    const ZoomFocus baseFocus = {clamp(focus.cx, 0.0, 1.0),
                                 clamp(focus.cy, 0.0, 1.0)};
    const FocusBounds bounds = getFocusBoundsForScale(zoomScale);
    return {
        clamp(baseFocus.cx, bounds.minX, bounds.maxX),
        clamp(baseFocus.cy, bounds.minY, bounds.maxY),
    };
}

ZoomFocus softenFocusToScale(const ZoomFocus& focus, double zoomScale) {
    const ZoomFocus baseFocus = {clamp(focus.cx, 0.0, 1.0),
                                 clamp(focus.cy, 0.0, 1.0)};
    const FocusBounds bounds = getFocusBoundsForScale(zoomScale);
    const double horizontalRange = bounds.maxX - bounds.minX;
    const double verticalRange = bounds.maxY - bounds.minY;
    const double horizontalSoftness = std::min(0.12, horizontalRange * 0.35);
    const double verticalSoftness = std::min(0.12, verticalRange * 0.35);
    return {
        softClampToRange(baseFocus.cx, bounds.minX, bounds.maxX,
                         horizontalSoftness),
        softClampToRange(baseFocus.cy, bounds.minY, bounds.maxY,
                         verticalSoftness),
    };
}

ZoomFocus stageFocusToVideoSpace(const ZoomFocus& focus,
                                 const Size& stageSize,
                                 const Size& videoSize,
                                 double baseScale,
                                 double baseOffsetX,
                                 double baseOffsetY) {
    if (stageSize.width == 0.0 || stageSize.height == 0.0 ||
        videoSize.width == 0.0 || videoSize.height == 0.0 ||
        baseScale <= 0.0) {
        return focus;
    }

    const double stageX = focus.cx * stageSize.width;
    const double stageY = focus.cy * stageSize.height;
    const double videoNormX =
        (stageX - baseOffsetX) / (videoSize.width * baseScale);
    const double videoNormY =
        (stageY - baseOffsetY) / (videoSize.height * baseScale);

    return {videoNormX, videoNormY};
}

} // namespace openscreen
