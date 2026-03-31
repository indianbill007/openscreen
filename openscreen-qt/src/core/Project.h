#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "types.h"

namespace openscreen {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

inline constexpr int PROJECT_VERSION = 2;
inline constexpr int WALLPAPER_COUNT = 18;

inline std::array<std::string, 18> makeWallpaperPaths() {
    std::array<std::string, 18> paths;
    for (int i = 0; i < 18; ++i) {
        paths[static_cast<size_t>(i)] =
            "/wallpapers/wallpaper" + std::to_string(i + 1) + ".jpg";
    }
    return paths;
}

inline const std::array<std::string, 18> WALLPAPER_PATHS = makeWallpaperPaths();

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

struct ProjectMedia {
    std::string screenVideoPath;
    std::optional<std::string> webcamVideoPath;
};

inline void to_json(nlohmann::json& j, const ProjectMedia& m) {
    j = nlohmann::json{{"screenVideoPath", m.screenVideoPath}};
    if (m.webcamVideoPath.has_value()) {
        j["webcamVideoPath"] = *m.webcamVideoPath;
    }
}

inline void from_json(const nlohmann::json& j, ProjectMedia& m) {
    j.at("screenVideoPath").get_to(m.screenVideoPath);
    if (j.contains("webcamVideoPath") && !j["webcamVideoPath"].is_null()) {
        m.webcamVideoPath = j["webcamVideoPath"].get<std::string>();
    } else {
        m.webcamVideoPath = std::nullopt;
    }
}

struct ExportSettings {
    std::string exportQuality = "good";
    std::string exportFormat = "mp4";
    int gifFrameRate = 15;
    bool gifLoop = true;
    std::string gifSizePreset = "medium";
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ExportSettings,
    exportQuality, exportFormat, gifFrameRate, gifLoop, gifSizePreset)

struct EditorState {
    std::string wallpaper = "/wallpapers/wallpaper1.jpg";
    double shadowIntensity = 0.0;
    bool showBlur = false;
    double motionBlurAmount = 0.0;
    double borderRadius = 0.0;
    double padding = 50.0;
    CropRegion cropRegion;
    std::vector<ZoomRegion> zoomRegions;
    std::vector<TrimRegion> trimRegions;
    std::vector<SpeedRegion> speedRegions;
    std::vector<AnnotationRegion> annotationRegions;
    AspectRatio aspectRatio = AspectRatio::R16_9;
    WebcamLayoutPreset webcamLayoutPreset = WebcamLayoutPreset::PictureInPicture;
    std::optional<WebcamPosition> webcamPosition;
};

inline void to_json(nlohmann::json& j, const EditorState& e) {
    j = nlohmann::json{
        {"wallpaper", e.wallpaper},
        {"shadowIntensity", e.shadowIntensity},
        {"showBlur", e.showBlur},
        {"motionBlurAmount", e.motionBlurAmount},
        {"borderRadius", e.borderRadius},
        {"padding", e.padding},
        {"cropRegion", e.cropRegion},
        {"zoomRegions", e.zoomRegions},
        {"trimRegions", e.trimRegions},
        {"speedRegions", e.speedRegions},
        {"annotationRegions", e.annotationRegions},
        {"aspectRatio", e.aspectRatio},
        {"webcamLayoutPreset", e.webcamLayoutPreset},
    };
    if (e.webcamPosition.has_value()) {
        j["webcamPosition"] = *e.webcamPosition;
    } else {
        j["webcamPosition"] = nullptr;
    }
}

inline void from_json(const nlohmann::json& j, EditorState& e) {
    EditorState defaults;
    e.wallpaper = j.value("wallpaper", defaults.wallpaper);
    e.shadowIntensity = j.value("shadowIntensity", defaults.shadowIntensity);
    e.showBlur = j.value("showBlur", defaults.showBlur);
    e.motionBlurAmount = j.value("motionBlurAmount", defaults.motionBlurAmount);
    e.borderRadius = j.value("borderRadius", defaults.borderRadius);
    e.padding = j.value("padding", defaults.padding);
    if (j.contains("cropRegion")) {
        e.cropRegion = j["cropRegion"].get<CropRegion>();
    }
    if (j.contains("zoomRegions")) {
        e.zoomRegions = j["zoomRegions"].get<std::vector<ZoomRegion>>();
    }
    if (j.contains("trimRegions")) {
        e.trimRegions = j["trimRegions"].get<std::vector<TrimRegion>>();
    }
    if (j.contains("speedRegions")) {
        e.speedRegions = j["speedRegions"].get<std::vector<SpeedRegion>>();
    }
    if (j.contains("annotationRegions")) {
        e.annotationRegions = j["annotationRegions"].get<std::vector<AnnotationRegion>>();
    }
    e.aspectRatio = j.value("aspectRatio", defaults.aspectRatio);
    e.webcamLayoutPreset = j.value("webcamLayoutPreset", defaults.webcamLayoutPreset);
    if (j.contains("webcamPosition") && !j["webcamPosition"].is_null()) {
        e.webcamPosition = j["webcamPosition"].get<WebcamPosition>();
    } else {
        e.webcamPosition = std::nullopt;
    }
}

struct EditorProjectData {
    int version = PROJECT_VERSION;
    std::optional<ProjectMedia> media;
    EditorState editor;
    ExportSettings exportSettings;
    std::optional<std::string> videoPath;  // legacy
};

inline void to_json(nlohmann::json& j, const EditorProjectData& d) {
    j = nlohmann::json{
        {"version", d.version},
        {"editor", d.editor},
        {"exportSettings", d.exportSettings},
    };
    if (d.media.has_value()) {
        j["media"] = *d.media;
    }
    if (d.videoPath.has_value()) {
        j["videoPath"] = *d.videoPath;
    }
}

inline void from_json(const nlohmann::json& j, EditorProjectData& d) {
    d.version = j.value("version", PROJECT_VERSION);
    if (j.contains("media") && !j["media"].is_null()) {
        d.media = j["media"].get<ProjectMedia>();
    } else {
        d.media = std::nullopt;
    }
    if (j.contains("editor")) {
        d.editor = j["editor"].get<EditorState>();
    }
    if (j.contains("exportSettings")) {
        d.exportSettings = j["exportSettings"].get<ExportSettings>();
    }
    if (j.contains("videoPath") && j["videoPath"].is_string()) {
        d.videoPath = j["videoPath"].get<std::string>();
    } else {
        d.videoPath = std::nullopt;
    }
}

// ---------------------------------------------------------------------------
// Functions
// ---------------------------------------------------------------------------

/// Convert an OS file path to a file:// URL.
std::string toFileUrl(const std::string& filePath);

/// Convert a file:// URL back to an OS file path.
std::string fromFileUrl(const std::string& fileUrl);

/// Find the next numeric ID for a given prefix among existing IDs.
/// E.g. prefix="zoom", ids={"zoom-1","zoom-3"} -> returns 4.
int deriveNextId(const std::string& prefix, const std::vector<std::string>& ids);

/// Validate that a JSON value looks like a valid project file.
bool validateProjectData(const nlohmann::json& candidate);

/// Normalize raw editor JSON into a fully-validated EditorState with defaults.
EditorState normalizeEditorState(const nlohmann::json& editorJson);

/// Load a project from a JSON file on disk.
EditorProjectData loadProject(const std::string& filePath);

/// Save a project to a JSON file on disk.
void saveProject(const std::string& filePath, const EditorProjectData& data);

} // namespace openscreen
