# OpenScreen C++/Qt Rewrite — Implementation Plan

## Overview

Rewrite OpenScreen (currently Electron + React + PixiJS) as a native C++17 desktop application using Qt 6 (Widgets + QML hybrid). This gives us native performance, smaller binary size (~30MB vs ~200MB+), lower memory footprint, and direct access to OS-level screen capture and GPU APIs.

---

## Architecture

```
openscreen-qt/
├── CMakeLists.txt                  # Top-level CMake
├── src/
│   ├── main.cpp                    # Entry point
│   ├── app/
│   │   ├── Application.h/cpp       # QApplication subclass, global state
│   │   └── Settings.h/cpp          # QSettings wrapper (shortcuts, locale, prefs)
│   │
│   ├── core/                       # Pure C++ logic (no Qt GUI deps)
│   │   ├── types.h                 # ZoomRegion, TrimRegion, SpeedRegion, AnnotationRegion, etc.
│   │   ├── Project.h/cpp           # Project file serialization (.openscreen JSON)
│   │   ├── EditorState.h/cpp       # Immutable editor state + undo/redo history
│   │   ├── TimelineModel.h/cpp     # Timeline region management (add/remove/resize)
│   │   ├── ZoomTransform.h/cpp     # Zoom math, focus clamping, motion blur velocity
│   │   ├── CompositeLayout.h/cpp   # Webcam layout calculations (PiP, vertical stack)
│   │   ├── AspectRatio.h/cpp       # Aspect ratio presets and calculations
│   │   └── CursorTelemetry.h/cpp   # Cursor position sampling + auto-zoom detection
│   │
│   ├── capture/                    # Platform-specific screen/audio capture
│   │   ├── ScreenCapture.h         # Abstract interface
│   │   ├── ScreenCaptureWin.h/cpp  # Windows: DXGI Desktop Duplication API
│   │   ├── ScreenCaptureMac.h/cpp  # macOS: ScreenCaptureKit (macOS 12.3+)
│   │   ├── ScreenCaptureLinux.h/cpp# Linux: PipeWire / XDG Desktop Portal
│   │   ├── AudioCapture.h          # Abstract interface
│   │   ├── AudioCaptureWin.h/cpp   # Windows: WASAPI loopback
│   │   ├── AudioCaptureMac.h/cpp   # macOS: ScreenCaptureKit audio
│   │   ├── AudioCaptureLinux.h/cpp # Linux: PipeWire audio
│   │   ├── MicCapture.h/cpp        # Cross-platform mic via QAudioSource
│   │   ├── AudioMixer.h/cpp        # Mix mic + system audio streams
│   │   └── WebcamCapture.h/cpp     # QCamera + QMediaCaptureSession
│   │
│   ├── render/                     # OpenGL-based rendering engine
│   │   ├── RenderEngine.h/cpp      # QOpenGLWidget-based renderer
│   │   ├── Shaders.h/cpp           # GLSL shaders (zoom, blur, motion blur, shadow)
│   │   ├── VideoTexture.h/cpp      # Decode frame → OpenGL texture upload
│   │   ├── WallpaperRenderer.h/cpp # Background rendering (images, gradients, solid)
│   │   ├── AnnotationRenderer.h/cpp# Text/arrow/image overlay rendering (QPainter)
│   │   ├── WebcamRenderer.h/cpp    # Webcam compositing with shadow/border
│   │   └── CropMask.h/cpp          # Crop region masking
│   │
│   ├── export/                     # Video export pipeline
│   │   ├── Exporter.h/cpp          # Main export orchestrator
│   │   ├── VideoDecoder.h/cpp      # FFmpeg-based frame decoder (sequential)
│   │   ├── VideoEncoder.h/cpp      # FFmpeg H.264/H.265 encoder (HW accel)
│   │   ├── AudioProcessor.h/cpp    # Audio decode, trim, speed (pitch-preserved)
│   │   ├── Muxer.h/cpp             # FFmpeg MP4/MKV muxer
│   │   ├── GifExporter.h/cpp       # GIF export via FFmpeg palettegen + paletteuse
│   │   ├── FrameRenderer.h/cpp     # Offscreen OpenGL rendering per export frame
│   │   └── ExportConfig.h          # Quality, resolution, format settings
│   │
│   ├── ui/                         # Qt UI layer
│   │   ├── windows/
│   │   │   ├── MainWindow.h/cpp    # Editor window (QMainWindow)
│   │   │   ├── HudOverlay.h/cpp    # Recording HUD (frameless, always-on-top)
│   │   │   └── SourceSelector.h/cpp# Screen/window picker dialog
│   │   │
│   │   ├── editor/
│   │   │   ├── VideoPreview.h/cpp  # OpenGL preview widget (QOpenGLWidget)
│   │   │   ├── PlaybackControls.h/cpp # Play/pause, scrubber, time display
│   │   │   ├── SettingsPanel.h/cpp # Effect settings (accordion layout)
│   │   │   ├── AnnotationOverlay.h/cpp # Draggable/resizable annotations
│   │   │   ├── ExportDialog.h/cpp  # Export settings + progress dialog
│   │   │   └── CropControl.h/cpp   # Crop region selector overlay
│   │   │
│   │   ├── timeline/
│   │   │   ├── TimelineWidget.h/cpp    # Custom QWidget timeline
│   │   │   ├── TimelineRow.h/cpp       # Row per effect type
│   │   │   ├── TimelineItem.h/cpp      # Draggable/resizable region item
│   │   │   └── TimelineRuler.h/cpp     # Time ruler with tick marks
│   │   │
│   │   ├── widgets/                # Reusable UI components
│   │   │   ├── ColorPicker.h/cpp
│   │   │   ├── WallpaperPicker.h/cpp
│   │   │   ├── AspectRatioSelector.h/cpp
│   │   │   ├── AudioLevelMeter.h/cpp
│   │   │   ├── ShortcutEditor.h/cpp
│   │   │   └── AccordionWidget.h/cpp
│   │   │
│   │   └── styles/
│   │       └── Theme.h/cpp         # Dark theme, colors, fonts (QSS stylesheet)
│   │
│   └── i18n/                       # Internationalization
│       ├── TranslationManager.h/cpp # Qt .ts/.qm file management
│       └── translations/
│           ├── openscreen_en.ts
│           ├── openscreen_es.ts
│           └── openscreen_zh_CN.ts
│
├── resources/
│   ├── wallpapers/                 # 18 built-in backgrounds
│   ├── icons/                      # App + UI icons
│   ├── shaders/                    # GLSL shader files
│   │   ├── zoom.vert / zoom.frag
│   │   ├── motion_blur.frag
│   │   ├── gaussian_blur.frag
│   │   └── drop_shadow.frag
│   └── openscreen.qrc             # Qt resource file
│
├── third_party/
│   └── CMakeLists.txt              # FFmpeg, etc. dependency management
│
└── tests/
    ├── test_zoom_transform.cpp
    ├── test_timeline_model.cpp
    ├── test_editor_state.cpp
    ├── test_aspect_ratio.cpp
    ├── test_composite_layout.cpp
    ├── test_audio_mixer.cpp
    └── test_export_pipeline.cpp
```

