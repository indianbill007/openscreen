#pragma once

#include <cmath>

#include "types.h"

namespace openscreen {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

inline constexpr double PEAK_VELOCITY_PPS = 1400.0;
inline constexpr double MAX_BLUR_PX = 14.0;
inline constexpr double VELOCITY_THRESHOLD_PPS = 12.0;
inline constexpr double MAX_AMOUNT_BOOST = 2.2;

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

struct Size {
    double width = 0.0;
    double height = 0.0;
};

struct Rect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct AppliedTransform {
    double scale = 1.0;
    double x = 0.0;
    double y = 0.0;
};

struct MotionBlurState {
    double lastFrameTimeMs = 0.0;
    double prevCamX = 0.0;
    double prevCamY = 0.0;
    double prevCamScale = 0.0;
    bool initialized = false;
};

struct MotionBlurResult {
    double velocityX = 0.0;
    double velocityY = 0.0;
    double targetBlur = 0.0;
    double kernelSize = 0.0;
};

struct FocusBounds {
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
};

// ---------------------------------------------------------------------------
// Zoom transform functions
// ---------------------------------------------------------------------------

double getMotionBlurAmountResponse(double motionBlurAmount);

AppliedTransform computeZoomTransform(const Size& stageSize,
                                      const Rect& baseMask,
                                      double zoomScale,
                                      double zoomProgress,
                                      double focusX,
                                      double focusY);

ZoomFocus computeFocusFromTransform(const Size& stageSize,
                                    const Rect& baseMask,
                                    double zoomScale,
                                    double x,
                                    double y);

MotionBlurResult computeMotionBlur(MotionBlurState& state,
                                   const AppliedTransform& transform,
                                   const Size& stageSize,
                                   double motionBlurAmount,
                                   double frameTimeMs);

// ---------------------------------------------------------------------------
// Focus utility functions
// ---------------------------------------------------------------------------

double easeIntoBoundary(double normalized);

double softClampToRange(double value, double min, double max, double softness);

FocusBounds getFocusBoundsForScale(double zoomScale);

ZoomFocus clampFocusToStage(const ZoomFocus& focus, ZoomDepth depth);

ZoomFocus clampFocusToScale(const ZoomFocus& focus, double zoomScale);

ZoomFocus softenFocusToScale(const ZoomFocus& focus, double zoomScale);

ZoomFocus stageFocusToVideoSpace(const ZoomFocus& focus,
                                 const Size& stageSize,
                                 const Size& videoSize,
                                 double baseScale,
                                 double baseOffsetX,
                                 double baseOffsetY);

} // namespace openscreen
