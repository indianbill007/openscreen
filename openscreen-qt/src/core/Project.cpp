#include "Project.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace openscreen {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

bool isFiniteDouble(double v) {
    return std::isfinite(v);
}

bool isFileUrl(const std::string& value) {
    if (value.size() < 7) return false;
    std::string prefix = value.substr(0, 7);
    for (auto& c : prefix) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return prefix == "file://";
}

/// Percent-encode a single path segment (RFC 3986 unreserved chars pass through).
std::string encodeSegment(const std::string& segment) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;
    for (unsigned char c : segment) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            encoded << '%' << std::uppercase;
            encoded.width(2);
            encoded << static_cast<int>(c);
        }
    }
    return encoded.str();
}

/// Percent-encode path segments separated by '/'.
/// If keepWindowsDrive is true, a segment like "C:" at index 1 is kept verbatim.
std::string encodePathSegments(const std::string& pathname, bool keepWindowsDrive = false) {
    std::vector<std::string> segments;
    std::istringstream stream(pathname);
    std::string seg;
    while (std::getline(stream, seg, '/')) {
        segments.push_back(seg);
    }

    std::ostringstream result;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) result << '/';
        const auto& s = segments[i];
        if (s.empty()) {
            continue;
        }
        if (keepWindowsDrive && i == 1 && s.size() == 2 &&
            std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':') {
            result << s;
        } else {
            result << encodeSegment(s);
        }
    }
    return result.str();
}

/// Decode percent-encoded characters in a string.
std::string percentDecode(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size() &&
            std::isxdigit(static_cast<unsigned char>(input[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(input[i + 2]))) {
            auto hexVal = static_cast<char>(
                std::stoi(input.substr(i + 1, 2), nullptr, 16));
            result += hexVal;
            i += 2;
        } else {
            result += input[i];
        }
    }
    return result;
}

/// Check if a string matches a valid PlaybackSpeed JSON value.
bool isValidPlaybackSpeed(double speed) {
    return speed == 0.25 || speed == 0.5 || speed == 0.75 ||
           speed == 1.25 || speed == 1.5 || speed == 1.75 || speed == 2.0;
}

/// Check if a value is one of the valid ZoomDepth integers.
bool isValidZoomDepth(int depth) {
    return depth >= 1 && depth <= 6;
}

/// Check if aspect ratio string is valid.
bool isValidAspectRatio(const std::string& ar) {
    static const std::unordered_set<std::string> valid = {
        "16:9", "9:16", "1:1", "4:3", "4:5", "16:10", "10:16", "native"
    };
    return valid.count(ar) > 0;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// toFileUrl
// ---------------------------------------------------------------------------

std::string toFileUrl(const std::string& filePath) {
    // Normalize backslashes to forward slashes.
    std::string normalized = filePath;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    // Windows drive path: C:/Users/...
    if (normalized.size() >= 3 &&
        std::isalpha(static_cast<unsigned char>(normalized[0])) &&
        normalized[1] == ':' && normalized[2] == '/') {
        return "file://" + encodePathSegments("/" + normalized, true);
    }

    // UNC path: //server/share/...
    if (normalized.size() >= 2 && normalized[0] == '/' && normalized[1] == '/') {
        // Strip leading slashes, split host from path.
        std::string stripped = normalized.substr(2);
        auto slashPos = stripped.find('/');
        if (slashPos == std::string::npos) {
            return "file://" + stripped + "/";
        }
        std::string host = stripped.substr(0, slashPos);
        std::string rest = stripped.substr(slashPos + 1);
        // Encode path parts individually.
        std::vector<std::string> parts;
        std::istringstream stream(rest);
        std::string part;
        while (std::getline(stream, part, '/')) {
            if (!part.empty()) {
                parts.push_back(encodeSegment(part));
            }
        }
        std::string encodedPath;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) encodedPath += '/';
            encodedPath += parts[i];
        }
        return encodedPath.empty()
            ? "file://" + host + "/"
            : "file://" + host + "/" + encodedPath;
    }

    // Unix absolute or relative path.
    std::string absolutePath = (normalized[0] == '/') ? normalized : "/" + normalized;
    return "file://" + encodePathSegments(absolutePath);
}

// ---------------------------------------------------------------------------
// fromFileUrl
// ---------------------------------------------------------------------------