---

## Dependencies

| Library | Purpose | Replaces (Electron) |
|---------|---------|---------------------|
| **Qt 6.6+** (Widgets, OpenGL, Multimedia) | UI framework, audio/camera, OpenGL context | React, Radix UI, Tailwind |
| **FFmpeg 6.x** (libavcodec, libavformat, libavutil, libswscale, libswresample) | Video decode/encode, muxing, audio processing | WebCodecs, mediabunny, web-demuxer, gif.js |
| **OpenGL 3.3+** | GPU-accelerated rendering & effects | PixiJS, pixi-filters |
| **nlohmann/json** | Project file serialization | Built-in JSON |
| **spdlog** | Logging | console.log |
| **Google Test** | Unit testing | Vitest |

### Platform-Specific APIs (no extra libs needed)

| Platform | Screen Capture | System Audio |
|----------|---------------|--------------|
| Windows | DXGI Desktop Duplication | WASAPI Loopback |
| macOS | ScreenCaptureKit | ScreenCaptureKit |
| Linux | PipeWire / XDG Portal | PipeWire |

---

## Phases

### Phase 1 — Foundation & Core Data Model
**Goal:** Build the skeleton app, data types, and state management. Nothing renders yet but the architecture is solid.

**Tasks:**
1. **CMake project setup**
   - Configure Qt6, FFmpeg, nlohmann/json, spdlog, GTest
   - Set up cross-platform build (Windows MSVC, macOS Clang, Linux GCC)
   - CI with GitHub Actions (build on all 3 platforms)

