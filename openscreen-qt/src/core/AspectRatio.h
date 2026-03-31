#pragma once

#include <string>

#include "types.h"

// Forward-declare Size from ZoomTransform.h to avoid pulling in the full header.
// If Size is already visible via a shared header, this forward declaration is harmless.
namespace openscreen {
struct Size;
}

namespace openscreen {

// ---------------------------------------------------------------------------
// Aspect-ratio utilities (ported from aspectRatioUtils.ts)
// ---------------------------------------------------------------------------

/// Returns the numeric value of an aspect ratio (width / height).
/// For AspectRatio::Native, returns 16/9 as a fallback.
/// Callers with source/crop context should use getNativeAspectRatioValue().
double getAspectRatioValue(AspectRatio ratio);

/// Returns the effective native aspect ratio given source dimensions and an
/// optional crop region (fractional).
double getNativeAspectRatioValue(double videoWidth,
                                double videoHeight,
                                const CropRegion* cropRegion = nullptr);

/// Returns {width, height} for the given aspect ratio, where width == baseWidth.
Size getAspectRatioDimensions(AspectRatio ratio, double baseWidth);

/// Returns a human-readable label: "Native" for native, "16:9" etc. for the rest.
std::string getAspectRatioLabel(AspectRatio ratio);

/// Returns true when the aspect ratio is portrait (value < 1).
bool isPortraitAspectRatio(AspectRatio ratio);

} // namespace openscreen