std::string fromFileUrl(const std::string& fileUrl) {
    std::string value = fileUrl;
    // Trim whitespace.
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }

    if (!isFileUrl(value)) {
        return fileUrl;
    }

    // Strip "file://" prefix.
    std::string afterPrefix = value.substr(7);

    // Check for host (UNC path): file://server/share -> //server/share
    // If afterPrefix starts with a letter (not '/'), it's a host.
    if (!afterPrefix.empty() && afterPrefix[0] != '/' &&
        afterPrefix.find('/') != std::string::npos) {
        // Has host component.
        auto slashPos = afterPrefix.find('/');
        std::string host = afterPrefix.substr(0, slashPos);
        if (host != "localhost") {
            std::string pathname = percentDecode(afterPrefix.substr(slashPos));
            return "//" + host + pathname;
        }
        // localhost — treat as local path.
        afterPrefix = afterPrefix.substr(slashPos);
    }

    std::string pathname = percentDecode(afterPrefix);

    // Windows drive letter: /C:/... -> C:/...
    if (pathname.size() >= 3 && pathname[0] == '/' &&
        std::isalpha(static_cast<unsigned char>(pathname[1])) &&
        pathname[2] == ':') {
        return pathname.substr(1);
    }

    return pathname;
}

// ---------------------------------------------------------------------------
// deriveNextId
// ---------------------------------------------------------------------------

int deriveNextId(const std::string& prefix, const std::vector<std::string>& ids) {
    int maxVal = 0;
    std::regex pattern("^" + prefix + "-(\\d+)$");
    for (const auto& id : ids) {
        std::smatch match;
        if (std::regex_match(id, match, pattern)) {
            int val = std::stoi(match[1].str());
            if (val > maxVal) {
                maxVal = val;
            }
        }
    }
    return maxVal + 1;
}

// ---------------------------------------------------------------------------
// validateProjectData
// ---------------------------------------------------------------------------

bool validateProjectData(const nlohmann::json& candidate) {
    if (!candidate.is_object()) return false;
    if (!candidate.contains("version") || !candidate["version"].is_number()) return false;
    if (!candidate.contains("editor") || !candidate["editor"].is_object()) return false;
    return true;
}

// ---------------------------------------------------------------------------
// normalizeEditorState
// ---------------------------------------------------------------------------