2. **Core data types** (`core/types.h`)
   - Port all TypeScript types: `ZoomRegion`, `TrimRegion`, `SpeedRegion`, `AnnotationRegion`, `CropRegion`, `AspectRatio`, `WebcamLayoutPreset`
   - Immutable value types (structs with const fields or builder pattern)
   - JSON serialization via nlohmann/json

3. **Editor state + undo/redo** (`core/EditorState`)
   - Immutable state snapshots (copy-on-write)
   - 80-slot circular history buffer
   - `pushState()`, `commitState()`, `undo()`, `redo()`
   - Unit tests for all state transitions

4. **Timeline model** (`core/TimelineModel`)
   - Add/remove/resize regions for each type
   - Overlap validation (zoom regions can overlap, trim regions cannot)
   - Time range queries (which regions are active at time T?)
   - Unit tests

5. **Zoom transform math** (`core/ZoomTransform`)
   - Port `computeZoomTransform()` — scale + translation from focus + depth
   - Port `clampFocus()` — keep zoom target within viewport bounds
   - Motion blur velocity calculation (frame-to-frame delta)
   - Unit tests against known values from the JS version

6. **Project file I/O** (`core/Project`)
   - Save/load `.openscreen` JSON format (version 2, compatible with Electron version)
   - Media path resolution (relative ↔ absolute)
   - Unit tests

7. **Application skeleton** (`app/`, `ui/windows/`)
   - `QApplication` subclass with settings
   - Empty `MainWindow` (editor), `HudOverlay`, `SourceSelector`
   - Dark theme stylesheet (QSS)
   - Menu bar with File > New/Open/Save/Export

**Deliverable:** App launches, shows empty editor window with menu bar. Core logic is tested. No recording or playback yet.

**Estimated effort:** ~2-3 weeks

---

### Phase 2 — Screen Recording & Capture
**Goal:** Record screen, mic, system audio, and webcam. Save to disk as raw media files.

**Tasks:**
1. **Screen capture abstraction** (`capture/ScreenCapture.h`)
   - Abstract interface: `start()`, `stop()`, `onFrame(callback)`
   - Frame format: raw BGRA pixels + timestamp

2. **Windows screen capture** (`capture/ScreenCaptureWin`)
   - DXGI Desktop Duplication API
   - Enumerate monitors and windows (for source selector)
   - Capture at native resolution, 60fps target
   - Cursor position sampling (100ms interval → CursorTelemetry)

3. **macOS screen capture** (`capture/ScreenCaptureMac`)
   - ScreenCaptureKit (`SCStream`, `SCContentFilter`)
   - Permission handling (`CGRequestScreenCaptureAccess`)
   - Window/display enumeration with thumbnails

4. **Linux screen capture** (`capture/ScreenCaptureLinux`)
   - PipeWire via `xdg-desktop-portal` (Wayland-compatible)
   - Fallback: X11 `XShmGetImage` for X11-only systems

5. **Audio capture** (`capture/AudioCapture*`)
   - System audio: WASAPI loopback (Win), ScreenCaptureKit (Mac), PipeWire (Linux)
   - Microphone: `QAudioSource` (cross-platform via Qt Multimedia)
   - Gain boost (1.4x for mic)
   - Audio level metering (RMS calculation for UI meter)

6. **Audio mixer** (`capture/AudioMixer`)
   - Mix mic + system audio into single PCM stream
   - Separate gain controls per source
   - Output: interleaved float32 PCM

7. **Webcam capture** (`capture/WebcamCapture`)
   - `QCamera` + `QMediaCaptureSession` + `QVideoSink`
   - Target 720p @ 30fps
   - Device enumeration and selection

8. **Recording encoder**
   - Use FFmpeg to encode captured frames in real-time
   - Video: H.264 (hardware-accelerated where available) → MP4
   - Audio: AAC or Opus → same MP4 container
   - Or write raw frames and encode post-capture (simpler, more reliable)

9. **HUD overlay window** (`ui/windows/HudOverlay`)
   - Frameless, always-on-top, semi-transparent
   - Record/stop button, timer, mic selector, audio level meter
   - System tray icon with recording state

10. **Source selector** (`ui/windows/SourceSelector`)
    - Grid of screen/window thumbnails
    - Click to select, returns source ID to recording system

**Deliverable:** Can record screen + audio + webcam, saves to disk. HUD overlay works. Source picker shows available screens/windows.

