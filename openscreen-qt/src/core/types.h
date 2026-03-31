#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace openscreen {

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

inline double clamp(double value, double min, double max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

enum class ZoomDepth { D1 = 1, D2 = 2, D3 = 3, D4 = 4, D5 = 5, D6 = 6 };

enum class AnnotationType { Text, Image, Figure };

enum class ArrowDirection {
    Up,
    Down,
    Left,
    Right,
    UpRight,
    UpLeft,
    DownRight,
    DownLeft
};

enum class FontWeight { Normal, Bold };

enum class FontStyle { Normal, Italic };

enum class TextDecoration { None, Underline };

enum class TextAlign { Left, Center, Right };

enum class PlaybackSpeed {
    X0_25,
    X0_50,
    X0_75,
    X1_25,
    X1_50,
    X1_75,
    X2_00
};

enum class WebcamLayoutPreset { PictureInPicture, VerticalStack };

enum class AspectRatio {
    R16_9,
    R9_16,
    R1_1,
    R4_3,
    R4_5,
    R16_10,
    R10_16,
    Native
};

// ---------------------------------------------------------------------------
// Enum ↔ JSON helpers
// ---------------------------------------------------------------------------

NLOHMANN_JSON_SERIALIZE_ENUM(ZoomDepth, {
    {ZoomDepth::D1, 1},
    {ZoomDepth::D2, 2},
    {ZoomDepth::D3, 3},
    {ZoomDepth::D4, 4},
    {ZoomDepth::D5, 5},
    {ZoomDepth::D6, 6},
})

NLOHMANN_JSON_SERIALIZE_ENUM(AnnotationType, {
    {AnnotationType::Text, "text"},
    {AnnotationType::Image, "image"},
    {AnnotationType::Figure, "figure"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(ArrowDirection, {
    {ArrowDirection::Up, "up"},
    {ArrowDirection::Down, "down"},
    {ArrowDirection::Left, "left"},
    {ArrowDirection::Right, "right"},
    {ArrowDirection::UpRight, "up-right"},
    {ArrowDirection::UpLeft, "up-left"},
    {ArrowDirection::DownRight, "down-right"},
    {ArrowDirection::DownLeft, "down-left"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(FontWeight, {
    {FontWeight::Normal, "normal"},
    {FontWeight::Bold, "bold"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(FontStyle, {
    {FontStyle::Normal, "normal"},
    {FontStyle::Italic, "italic"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(TextDecoration, {
    {TextDecoration::None, "none"},
    {TextDecoration::Underline, "underline"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(TextAlign, {
    {TextAlign::Left, "left"},
    {TextAlign::Center, "center"},
    {TextAlign::Right, "right"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(PlaybackSpeed, {
    {PlaybackSpeed::X0_25, 0.25},
    {PlaybackSpeed::X0_50, 0.5},
    {PlaybackSpeed::X0_75, 0.75},
    {PlaybackSpeed::X1_25, 1.25},
    {PlaybackSpeed::X1_50, 1.5},
    {PlaybackSpeed::X1_75, 1.75},
    {PlaybackSpeed::X2_00, 2.0},
})

NLOHMANN_JSON_SERIALIZE_ENUM(WebcamLayoutPreset, {
    {WebcamLayoutPreset::PictureInPicture, "picture-in-picture"},
    {WebcamLayoutPreset::VerticalStack, "vertical-stack"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(AspectRatio, {
    {AspectRatio::R16_9, "16:9"},
    {AspectRatio::R9_16, "9:16"},
    {AspectRatio::R1_1, "1:1"},
    {AspectRatio::R4_3, "4:3"},
    {AspectRatio::R4_5, "4:5"},
    {AspectRatio::R16_10, "16:10"},
    {AspectRatio::R10_16, "10:16"},
    {AspectRatio::Native, "native"},
})

// ---------------------------------------------------------------------------
// Enum → numeric value helpers
// ---------------------------------------------------------------------------

inline double playbackSpeedValue(PlaybackSpeed s) {
    switch (s) {
        case PlaybackSpeed::X0_25: return 0.25;
        case PlaybackSpeed::X0_50: return 0.50;
        case PlaybackSpeed::X0_75: return 0.75;
        case PlaybackSpeed::X1_25: return 1.25;
        case PlaybackSpeed::X1_50: return 1.50;
        case PlaybackSpeed::X1_75: return 1.75;
        case PlaybackSpeed::X2_00: return 2.00;
    }
    return 1.0;
}

inline double zoomDepthScale(ZoomDepth d) {
    switch (d) {
        case ZoomDepth::D1: return 1.25;
        case ZoomDepth::D2: return 1.50;
        case ZoomDepth::D3: return 1.80;
        case ZoomDepth::D4: return 2.20;
        case ZoomDepth::D5: return 3.50;
        case ZoomDepth::D6: return 5.00;
    }
    return 1.0;
}

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

struct WebcamPosition {
    double cx = 0.0;
    double cy = 0.0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(WebcamPosition, cx, cy)

struct ZoomFocus {
    double cx = 0.0;
    double cy = 0.0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ZoomFocus, cx, cy)

struct ZoomRegion {
    std::string id;
    int startMs = 0;
    int endMs = 0;
    ZoomDepth depth = ZoomDepth::D3;
    ZoomFocus focus;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ZoomRegion, id, startMs, endMs, depth, focus)

struct CursorTelemetryPoint {
    int timeMs = 0;
    double cx = 0.0;
    double cy = 0.0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CursorTelemetryPoint, timeMs, cx, cy)

struct TrimRegion {
    std::string id;
    int startMs = 0;
    int endMs = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TrimRegion, id, startMs, endMs)

struct FigureData {
    ArrowDirection arrowDirection = ArrowDirection::Right;
    std::string color = "#34B27B";
    int strokeWidth = 4;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FigureData, arrowDirection, color, strokeWidth)

struct AnnotationPosition {
    double x = 0.0;
    double y = 0.0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AnnotationPosition, x, y)

struct AnnotationSize {
    double width = 0.0;
    double height = 0.0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AnnotationSize, width, height)

struct AnnotationTextStyle {
    std::string color = "#ffffff";
    std::string backgroundColor = "transparent";
    int fontSize = 32;
    std::string fontFamily = "Inter";
    FontWeight fontWeight = FontWeight::Bold;
    FontStyle fontStyle = FontStyle::Normal;
    TextDecoration textDecoration = TextDecoration::None;
    TextAlign textAlign = TextAlign::Left;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AnnotationTextStyle,
    color, backgroundColor, fontSize, fontFamily,
    fontWeight, fontStyle, textDecoration, textAlign)

struct AnnotationRegion {
    std::string id;
    int startMs = 0;
    int endMs = 0;
    AnnotationType type = AnnotationType::Text;
    std::string content;
    std::optional<std::string> textContent;
    std::optional<std::string> imageContent;
    AnnotationPosition position;
    AnnotationSize size;
    AnnotationTextStyle style;
    int zIndex = 0;
    std::optional<FigureData> figureData;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AnnotationRegion,
    id, startMs, endMs, type, content,
    textContent, imageContent,
    position, size, style, zIndex, figureData)

struct CropRegion {
    double x = 0.0;
    double y = 0.0;
    double width = 1.0;
    double height = 1.0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CropRegion, x, y, width, height)

struct SpeedRegion {
    std::string id;
    int startMs = 0;
    int endMs = 0;
    PlaybackSpeed speed = PlaybackSpeed::X1_50;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SpeedRegion, id, startMs, endMs, speed)

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

inline const std::unordered_map<ZoomDepth, double> ZOOM_DEPTH_SCALES = {
    {ZoomDepth::D1, 1.25},
    {ZoomDepth::D2, 1.50},
    {ZoomDepth::D3, 1.80},
    {ZoomDepth::D4, 2.20},
    {ZoomDepth::D5, 3.50},
    {ZoomDepth::D6, 5.00},
};

inline constexpr ZoomDepth DEFAULT_ZOOM_DEPTH = ZoomDepth::D3;

inline const CropRegion DEFAULT_CROP_REGION{0.0, 0.0, 1.0, 1.0};

inline const AnnotationPosition DEFAULT_ANNOTATION_POSITION{50.0, 50.0};

inline const AnnotationSize DEFAULT_ANNOTATION_SIZE{30.0, 20.0};

inline const AnnotationTextStyle DEFAULT_ANNOTATION_STYLE{
    "#ffffff",          // color
    "transparent",      // backgroundColor
    32,                 // fontSize
    "Inter",            // fontFamily
    FontWeight::Bold,   // fontWeight
    FontStyle::Normal,  // fontStyle
    TextDecoration::None, // textDecoration
    TextAlign::Left     // textAlign
};

inline const FigureData DEFAULT_FIGURE_DATA{
    ArrowDirection::Right, // arrowDirection
    "#34B27B",             // color
    4                      // strokeWidth
};

inline constexpr PlaybackSpeed DEFAULT_PLAYBACK_SPEED = PlaybackSpeed::X1_50;

inline const std::array<PlaybackSpeed, 7> SPEED_OPTIONS = {
    PlaybackSpeed::X0_25,
    PlaybackSpeed::X0_50,
    PlaybackSpeed::X0_75,
    PlaybackSpeed::X1_25,
    PlaybackSpeed::X1_50,
    PlaybackSpeed::X1_75,
    PlaybackSpeed::X2_00,
};

inline constexpr WebcamLayoutPreset DEFAULT_WEBCAM_LAYOUT_PRESET =
    WebcamLayoutPreset::PictureInPicture;

inline const std::array<AspectRatio, 8> ASPECT_RATIOS = {
    AspectRatio::R16_9,
    AspectRatio::R9_16,
    AspectRatio::R1_1,
    AspectRatio::R4_3,
    AspectRatio::R4_5,
    AspectRatio::R16_10,
    AspectRatio::R10_16,
    AspectRatio::Native,
};

// ---------------------------------------------------------------------------
// Utility: clamp a ZoomFocus so the zoomed viewport stays within [0,1]
// ---------------------------------------------------------------------------

inline ZoomFocus clampFocusToDepth(const ZoomFocus& focus, ZoomDepth depth) {
    const double scale = zoomDepthScale(depth);
    const double halfViewport = 0.5 / scale;
    return ZoomFocus{
        clamp(focus.cx, halfViewport, 1.0 - halfViewport),
        clamp(focus.cy, halfViewport, 1.0 - halfViewport)
    };
}

} // namespace openscreen