EditorState normalizeEditorState(const nlohmann::json& editorJson) {
    EditorState result;

    // wallpaper
    if (editorJson.contains("wallpaper") && editorJson["wallpaper"].is_string()) {
        result.wallpaper = editorJson["wallpaper"].get<std::string>();
    } else {
        result.wallpaper = WALLPAPER_PATHS[0];
    }

    // shadowIntensity
    if (editorJson.contains("shadowIntensity") && editorJson["shadowIntensity"].is_number()) {
        result.shadowIntensity = editorJson["shadowIntensity"].get<double>();
    } else {
        result.shadowIntensity = 0.0;
    }

    // showBlur
    if (editorJson.contains("showBlur") && editorJson["showBlur"].is_boolean()) {
        result.showBlur = editorJson["showBlur"].get<bool>();
    } else {
        result.showBlur = false;
    }

    // motionBlurAmount — with legacy motionBlurEnabled fallback
    if (editorJson.contains("motionBlurAmount") && editorJson["motionBlurAmount"].is_number()) {
        double raw = editorJson["motionBlurAmount"].get<double>();
        result.motionBlurAmount = isFiniteDouble(raw) ? clamp(raw, 0.0, 1.0) : 0.0;
    } else if (editorJson.contains("motionBlurEnabled") &&
               editorJson["motionBlurEnabled"].is_boolean()) {
        result.motionBlurAmount = editorJson["motionBlurEnabled"].get<bool>() ? 0.35 : 0.0;
    } else {
        result.motionBlurAmount = 0.0;
    }

    // borderRadius
    if (editorJson.contains("borderRadius") && editorJson["borderRadius"].is_number()) {
        result.borderRadius = editorJson["borderRadius"].get<double>();
    } else {
        result.borderRadius = 0.0;
    }

    // padding
    if (editorJson.contains("padding") && editorJson["padding"].is_number()) {
        double raw = editorJson["padding"].get<double>();
        result.padding = isFiniteDouble(raw) ? clamp(raw, 0.0, 100.0) : 50.0;
    } else {
        result.padding = 50.0;
    }

    // cropRegion
    {
        double rawX = DEFAULT_CROP_REGION.x;
        double rawY = DEFAULT_CROP_REGION.y;
        double rawW = DEFAULT_CROP_REGION.width;
        double rawH = DEFAULT_CROP_REGION.height;

        if (editorJson.contains("cropRegion") && editorJson["cropRegion"].is_object()) {
            const auto& cr = editorJson["cropRegion"];
            if (cr.contains("x") && cr["x"].is_number()) {
                double v = cr["x"].get<double>();
                if (isFiniteDouble(v)) rawX = v;
            }
            if (cr.contains("y") && cr["y"].is_number()) {
                double v = cr["y"].get<double>();
                if (isFiniteDouble(v)) rawY = v;
            }
            if (cr.contains("width") && cr["width"].is_number()) {
                double v = cr["width"].get<double>();
                if (isFiniteDouble(v)) rawW = v;
            }
            if (cr.contains("height") && cr["height"].is_number()) {
                double v = cr["height"].get<double>();
                if (isFiniteDouble(v)) rawH = v;
            }
        }

        double cx = clamp(rawX, 0.0, 1.0);
        double cy = clamp(rawY, 0.0, 1.0);
        double cw = clamp(rawW, 0.01, 1.0 - cx);
        double ch = clamp(rawH, 0.01, 1.0 - cy);
        result.cropRegion = CropRegion{cx, cy, cw, ch};
    }

    // zoomRegions
    if (editorJson.contains("zoomRegions") && editorJson["zoomRegions"].is_array()) {
        for (const auto& rj : editorJson["zoomRegions"]) {
            if (!rj.is_object() || !rj.contains("id") || !rj["id"].is_string()) {
                continue;
            }

            int rawStart = 0;
            int rawEnd = 0;
            if (rj.contains("startMs") && rj["startMs"].is_number()) {
                rawStart = static_cast<int>(std::round(rj["startMs"].get<double>()));
            }
            if (rj.contains("endMs") && rj["endMs"].is_number()) {
                rawEnd = static_cast<int>(std::round(rj["endMs"].get<double>()));
            } else {
                rawEnd = rawStart + 1000;
            }

            int startMs = std::max(0, std::min(rawStart, rawEnd));
            int endMs = std::max(startMs + 1, rawEnd);

            int depthVal = 3;
            if (rj.contains("depth") && rj["depth"].is_number_integer()) {
                depthVal = rj["depth"].get<int>();
            }
            ZoomDepth depth = isValidZoomDepth(depthVal)
                ? static_cast<ZoomDepth>(depthVal)
                : DEFAULT_ZOOM_DEPTH;

            double fcx = 0.5;
            double fcy = 0.5;
            if (rj.contains("focus") && rj["focus"].is_object()) {
                const auto& fj = rj["focus"];
                if (fj.contains("cx") && fj["cx"].is_number()) {
                    double v = fj["cx"].get<double>();
                    if (isFiniteDouble(v)) fcx = v;
                }
                if (fj.contains("cy") && fj["cy"].is_number()) {
                    double v = fj["cy"].get<double>();
                    if (isFiniteDouble(v)) fcy = v;
                }
            }

            ZoomRegion zr;
            zr.id = rj["id"].get<std::string>();
            zr.startMs = startMs;
            zr.endMs = endMs;
            zr.depth = depth;
            zr.focus = ZoomFocus{clamp(fcx, 0.0, 1.0), clamp(fcy, 0.0, 1.0)};
            result.zoomRegions.push_back(zr);
        }
    }

    // trimRegions
    if (editorJson.contains("trimRegions") && editorJson["trimRegions"].is_array()) {
        for (const auto& rj : editorJson["trimRegions"]) {
            if (!rj.is_object() || !rj.contains("id") || !rj["id"].is_string()) {
                continue;
            }

            int rawStart = 0;
            int rawEnd = 0;
            if (rj.contains("startMs") && rj["startMs"].is_number()) {
                rawStart = static_cast<int>(std::round(rj["startMs"].get<double>()));
            }
            if (rj.contains("endMs") && rj["endMs"].is_number()) {
                rawEnd = static_cast<int>(std::round(rj["endMs"].get<double>()));
            } else {
                rawEnd = rawStart + 1000;
            }

            int startMs = std::max(0, std::min(rawStart, rawEnd));
            int endMs = std::max(startMs + 1, rawEnd);

            TrimRegion tr;
            tr.id = rj["id"].get<std::string>();
            tr.startMs = startMs;
            tr.endMs = endMs;
            result.trimRegions.push_back(tr);
        }
    }

    // speedRegions
    if (editorJson.contains("speedRegions") && editorJson["speedRegions"].is_array()) {
        for (const auto& rj : editorJson["speedRegions"]) {
            if (!rj.is_object() || !rj.contains("id") || !rj["id"].is_string()) {
                continue;
            }

            int rawStart = 0;
            int rawEnd = 0;
            if (rj.contains("startMs") && rj["startMs"].is_number()) {
                rawStart = static_cast<int>(std::round(rj["startMs"].get<double>()));
            }
            if (rj.contains("endMs") && rj["endMs"].is_number()) {
                rawEnd = static_cast<int>(std::round(rj["endMs"].get<double>()));
            } else {
                rawEnd = rawStart + 1000;
            }

            int startMs = std::max(0, std::min(rawStart, rawEnd));
            int endMs = std::max(startMs + 1, rawEnd);

            PlaybackSpeed speed = DEFAULT_PLAYBACK_SPEED;
            if (rj.contains("speed") && rj["speed"].is_number()) {
                double sv = rj["speed"].get<double>();
                if (isValidPlaybackSpeed(sv)) {
                    // Convert double to PlaybackSpeed enum via JSON round-trip.
                    nlohmann::json sj = sv;
                    speed = sj.get<PlaybackSpeed>();
                }
            }

            SpeedRegion sr;
            sr.id = rj["id"].get<std::string>();
            sr.startMs = startMs;
            sr.endMs = endMs;
            sr.speed = speed;
            result.speedRegions.push_back(sr);
        }
    }

    // annotationRegions
    if (editorJson.contains("annotationRegions") && editorJson["annotationRegions"].is_array()) {
        int index = 0;
        for (const auto& rj : editorJson["annotationRegions"]) {
            if (!rj.is_object() || !rj.contains("id") || !rj["id"].is_string()) {
                ++index;
                continue;
            }

            int rawStart = 0;
            int rawEnd = 0;
            if (rj.contains("startMs") && rj["startMs"].is_number()) {
                rawStart = static_cast<int>(std::round(rj["startMs"].get<double>()));
            }
            if (rj.contains("endMs") && rj["endMs"].is_number()) {
                rawEnd = static_cast<int>(std::round(rj["endMs"].get<double>()));
            } else {
                rawEnd = rawStart + 1000;
            }

            int startMs = std::max(0, std::min(rawStart, rawEnd));
            int endMs = std::max(startMs + 1, rawEnd);

            // type
            AnnotationType atype = AnnotationType::Text;
            if (rj.contains("type") && rj["type"].is_string()) {
                std::string ts = rj["type"].get<std::string>();
                if (ts == "image") atype = AnnotationType::Image;
                else if (ts == "figure") atype = AnnotationType::Figure;
            }

            // content
            std::string content;
            if (rj.contains("content") && rj["content"].is_string()) {
                content = rj["content"].get<std::string>();
            }

            // textContent, imageContent
            std::optional<std::string> textContent;
            std::optional<std::string> imageContent;
            if (rj.contains("textContent") && rj["textContent"].is_string()) {
                textContent = rj["textContent"].get<std::string>();
            }
            if (rj.contains("imageContent") && rj["imageContent"].is_string()) {
                imageContent = rj["imageContent"].get<std::string>();
            }

            // position
            double px = DEFAULT_ANNOTATION_POSITION.x;
            double py = DEFAULT_ANNOTATION_POSITION.y;
            if (rj.contains("position") && rj["position"].is_object()) {
                const auto& pj = rj["position"];
                if (pj.contains("x") && pj["x"].is_number()) {
                    double v = pj["x"].get<double>();
                    if (isFiniteDouble(v)) px = v;
                }
                if (pj.contains("y") && pj["y"].is_number()) {
                    double v = pj["y"].get<double>();
                    if (isFiniteDouble(v)) py = v;
                }
            }

            // size
            double sw = DEFAULT_ANNOTATION_SIZE.width;
            double sh = DEFAULT_ANNOTATION_SIZE.height;
            if (rj.contains("size") && rj["size"].is_object()) {
                const auto& sj = rj["size"];
                if (sj.contains("width") && sj["width"].is_number()) {
                    double v = sj["width"].get<double>();
                    if (isFiniteDouble(v)) sw = v;
                }
                if (sj.contains("height") && sj["height"].is_number()) {
                    double v = sj["height"].get<double>();
                    if (isFiniteDouble(v)) sh = v;
                }
            }

            // style: merge defaults with whatever is provided
            AnnotationTextStyle style = DEFAULT_ANNOTATION_STYLE;
            if (rj.contains("style") && rj["style"].is_object()) {
                const auto& stj = rj["style"];
                if (stj.contains("color") && stj["color"].is_string())
                    style.color = stj["color"].get<std::string>();
                if (stj.contains("backgroundColor") && stj["backgroundColor"].is_string())
                    style.backgroundColor = stj["backgroundColor"].get<std::string>();
                if (stj.contains("fontSize") && stj["fontSize"].is_number_integer())
                    style.fontSize = stj["fontSize"].get<int>();
                if (stj.contains("fontFamily") && stj["fontFamily"].is_string())
                    style.fontFamily = stj["fontFamily"].get<std::string>();
                if (stj.contains("fontWeight"))
                    style.fontWeight = stj["fontWeight"].get<FontWeight>();
                if (stj.contains("fontStyle"))
                    style.fontStyle = stj["fontStyle"].get<FontStyle>();
                if (stj.contains("textDecoration"))
                    style.textDecoration = stj["textDecoration"].get<TextDecoration>();
                if (stj.contains("textAlign"))
                    style.textAlign = stj["textAlign"].get<TextAlign>();
            }

            // zIndex
            int zIndex = index + 1;
            if (rj.contains("zIndex") && rj["zIndex"].is_number()) {
                double v = rj["zIndex"].get<double>();
                if (isFiniteDouble(v)) zIndex = static_cast<int>(v);
            }

            // figureData
            std::optional<FigureData> figureData;
            if (rj.contains("figureData") && rj["figureData"].is_object()) {
                FigureData fd = DEFAULT_FIGURE_DATA;
                const auto& fdj = rj["figureData"];
                if (fdj.contains("arrowDirection"))
                    fd.arrowDirection = fdj["arrowDirection"].get<ArrowDirection>();
                if (fdj.contains("color") && fdj["color"].is_string())
                    fd.color = fdj["color"].get<std::string>();
                if (fdj.contains("strokeWidth") && fdj["strokeWidth"].is_number_integer())
                    fd.strokeWidth = fdj["strokeWidth"].get<int>();
                figureData = fd;
            }

            AnnotationRegion ar;
            ar.id = rj["id"].get<std::string>();
            ar.startMs = startMs;
            ar.endMs = endMs;
            ar.type = atype;
            ar.content = content;
            ar.textContent = textContent;
            ar.imageContent = imageContent;
            ar.position = AnnotationPosition{clamp(px, 0.0, 100.0), clamp(py, 0.0, 100.0)};
            ar.size = AnnotationSize{clamp(sw, 1.0, 200.0), clamp(sh, 1.0, 200.0)};
            ar.style = style;
            ar.zIndex = zIndex;
            ar.figureData = figureData;
            result.annotationRegions.push_back(ar);

            ++index;
        }
    }

    // aspectRatio
    if (editorJson.contains("aspectRatio") && editorJson["aspectRatio"].is_string()) {
        std::string ar = editorJson["aspectRatio"].get<std::string>();
        if (isValidAspectRatio(ar)) {
            nlohmann::json arj = ar;
            result.aspectRatio = arj.get<AspectRatio>();
        } else {
            result.aspectRatio = AspectRatio::R16_9;
        }
    } else {
        result.aspectRatio = AspectRatio::R16_9;
    }

    // webcamLayoutPreset
    if (editorJson.contains("webcamLayoutPreset") && editorJson["webcamLayoutPreset"].is_string()) {
        std::string wlp = editorJson["webcamLayoutPreset"].get<std::string>();
        if (wlp == "vertical-stack") {
            result.webcamLayoutPreset = WebcamLayoutPreset::VerticalStack;
        } else {
            result.webcamLayoutPreset = WebcamLayoutPreset::PictureInPicture;
        }
    } else {
        result.webcamLayoutPreset = DEFAULT_WEBCAM_LAYOUT_PRESET;
    }

    // webcamPosition
    if (editorJson.contains("webcamPosition") && editorJson["webcamPosition"].is_object()) {
        const auto& wpj = editorJson["webcamPosition"];
        if (wpj.contains("cx") && wpj["cx"].is_number() &&
            wpj.contains("cy") && wpj["cy"].is_number()) {
            double wcx = wpj["cx"].get<double>();
            double wcy = wpj["cy"].get<double>();
            if (isFiniteDouble(wcx) && isFiniteDouble(wcy)) {
                result.webcamPosition = WebcamPosition{
                    clamp(wcx, 0.0, 1.0),
                    clamp(wcy, 0.0, 1.0)
                };
            } else {
                result.webcamPosition = WebcamPosition{0.0, 0.0};
            }
        } else {
            result.webcamPosition = WebcamPosition{0.0, 0.0};
        }
    } else {
        result.webcamPosition = WebcamPosition{0.0, 0.0};
    }

    return result;
}

