#include "AspectRatio.h"

#include "ZoomTransform.h" // Size

namespace openscreen {

double getAspectRatioValue(AspectRatio ratio) {
    switch (ratio) {
        case AspectRatio::R16_9:  return 16.0 / 9.0;
        case AspectRatio::R9_16:  return 9.0 / 16.0;
        case AspectRatio::R1_1:   return 1.0;
        case AspectRatio::R4_3:   return 4.0 / 3.0;
        case AspectRatio::R4_5:   return 4.0 / 5.0;
        case AspectRatio::R16_10: return 16.0 / 10.0;
        case AspectRatio::R10_16: return 10.0 / 16.0;
        case AspectRatio::Native: return 16.0 / 9.0;
    }
    return 16.0 / 9.0;
}

double getNativeAspectRatioValue(double videoWidth,
                                 double videoHeight,
                                 const CropRegion* cropRegion) {
    const double cropW = cropRegion ? cropRegion->width : 1.0;
    const double cropH = cropRegion ? cropRegion->height : 1.0;
    return (videoWidth * cropW) / (videoHeight * cropH);
}

Size getAspectRatioDimensions(AspectRatio ratio, double baseWidth) {
    const double r = getAspectRatioValue(ratio);
    return Size{baseWidth, baseWidth / r};
}

std::string getAspectRatioLabel(AspectRatio ratio) {
    switch (ratio) {
        case AspectRatio::R16_9:  return "16:9";
        case AspectRatio::R9_16:  return "9:16";
        case AspectRatio::R1_1:   return "1:1";
        case AspectRatio::R4_3:   return "4:3";
        case AspectRatio::R4_5:   return "4:5";
        case AspectRatio::R16_10: return "16:10";
        case AspectRatio::R10_16: return "10:16";
        case AspectRatio::Native: return "Native";
    }
    return "16:9";
}

bool isPortraitAspectRatio(AspectRatio ratio) {
    return getAspectRatioValue(ratio) < 1.0;
}

} // namespace openscreen
