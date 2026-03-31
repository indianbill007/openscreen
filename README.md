> [!WARNING]
> This is very much in beta and might be buggy here and there (but hope you have a good experience!).

<p align="center">
  <img src="public/openscreen.png" alt="OpenScreen Logo" width="64" />
  <br />
  <br />
  <a href="https://deepwiki.com/siddharthvaddem/openscreen">
    <img src="https://deepwiki.com/badge.svg" alt="Ask DeepWiki" />
  </a>
</p>

# <p align="center">OpenScreen</p>

<p align="center"><strong>OpenScreen is your free, open-source alternative to Screen Studio (sort of).</strong></p>

If you don’t want to pay $29/month for Screen Studio but want a much simpler version that does what most people seem to need, making beautiful product demos and walkthroughs, here’s a free-to-use app for you. OpenScreen does not offer all Screen Studio features, but covers the basics well!

Screen Studio is an awesome product and this is definitely not a 1:1 clone. OpenScreen is a much simpler take, just the basics for folks who want control and don’t want to pay. If you need all the fancy features, your best bet is to support Screen Studio (they really do a great job, haha). But if you just want something free (no gotchas) and open, this project does the job!

OpenScreen is 100% free for personal and commercial use. Use it, modify it, distribute it. (Just be cool and give a shoutout if you feel like it!)

<p align="center">
	<img src="public/preview3.png" alt="OpenScreen App Preview 3" style="height: 320px; margin-right: 12px;" />
	<img src="public/preview4.png" alt="OpenScreen App Preview 4" style="height: 320px; margin-right: 12px;" />
</p>

## Core Features
- Record your whole screen or specific windows.
- Add automatic zooms or manual zooms (customizable depth levels).
- Record microphone audio and system audio capture.
- Customize the duration and position of zooms however you please.
- Crop video recordings to hide parts.
- Choose between wallpapers, solid colors, gradients or a custom background.
- Motion blur for smoother pan and zoom effects.
- Add annotations (text, arrows, images).
- Trim sections of the clip.
- Customize speed at different segments.
- Export in different aspect ratios and resolutions.

---

## Native C++/Qt Rewrite (In Progress)

We are rewriting OpenScreen from Electron to a **native C++17 / Qt6** application for better performance, smaller binary size, lower memory usage, and a truly native experience.

### Why the rewrite?

Electron bundles an entire Chromium browser, which means large binaries (~200MB+), high memory usage, and slower startup. For a screen recording and video editing app that needs real-time GPU rendering and efficient video processing, going native makes a significant difference:

| | Electron (current) | C++/Qt (new) |
|---|---|---|
| Binary size | ~200MB+ | ~30MB |
| Memory usage | ~300MB+ | ~80MB |
| Startup time | 3-5s | <1s |
| GPU rendering | PixiJS (WebGL) | OpenGL 3.3 (native) |
| Video processing | WebCodecs + mediabunny | FFmpeg (hardware-accelerated) |
| Screen capture | Electron desktopCapturer | Native OS APIs (DXGI, ScreenCaptureKit, PipeWire) |

### Tech Stack (new)
- **C++17** with Qt 6.6+ (Widgets, OpenGL, Multimedia)
- **FFmpeg 6.x** for video decode/encode/muxing with hardware acceleration
- **OpenGL 3.3** with custom GLSL shaders for zoom, blur, and motion effects
- **Platform-native screen capture**: DXGI Desktop Duplication (Windows), ScreenCaptureKit (macOS), PipeWire (Linux)
- **nlohmann/json** for project file serialization
- **GoogleTest** for unit testing

### What we've built so far

**Phase 1 (Complete)** delivered the full foundation — 38 files, 5,823 lines of C++, and 105 unit tests:

- All core data types ported from TypeScript (zoom regions, trim regions, speed regions, annotations, crop, aspect ratios)
- Editor state management with immutable snapshot-based undo/redo (80-slot history)
- Timeline model with region CRUD, overlap validation, and time-based queries
- Zoom transform math: camera positioning, focus clamping, motion blur velocity calculations
- Aspect ratio utilities (8 presets) and webcam composite layout engine (PiP + vertical stack)
- Project file I/O: save/load `.openscreen` JSON with full validation and normalization
- Cursor telemetry analysis for auto-zoom suggestions based on dwell detection
- Qt UI skeleton: MainWindow with menus, frameless HUD recording overlay, source selector dialog, comprehensive dark theme