**Estimated effort:** ~3-4 weeks

---

### Phase 3 — Video Playback & Preview
**Goal:** Load recorded video, play it back in the editor with basic controls.

**Tasks:**
1. **Video decoder** (`export/VideoDecoder`)
   - FFmpeg-based: `avformat_open_input` → `avcodec_send_packet` → `avcodec_receive_frame`
   - Sequential decode (no seeking for now)
   - Frame → QImage or raw pixel buffer
   - Correct PTS handling for variable frame rate

2. **OpenGL video preview** (`ui/editor/VideoPreview`)
   - `QOpenGLWidget` subclass
   - Upload decoded frames as GL textures (`glTexSubImage2D`)
   - Basic vertex/fragment shader for textured quad
   - Maintain aspect ratio with letterboxing

3. **Playback controls** (`ui/editor/PlaybackControls`)
   - Play/pause toggle
   - Timeline scrubber (QSlider or custom painted widget)
   - Current time / total duration display
   - Frame-accurate seeking (decode keyframe + walk forward)

4. **Wallpaper rendering** (`render/WallpaperRenderer`)
   - Load built-in wallpaper images as textures
   - Solid color rendering
   - CSS gradient parsing → OpenGL gradient shader
   - Render behind video with padding/border-radius

5. **Basic zoom preview**
   - Apply `ZoomTransform` math to GL viewport
   - Interactive: click on preview to set zoom focus point
   - Smooth animated transitions between zoom levels

**Deliverable:** Load a recording, play it back with wallpaper and zoom preview. Scrub through the timeline.

**Estimated effort:** ~2-3 weeks

---

### Phase 4 — Timeline Editor
**Goal:** Full drag-and-drop timeline for zoom, trim, speed, and annotation regions.

**Tasks:**
1. **Custom timeline widget** (`ui/timeline/TimelineWidget`)
   - Custom `QWidget` with `paintEvent()` — no need for a library
   - Horizontal scrolling with zoom (Ctrl+scroll)
   - Time ruler with adaptive tick marks (ms → s → min)
   - Vertical rows for each effect type

2. **Timeline items** (`ui/timeline/TimelineItem`)
   - Colored rectangles per region type (blue=zoom, red=trim, green=speed, purple=annotation)
   - Drag to move, drag edges to resize
   - Snap to playhead or other item edges
   - Right-click context menu (delete, duplicate)
   - Selection highlight

3. **Timeline rows** (`ui/timeline/TimelineRow`)
   - Header with icon + label + "Add" button
   - Scroll sync with other rows
   - Collapse/expand

4. **Region editing integration**
   - Click zoom region → settings panel shows depth + focus controls
   - Click speed region → speed selector appears
   - Click annotation → annotation settings panel activates
   - Delete key removes selected region

5. **Auto-zoom suggestions** (port `zoomSuggestionUtils.ts`)
   - Analyze cursor telemetry for dwell points
   - Suggest zoom regions with calculated focus
   - "Accept" / "Dismiss" UI

6. **Playhead sync**
   - Playhead line on timeline follows video playback
   - Click on timeline to seek
   - Dragging playhead scrubs video

**Deliverable:** Fully interactive timeline. Add/remove/resize zoom/trim/speed/annotation regions. Auto-zoom suggestions work.

**Estimated effort:** ~3-4 weeks

---

### Phase 5 — Effects & Annotations
**Goal:** Full rendering pipeline with all visual effects and annotation support.

**Tasks:**
1. **GLSL shaders** (`resources/shaders/`)
   - `motion_blur.frag` — directional blur based on velocity vector
   - `gaussian_blur.frag` — standard blur (two-pass separable)
   - `drop_shadow.frag` — shadow for webcam overlay
   - `rounded_rect.frag` — border-radius masking via SDF

2. **Motion blur integration** (`render/Shaders`)
   - Calculate velocity from frame-to-frame camera position delta
   - Apply directional blur when velocity > 12 px/s threshold
   - Intensity slider maps to max blur radius (0-14px)

3. **Webcam compositing** (`render/WebcamRenderer`)
   - Picture-in-picture layout (corner overlay)
   - Vertical stack layout (side-by-side)
   - Drop shadow + border radius
   - Position calculated by `CompositeLayout` core module

4. **Crop region** (`ui/editor/CropControl`)
   - Draggable crop handles overlay on preview
   - Preview dims outside crop region
   - Applied via GL scissor or stencil mask

