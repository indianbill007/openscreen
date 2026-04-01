#include "WallpaperRenderer.h"

#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace openscreen {

namespace {

constexpr int kBuiltinWallpaperCount = 18;

/// Clamp a value to [lo, hi]
double clamp01(double v)
{
    return std::clamp(v, 0.0, 1.0);
}

/// Split top-level comma-separated args respecting nested parentheses
std::vector<QString> splitGradientArgs(const QString& input)
{
    std::vector<QString> parts;
    QString current;
    int depth = 0;

    for (const QChar ch : input) {
        if (ch == '(') {
            ++depth;
            current += ch;
        } else if (ch == ')') {
            depth = std::max(0, depth - 1);
            current += ch;
        } else if (ch == ',' && depth == 0) {
            const auto trimmed = current.trimmed();
            if (!trimmed.isEmpty()) {
                parts.push_back(trimmed);
            }
            current.clear();
        } else {
            current += ch;
        }
    }

    const auto trimmed = current.trimmed();
    if (!trimmed.isEmpty()) {
        parts.push_back(trimmed);
    }
    return parts;
}

/// Check whether the first argument is a gradient descriptor (not a color stop)
bool isGradientDescriptor(const QString& type, const QString& part)
{
    if (type == "linear") {
        static const QRegularExpression toRe(R"(^\s*to\s+)", QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression degRe(R"(-?\d*\.?\d+deg)", QRegularExpression::CaseInsensitiveOption);
        return toRe.match(part).hasMatch() || degRe.match(part).hasMatch();
    }
    // radial
    static const QRegularExpression shapeRe(R"(\b(circle|ellipse|closest|farthest)\b)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression atRe(R"(\bat\b)", QRegularExpression::CaseInsensitiveOption);
    return shapeRe.match(part).hasMatch() || atRe.match(part).hasMatch();
}

/// Resolve a linear-gradient descriptor to an angle in degrees
double resolveLinearAngle(const QString& descriptor)
{
    if (descriptor.isEmpty()) {
        return 180.0;
    }

    static const QRegularExpression angleRe(R"((-?\d*\.?\d+)deg)", QRegularExpression::CaseInsensitiveOption);
    auto m = angleRe.match(descriptor);
    if (m.hasMatch()) {
        return m.captured(1).toDouble();
    }

    const QString normalized = descriptor.trimmed().toLower().simplified();

    struct DirEntry {
        const char* key;
        double angle;
    };
    static constexpr DirEntry kDirections[] = {
        {"to top",          0.0},
        {"to top right",   45.0},
        {"to right",       90.0},
        {"to bottom right",135.0},
        {"to bottom",      180.0},
        {"to bottom left", 225.0},
        {"to left",        270.0},
        {"to top left",    315.0},
    };

    for (const auto& entry : kDirections) {
        if (normalized == QLatin1String(entry.key)) {
            return entry.angle;
        }
    }
    return 180.0;
}

/// Parse a radial-gradient descriptor for center position (normalized 0-1)
void resolveRadialCenter(const QString& descriptor, double& cx, double& cy)
{
    cx = 0.5;
    cy = 0.5;

    if (descriptor.isEmpty()) {
        return;
    }

    static const QRegularExpression atRe(
        R"(at\s+(-?\d*\.?\d+)%\s+(-?\d*\.?\d+)%)",
        QRegularExpression::CaseInsensitiveOption
    );
    auto m = atRe.match(descriptor);
    if (m.hasMatch()) {
        cx = m.captured(1).toDouble() / 100.0;
        cy = m.captured(2).toDouble() / 100.0;
    }
}

struct RawStop {
    QString color;
    double offset = -1.0;  // negative means unset
    bool hasOffset = false;
};

/// Parse a single color stop token: "color [percentage%]"
std::optional<RawStop> parseColorStop(const QString& part)
{
    static const QRegularExpression colorRe(
        R"(^(#[0-9a-fA-F]{3,8}|(?:rgba?|hsla?)\([^)]*\)|[a-zA-Z-]+))"
    );
    auto m = colorRe.match(part.trimmed());
    if (!m.hasMatch()) {
        return std::nullopt;
    }

    RawStop stop;
    stop.color = m.captured(1);

    const QString rest = part.mid(m.capturedEnd(0));
    static const QRegularExpression pctRe(R"((-?\d*\.?\d+)%)");
    auto pm = pctRe.match(rest);
    if (pm.hasMatch()) {
        stop.offset = clamp01(pm.captured(1).toDouble() / 100.0);
        stop.hasOffset = true;
    }
    return stop;
}

/// Normalize stop offsets — fill in missing positions with even spacing
std::vector<GradientStop> normalizeStops(const std::vector<RawStop>& raw)
{
    const auto n = static_cast<int>(raw.size());
    if (n == 0) {
        return {};
    }

    // Check if any have explicit offsets
    const bool anyExplicit = std::any_of(raw.begin(), raw.end(),
        [](const RawStop& s) { return s.hasOffset; });

    // If no explicit offsets, distribute evenly
    if (!anyExplicit) {
        std::vector<GradientStop> result;
        result.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            GradientStop gs;
            gs.color = QColor(raw[static_cast<size_t>(i)].color);
            gs.offset = (n == 1) ? 0.0 : static_cast<double>(i) / (n - 1);
            result.push_back(gs);
        }
        return result;
    }

    // Resolve with interpolation between explicit stops
    std::vector<double> offsets(static_cast<size_t>(n), -1.0);
    for (int i = 0; i < n; ++i) {
        if (raw[static_cast<size_t>(i)].hasOffset) {
            offsets[static_cast<size_t>(i)] = raw[static_cast<size_t>(i)].offset;
        }
    }

    // Find first and last explicit
    int firstExplicit = -1;
    int lastExplicit = -1;
    for (int i = 0; i < n; ++i) {
        if (offsets[static_cast<size_t>(i)] >= 0.0) {
            if (firstExplicit < 0) {
                firstExplicit = i;
            }
            lastExplicit = i;
        }
    }

    // Fill before first explicit
    for (int i = 0; i < firstExplicit; ++i) {
        const double end = offsets[static_cast<size_t>(firstExplicit)];
        offsets[static_cast<size_t>(i)] = (firstExplicit == 0)
            ? end
            : (end * i) / firstExplicit;
    }

    // Fill after last explicit
    for (int i = lastExplicit + 1; i < n; ++i) {
        const double start = offsets[static_cast<size_t>(lastExplicit)];
        const int denom = n - 1 - lastExplicit;
        offsets[static_cast<size_t>(i)] = (denom <= 0)
            ? start
            : start + ((1.0 - start) * (i - lastExplicit)) / denom;
    }

    // Fill gaps between explicit stops
    int runStart = firstExplicit;
    while (runStart < lastExplicit) {
        int nextExplicit = -1;
        for (int i = runStart + 1; i <= lastExplicit; ++i) {
            if (offsets[static_cast<size_t>(i)] >= 0.0) {
                nextExplicit = i;
                break;
            }
        }
        if (nextExplicit < 0) {
            break;
        }

        const double startVal = offsets[static_cast<size_t>(runStart)];
        const double endVal = offsets[static_cast<size_t>(nextExplicit)];
        const int gap = nextExplicit - runStart;

        for (int i = runStart + 1; i < nextExplicit; ++i) {
            offsets[static_cast<size_t>(i)] = startVal + ((endVal - startVal) * (i - runStart)) / gap;
        }
        runStart = nextExplicit;
    }

    // Build result
    std::vector<GradientStop> result;
    result.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        GradientStop gs;
        gs.color = QColor(raw[static_cast<size_t>(i)].color);
        gs.offset = clamp01(std::max(0.0, offsets[static_cast<size_t>(i)]));
        result.push_back(gs);
    }
    return result;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// classify
// ---------------------------------------------------------------------------
WallpaperRenderer::WallpaperType WallpaperRenderer::classify(const QString& wallpaper)
{
    const auto trimmed = wallpaper.trimmed();

    if (trimmed.startsWith('#') || QColor::isValidColorName(trimmed)) {
        return WallpaperType::SolidColor;
    }

    if (trimmed.contains("gradient(")) {
        return WallpaperType::Gradient;
    }

    return WallpaperType::Image;
}

// ---------------------------------------------------------------------------
// loadImage
// ---------------------------------------------------------------------------
QImage WallpaperRenderer::loadImage(const QString& path, int targetWidth, int targetHeight)
{
    QImage img(path);
    if (img.isNull()) {
        return QImage();
    }

    // Scale with aspect ratio, then crop to center
    const QImage scaled = img.scaled(
        targetWidth, targetHeight,
        Qt::KeepAspectRatioByExpanding,
        Qt::SmoothTransformation
    );

    if (scaled.width() == targetWidth && scaled.height() == targetHeight) {
        return scaled;
    }

    const int cropX = (scaled.width() - targetWidth) / 2;
    const int cropY = (scaled.height() - targetHeight) / 2;
    return scaled.copy(cropX, cropY, targetWidth, targetHeight);
}

// ---------------------------------------------------------------------------
// parseColor
// ---------------------------------------------------------------------------
QColor WallpaperRenderer::parseColor(const QString& colorStr)
{
    return QColor(colorStr.trimmed());
}

// ---------------------------------------------------------------------------
// parseGradient
// ---------------------------------------------------------------------------
std::optional<ParsedGradient> WallpaperRenderer::parseGradient(const QString& gradientStr)
{
    static const QRegularExpression gradRe(
        R"(^(linear|radial)-gradient\((.*)\)$)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );

    auto match = gradRe.match(gradientStr.trimmed());
    if (!match.hasMatch()) {
        return std::nullopt;
    }

    const QString typeStr = match.captured(1).toLower();
    const auto args = splitGradientArgs(match.captured(2));
    if (args.empty()) {
        return std::nullopt;
    }

    QString descriptor;
    std::vector<QString> stopArgs;

    if (isGradientDescriptor(typeStr, args[0])) {
        descriptor = args[0];
        stopArgs.assign(args.begin() + 1, args.end());
    } else {
        stopArgs = args;
    }

    // Parse color stops
    std::vector<RawStop> rawStops;
    for (const auto& arg : stopArgs) {
        auto stop = parseColorStop(arg);
        if (stop.has_value()) {
            rawStops.push_back(std::move(*stop));
        }
    }

    if (rawStops.empty()) {
        return std::nullopt;
    }

    ParsedGradient result;

    if (typeStr == "radial") {
        result.type = ParsedGradient::Radial;
        resolveRadialCenter(descriptor, result.centerX, result.centerY);
    } else {
        result.type = ParsedGradient::Linear;
        result.angleDeg = resolveLinearAngle(descriptor);
    }

    result.stops = normalizeStops(rawStops);
    return result;
}

// ---------------------------------------------------------------------------
// renderGradient
// ---------------------------------------------------------------------------
QImage WallpaperRenderer::renderGradient(const ParsedGradient& gradient, int width, int height)
{
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::black);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    if (gradient.type == ParsedGradient::Linear) {
        // Compute start/end points from angle (same math as getLinearGradientPoints)
        const double radians = gradient.angleDeg * M_PI / 180.0;
        const double vx = std::sin(radians);
        const double vy = -std::cos(radians);
        const double halfSpan = (std::abs(vx) * width + std::abs(vy) * height) / 2.0;
        const double cx = width / 2.0;
        const double cy = height / 2.0;

        QLinearGradient lg(
            cx - vx * halfSpan, cy - vy * halfSpan,
            cx + vx * halfSpan, cy + vy * halfSpan
        );

        for (const auto& stop : gradient.stops) {
            lg.setColorAt(stop.offset, stop.color);
        }

        painter.fillRect(image.rect(), lg);
    } else {
        // Radial gradient — farthest-corner sizing
        const double cx = gradient.centerX * width;
        const double cy = gradient.centerY * height;

        const double d0 = std::hypot(cx, cy);
        const double d1 = std::hypot(width - cx, cy);
        const double d2 = std::hypot(cx, height - cy);
        const double d3 = std::hypot(width - cx, height - cy);
        const double radius = std::max({d0, d1, d2, d3});

        QRadialGradient rg(cx, cy, radius, cx, cy);

        for (const auto& stop : gradient.stops) {
            rg.setColorAt(stop.offset, stop.color);
        }

        painter.fillRect(image.rect(), rg);
    }

    painter.end();
    return image;
}

// ---------------------------------------------------------------------------
// renderSolidColor
// ---------------------------------------------------------------------------
QImage WallpaperRenderer::renderSolidColor(const QColor& color, int width, int height)
{
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(color);
    return image;
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------
QImage WallpaperRenderer::render(const QString& wallpaper, int width, int height)
{
    const auto type = classify(wallpaper);

    switch (type) {
    case WallpaperType::SolidColor:
        return renderSolidColor(parseColor(wallpaper), width, height);

    case WallpaperType::Gradient: {
        auto grad = parseGradient(wallpaper);
        if (grad.has_value()) {
            return renderGradient(*grad, width, height);
        }
        // Fallback to black if parsing fails
        return renderSolidColor(Qt::black, width, height);
    }

    case WallpaperType::Image:
        return loadImage(wallpaper, width, height);
    }

    return renderSolidColor(Qt::black, width, height);
}

// ---------------------------------------------------------------------------
// builtinWallpapers
// ---------------------------------------------------------------------------
std::vector<QString> WallpaperRenderer::builtinWallpapers()
{
    std::vector<QString> paths;
    paths.reserve(kBuiltinWallpaperCount);
    for (int i = 1; i <= kBuiltinWallpaperCount; ++i) {
        paths.push_back(QString(":/wallpapers/wallpaper%1.jpg").arg(i));
    }
    return paths;
}

} // namespace openscreen