### Rewrite Roadmap

| Phase | Status | Description |
|-------|--------|-------------|
| Phase 1 | Done | Foundation: core data types, editor state, undo/redo, zoom math, timeline model, project I/O, UI skeleton, 105 unit tests |
| Phase 2 | Planned | Screen recording: DXGI/ScreenCaptureKit/PipeWire capture, WASAPI/CoreAudio/PipeWire system audio, mic capture, audio mixing |
| Phase 3 | Planned | Video playback: FFmpeg decoder, OpenGL preview widget, wallpaper rendering, basic zoom preview |
| Phase 4 | Planned | Timeline editor: custom drag-and-drop widget, region manipulation, auto-zoom suggestions |
| Phase 5 | Planned | Effects & annotations: GLSL shaders (motion blur, gaussian blur, drop shadow), text/arrow/image overlays |
| Phase 6 | Planned | Export pipeline: H.264/H.265 with hardware acceleration, GIF export, pitch-preserved audio speed changes |
| Phase 7 | Planned | Polish: i18n (3 languages), keyboard shortcuts, system tray, installers (NSIS/DMG/AppImage) |

The Qt rewrite lives in the [`openscreen-qt/`](openscreen-qt/) directory. See the full [implementation plan](docs/IMPLEMENTATION_PLAN.md) for architecture details and phase breakdowns.

### Building the Qt version

```bash
# Prerequisites: Qt 6.6+, FFmpeg 6.x dev libs, CMake 3.25+, C++17 compiler
cd openscreen-qt
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run tests
cd build && ctest --output-on-failure
```

---

## Electron Version (current stable)

The current stable release uses the Electron stack. Download installers from the [Releases](https://github.com/siddharthvaddem/openscreen/releases) page.

### Installation

#### macOS

If you encounter issues with macOS Gatekeeper blocking the app (since it does not come with a developer certificate), you can bypass this by running the following command in your terminal after installation:

```bash
xattr -rd com.apple.quarantine /Applications/Openscreen.app
```

Note: Give your terminal Full Disk Access in **System Settings > Privacy & Security** to grant you access and then run the above command.

After running this command, proceed to **System Preferences > Security & Privacy** to grant the necessary permissions for "screen recording" and "accessibility". Once permissions are granted, you can launch the app.

#### Linux

Download the `.AppImage` file from the releases page. Make it executable and run:

```bash
chmod +x Openscreen-Linux-*.AppImage
./Openscreen-Linux-*.AppImage
```

You may need to grant screen recording permissions depending on your desktop environment.

**Note:** If the app fails to launch due to a "sandbox" error, run it with --no-sandbox:
```bash
./Openscreen-Linux-*.AppImage --no-sandbox
```

#### Windows

Works out of the box. Download the installer from the releases page.

### Limitations (Electron version)

System audio capture relies on Electron’s [desktopCapturer](https://www.electronjs.org/docs/latest/api/desktop-capturer) and has some platform-specific quirks:

- **macOS**: Requires macOS 13+. On macOS 14.2+ you’ll be prompted to grant audio capture permission. macOS 12 and below does not support system audio (mic still works).
- **Windows**: Works out of the box.
- **Linux**: Needs PipeWire (default on Ubuntu 22.04+, Fedora 34+). Older PulseAudio-only setups may not support system audio (mic should still work).

### Built with (Electron version)
- Electron
- React
- TypeScript
- Vite
- PixiJS
- dnd-timeline

---

## Contributing

Contributions are welcome! If you’d like to help out or see what’s currently being worked on, take a look at the open issues and the [project roadmap](https://github.com/users/siddharthvaddem/projects/3) to understand the current direction of the project and find ways to contribute.

For the Qt rewrite, see the [implementation plan](docs/IMPLEMENTATION_PLAN.md) and check the `openscreen-qt/` directory.

## License

This project is licensed under the [MIT License](./LICENSE). By using this software, you agree that the authors are not liable for any issues, damages, or claims arising from its use.