5. **Annotation overlay** (`ui/editor/AnnotationOverlay`)
   - Custom draggable/resizable widgets over video preview
   - Three types:
     - **Text:** QLabel-like with editable content, font, color, background
     - **Arrow:** QPainter-drawn arrow with 8 directions, color, stroke width
     - **Image:** QPixmap loaded from file, draggable/resizable
   - Position stored as % of preview area (responsive)

6. **Annotation settings panel** (`ui/editor/SettingsPanel`)
   - Font family selector (system fonts + Google Fonts subset)
   - Font size, color, background color pickers
   - Bold/italic/underline toggles
   - Arrow direction picker (8-way grid)
   - Arrow color and stroke width

7. **Settings panel** (`ui/editor/SettingsPanel`)
   - Accordion layout (QToolBox or custom)
   - Shadow intensity slider
   - Motion blur intensity slider
   - Border radius slider
   - Padding slider
   - Wallpaper picker (grid of thumbnails + color/gradient)
   - Aspect ratio selector (8 presets)
   - Webcam layout preset selector

**Deliverable:** All visual effects work in real-time preview. Annotations are editable. Settings panel controls all parameters.

**Estimated effort:** ~3-4 weeks

---

### Phase 6 — Export Pipeline
**Goal:** Export edited video to MP4 and GIF with all effects baked in.

**Tasks:**
1. **Offscreen renderer** (`export/FrameRenderer`)
   - `QOffscreenSurface` + `QOpenGLFramebufferObject`
   - Render each frame with all effects (zoom, blur, motion blur, crop, wallpaper, webcam, annotations)
   - Read pixels back: `glReadPixels` → raw RGBA buffer

