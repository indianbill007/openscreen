#pragma once
#include <QColor>
#include <QImage>
#include <QString>
#include <optional>
#include <string>
#include <vector>

namespace openscreen {

struct GradientStop {
    QColor color;
    double offset = 0.0;  // 0-1
};

struct ParsedGradient {
    enum Type { Linear, Radial };
    Type type = Linear;
    double angleDeg = 180.0;           // for linear
    double centerX = 0.5;             // for radial (normalized)
    double centerY = 0.5;             // for radial (normalized)
    std::vector<GradientStop> stops;
};

class WallpaperRenderer {
public:
    /// Parse a wallpaper string and classify it
    enum class WallpaperType { Image, SolidColor, Gradient };
    static WallpaperType classify(const QString& wallpaper);

    /// Load a wallpaper image from file path or Qt resource
    static QImage loadImage(const QString& path, int targetWidth, int targetHeight);

    /// Parse a hex color string
    static QColor parseColor(const QString& colorStr);

    /// Parse a CSS gradient string (linear-gradient or radial-gradient)
    static std::optional<ParsedGradient> parseGradient(const QString& gradientStr);

    /// Render a gradient to a QImage
    static QImage renderGradient(const ParsedGradient& gradient, int width, int height);

    /// Render a solid color to a QImage
    static QImage renderSolidColor(const QColor& color, int width, int height);

    /// High-level: render any wallpaper string to a QImage
    static QImage render(const QString& wallpaper, int width, int height);

    /// List of built-in wallpaper resource paths
    static std::vector<QString> builtinWallpapers();
};

} // namespace openscreen