// ---------------------------------------------------------------------------
// loadProject
// ---------------------------------------------------------------------------

EditorProjectData loadProject(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open project file: " + filePath);
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("Failed to parse project JSON: " + std::string(e.what()));
    }

    if (!validateProjectData(j)) {
        throw std::runtime_error("Invalid project data in file: " + filePath);
    }

    EditorProjectData data;
    data.version = j.value("version", PROJECT_VERSION);

    // Resolve media: prefer "media" field, fall back to legacy "videoPath".
    if (j.contains("media") && j["media"].is_object()) {
        data.media = j["media"].get<ProjectMedia>();
    } else if (j.contains("videoPath") && j["videoPath"].is_string()) {
        std::string vp = j["videoPath"].get<std::string>();
        if (!vp.empty()) {
            data.media = ProjectMedia{vp, std::nullopt};
            data.videoPath = vp;
        }
    }

    // Normalize editor state with validation and defaults.
    data.editor = normalizeEditorState(j["editor"]);

    // Export settings: pull from editor-level fields (as in TS) or top-level exportSettings.
    if (j.contains("exportSettings") && j["exportSettings"].is_object()) {
        data.exportSettings = j["exportSettings"].get<ExportSettings>();
    } else {
        // The TS version stores export settings inside editor, so read them from there.
        ExportSettings es;
        const auto& ej = j["editor"];
        if (ej.contains("exportQuality") && ej["exportQuality"].is_string()) {
            std::string eq = ej["exportQuality"].get<std::string>();
            es.exportQuality = (eq == "medium" || eq == "source") ? eq : "good";
        }
        if (ej.contains("exportFormat") && ej["exportFormat"].is_string()) {
            es.exportFormat = (ej["exportFormat"].get<std::string>() == "gif") ? "gif" : "mp4";
        }
        if (ej.contains("gifFrameRate") && ej["gifFrameRate"].is_number_integer()) {
            int gfr = ej["gifFrameRate"].get<int>();
            es.gifFrameRate = (gfr == 15 || gfr == 20 || gfr == 25 || gfr == 30) ? gfr : 15;
        }
        if (ej.contains("gifLoop") && ej["gifLoop"].is_boolean()) {
            es.gifLoop = ej["gifLoop"].get<bool>();
        }
        if (ej.contains("gifSizePreset") && ej["gifSizePreset"].is_string()) {
            std::string gsp = ej["gifSizePreset"].get<std::string>();
            es.gifSizePreset = (gsp == "medium" || gsp == "large" || gsp == "original")
                ? gsp : "medium";
        }
        data.exportSettings = es;
    }

    return data;
}

// ---------------------------------------------------------------------------
// saveProject
// ---------------------------------------------------------------------------

void saveProject(const std::string& filePath, const EditorProjectData& data) {
    nlohmann::json j = data;

    std::ofstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filePath);
    }
    file << j.dump(2);
    if (!file.good()) {
        throw std::runtime_error("Failed to write project file: " + filePath);
    }
}

} // namespace openscreen