2. **Sequential video decoder** (`export/VideoDecoder`)
   - Single-pass forward decode via FFmpeg
   - Skip trimmed regions (decode but don't yield)
   - Handle speed regions (adjust timestamps)
   - Frame resampling for target FPS

3. **Video encoder** (`export/VideoEncoder`)
   - H.264 with hardware acceleration:
     - Windows: NVENC or QSV via FFmpeg
     - macOS: VideoToolbox via FFmpeg
     - Linux: VAAPI via FFmpeg
   - Fallback: libx264 software encoding
   - Quality presets: medium (~20Mbps), good (~28-45Mbps), source
   - Adaptive bitrate based on resolution

4. **Audio processor** (`export/AudioProcessor`)
   - Decode audio from source via FFmpeg
   - Apply trim regions (skip audio in trimmed sections)
   - Apply speed regions with pitch preservation (libswresample or rubberband)
   - Re-encode as AAC

5. **MP4 muxer** (`export/Muxer`)
   - FFmpeg `avformat_write_header` / `av_interleaved_write_frame` / `av_write_trailer`
   - Interleave video + audio chunks
   - Proper PTS/DTS handling

6. **GIF exporter** (`export/GifExporter`)
   - FFmpeg `palettegen` + `paletteuse` filter chain
   - FPS presets: 15, 20, 25, 30
   - Size presets: 720p, 1080p, original
   - Loop control

7. **Export dialog** (`ui/editor/ExportDialog`)
   - Format selector (MP4 / GIF)
   - Quality selector (MP4 only)
   - GIF FPS and size presets
   - Progress bar with percentage + ETA
   - Cancel button (graceful abort)
   - File save dialog on completion

8. **Export worker thread**
   - Run entire export in `QThread` to keep UI responsive
   - Signal progress updates to dialog
   - Handle cancellation via atomic flag

**Deliverable:** Export working videos with all effects. MP4 and GIF output. Hardware-accelerated encoding where available.

**Estimated effort:** ~3-4 weeks

---

### Phase 7 — Polish & Platform Integration
**Goal:** Keyboard shortcuts, i18n, project persistence, system tray, and platform-specific polish.

**Tasks:**
1. **Keyboard shortcuts** (`app/Settings`)
   - Configurable binding system (QShortcut)
   - Default bindings matching Electron version (Z=zoom, T=trim, etc.)
   - Conflict detection
   - Shortcut editor dialog
   - Undo/redo: Ctrl+Z / Ctrl+Shift+Z

2. **Internationalization** (`i18n/`)
   - Port all locale strings to Qt `.ts` files
   - 3 locales: en, es, zh_CN
   - Qt Linguist for translator workflow
   - Dynamic locale switching without restart

3. **Project persistence**
   - Save/load `.openscreen` files (JSON, compatible with v2 format)
   - Recent projects list in File menu
   - Unsaved changes warning on close
   - Auto-save to temp location

4. **System tray**
   - `QSystemTrayIcon` with context menu
   - Recording state indicator (icon changes)
   - Quick access: show/hide, start/stop recording

5. **Platform polish**
   - macOS: native menu bar, permission request dialogs, Retina support
   - Windows: taskbar progress during export, high-DPI scaling
   - Linux: AppImage packaging, `.desktop` file, icon themes

6. **Installer / packaging**
   - Windows: NSIS or WiX installer (via CPack)
   - macOS: `.dmg` with drag-to-Applications
   - Linux: AppImage (via linuxdeployqt)
   - GitHub Actions CI/CD for all platforms

7. **Performance optimization**
   - Profile with Tracy or Qt Creator profiler
   - Optimize GL texture upload (PBO double-buffering)
   - Frame decode caching (3-frame lookahead)
   - Memory pooling for decoded frames

**Deliverable:** Production-ready app. All features ported. Installers for all 3 platforms.

**Estimated effort:** ~2-3 weeks

---

## Phase Summary

| Phase | Scope | Weeks | Cumulative |
|-------|-------|-------|------------|
| 1 | Foundation & Core | 2-3 | 2-3 |
| 2 | Screen Recording | 3-4 | 5-7 |
| 3 | Video Playback | 2-3 | 7-10 |
| 4 | Timeline Editor | 3-4 | 10-14 |
| 5 | Effects & Annotations | 3-4 | 13-18 |
| 6 | Export Pipeline | 3-4 | 16-22 |
| 7 | Polish & Packaging | 2-3 | 18-25 |

**Total estimate: ~18-25 weeks** for full feature parity.

---

## Key Architectural Decisions

### 1. Qt Widgets over QML
**Why:** The app is tool-heavy (timeline, settings panels, color pickers). Qt Widgets gives us mature, battle-tested controls. QML is better for fluid/animated UIs but adds complexity for editor-type tools. The video preview uses `QOpenGLWidget` either way.

### 2. FFmpeg over GStreamer
**Why:** FFmpeg is the industry standard for video processing. Better codec support, simpler API for our use case (decode → process → encode), and hardware acceleration on all platforms. GStreamer is pipeline-oriented which adds complexity we don't need.

### 3. OpenGL 3.3 over Vulkan
**Why:** OpenGL 3.3 is universally supported (including Intel integrated GPUs), simpler for 2D compositing + shader effects, and Qt has first-class `QOpenGLWidget` support. Vulkan is overkill for our rendering needs.

### 4. Immutable state with copy-on-write
**Why:** Matches the Electron version's architecture. Undo/redo becomes trivial (swap state pointers). Thread-safe for UI ↔ render thread communication.

### 5. Platform-specific capture, everything else cross-platform
**Why:** Screen capture is inherently platform-specific (no good cross-platform abstraction exists). Audio mixing, video processing, rendering, and UI are all cross-platform via Qt + FFmpeg + OpenGL.

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Platform capture APIs are complex and poorly documented | Start Phase 2 with Windows (best documented), then Mac, then Linux. Budget extra time for platform bugs. |
| FFmpeg API is C and error-prone | Create RAII wrappers for all FFmpeg resources. Use smart pointers. Write thorough unit tests. |
| OpenGL shader effects may not match PixiJS exactly | Port shaders one-by-one with visual comparison tests (render same frame in both, pixel diff). |
| Pitch-preserved speed change is hard without Web Audio | Use the `rubberband` library (MIT, used by Audacity) or `libsamplerate`. Both are battle-tested. |
| Timeline widget is complex to build from scratch | Start with basic functionality (rectangles you can drag). Iterate. The core math is already ported in Phase 1. |
| Cross-platform packaging is painful | Set up CI early (Phase 1). Test installers on real machines, not just CI. |

---

## Getting Started

```bash
# Prerequisites
# - Qt 6.6+ (install via online installer or system package manager)
# - FFmpeg 6.x dev libraries
# - CMake 3.25+
# - C++17 compiler (MSVC 2022, Clang 15+, GCC 12+)

# Clone and build
git clone https://github.com/user/openscreen-qt.git
cd openscreen-qt
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run
./build/openscreen

# Run tests
cd build && ctest --output-on-failure
```
