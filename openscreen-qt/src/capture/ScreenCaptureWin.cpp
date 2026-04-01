#ifdef _WIN32

#include "ScreenCaptureWin.h"

#include <comdef.h>
#include <dwmapi.h>

#include <algorithm>
#include <chrono>
#include <sstream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

namespace openscreen {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// RAII wrapper for COM initialization on the current thread.
struct ComInit {
    ComInit() { hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED); }
    ~ComInit() {
        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
    }
    ComInit(const ComInit&) = delete;
    ComInit& operator=(const ComInit&) = delete;
    HRESULT hr = S_OK;
};

int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
               steady_clock::now().time_since_epoch())
        .count();
}

/// Build a source ID string for a monitor output index.
std::string monitorId(int index) {
    return "monitor:" + std::to_string(index);
}

/// Build a source ID string for a window handle.
std::string windowId(HWND hwnd) {
    std::ostringstream oss;
    oss << "window:" << reinterpret_cast<uintptr_t>(hwnd);
    return oss.str();
}

/// Parse a source ID that starts with "monitor:" and return the index.
bool parseMonitorId(const std::string& id, int& outIndex) {
    const std::string prefix = "monitor:";
    if (id.rfind(prefix, 0) != 0) return false;
    try {
        outIndex = std::stoi(id.substr(prefix.size()));
        return true;
    } catch (...) {
        return false;
    }
}

/// Callback used by EnumWindows to collect visible, titled windows.
struct WindowEnumContext {
    std::vector<CaptureSource> sources;
};

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;

    // Skip windows with no title text.
    wchar_t titleBuf[256]{};
    int len = GetWindowTextW(hwnd, titleBuf, 256);
    if (len <= 0) return TRUE;

    // Skip tool windows, cloaked windows (UWP), etc.
    DWORD cloaked = 0;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) return TRUE;

    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

    RECT rc{};
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return TRUE;

    auto* ctx = reinterpret_cast<WindowEnumContext*>(lParam);

    CaptureSource src;
    src.id = windowId(hwnd);
    // Convert wide title to UTF-8.
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, titleBuf, len,
                                      nullptr, 0, nullptr, nullptr);
    if (utf8Len > 0) {
        src.name.resize(static_cast<size_t>(utf8Len));
        WideCharToMultiByte(CP_UTF8, 0, titleBuf, len,
                            src.name.data(), utf8Len, nullptr, nullptr);
    }
    src.isWindow = true;
    src.width = w;
    src.height = h;

    // Attempt a lightweight thumbnail via PrintWindow.
    {
        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, w, h);
        HGDIOBJ old = SelectObject(hdcMem, hBmp);

        // PW_RENDERFULLCONTENT (0x00000002) captures DWM‑composed content.
        PrintWindow(hwnd, hdcMem, 0x00000002);

        // Convert HBITMAP -> QPixmap via QImage.
        BITMAPINFOHEADER bi{};
        bi.biSize = sizeof(bi);
        bi.biWidth = w;
        bi.biHeight = -h; // top-down
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;

        std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 4);
        GetDIBits(hdcMem, hBmp, 0, static_cast<UINT>(h),
                  pixels.data(), reinterpret_cast<BITMAPINFO*>(&bi),
                  DIB_RGB_COLORS);

        QImage img(pixels.data(), w, h, w * 4, QImage::Format_ARGB32);
        src.thumbnail = QPixmap::fromImage(
            img.scaled(160, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        SelectObject(hdcMem, old);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
    }

    ctx->sources.push_back(std::move(src));
    return TRUE;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------

ScreenCaptureWin::~ScreenCaptureWin() {
    stop();
}

// ---------------------------------------------------------------------------
// D3D / DXGI init
// ---------------------------------------------------------------------------

bool ScreenCaptureWin::initD3D() {
    if (device_) return true;

    D3D_FEATURE_LEVEL featureLevel{};
    const D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0};

    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,                    // no software rasterizer
        0,                          // flags
        featureLevels,
        1,
        D3D11_SDK_VERSION,
        device_.ReleaseAndGetAddressOf(),
        &featureLevel,
        context_.ReleaseAndGetAddressOf());

    return SUCCEEDED(hr);
}

bool ScreenCaptureWin::initDuplication() {
    if (!device_) return false;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = device_.As(&dxgiDevice);
    if (FAILED(hr)) return false;

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
    if (FAILED(hr)) return false;

    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    hr = adapter->EnumOutputs(static_cast<UINT>(outputIndex_),
                              output.GetAddressOf());
    if (FAILED(hr)) return false;

    Microsoft::WRL::ComPtr<IDXGIOutput1> output1;
    hr = output.As(&output1);
    if (FAILED(hr)) return false;

    hr = output1->DuplicateOutput(device_.Get(),
                                  duplication_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    DXGI_OUTPUT_DESC desc{};
    output->GetDesc(&desc);
    captureWidth_ = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
    captureHeight_ = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;

    return true;
}

// ---------------------------------------------------------------------------
// Source enumeration
// ---------------------------------------------------------------------------

std::vector<CaptureSource> ScreenCaptureWin::enumerateSources() {
    std::vector<CaptureSource> sources;

    // --- Monitors -----------------------------------------------------------
    if (!initD3D()) return sources;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(device_.As(&dxgiDevice))) return sources;

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDevice->GetAdapter(adapter.GetAddressOf()))) return sources;

    for (UINT i = 0; ; ++i) {
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        HRESULT hr = adapter->EnumOutputs(i, output.GetAddressOf());
        if (hr == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(hr)) continue;

        DXGI_OUTPUT_DESC desc{};
        output->GetDesc(&desc);

        CaptureSource src;
        src.id = monitorId(static_cast<int>(i));

        // Convert device name (wide) to UTF-8.
        int nameLen = WideCharToMultiByte(CP_UTF8, 0, desc.DeviceName, -1,
                                          nullptr, 0, nullptr, nullptr);
        if (nameLen > 0) {
            src.name.resize(static_cast<size_t>(nameLen - 1));
            WideCharToMultiByte(CP_UTF8, 0, desc.DeviceName, -1,
                                src.name.data(), nameLen, nullptr, nullptr);
        }
        src.isWindow = false;
        src.width = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
        src.height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;

        sources.push_back(std::move(src));
    }

    // --- Windows ------------------------------------------------------------
    WindowEnumContext ctx;
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    sources.insert(sources.end(),
                   std::make_move_iterator(ctx.sources.begin()),
                   std::make_move_iterator(ctx.sources.end()));

    return sources;
}

// ---------------------------------------------------------------------------
// Source selection
// ---------------------------------------------------------------------------

bool ScreenCaptureWin::selectSource(const std::string& sourceId) {
    int idx = 0;
    if (parseMonitorId(sourceId, idx)) {
        selectedSourceId_ = sourceId;
        outputIndex_ = idx;
        return true;
    }

    // For windows we store the id but currently only support monitor capture
    // via DXGI duplication. Window capture would require a different path
    // (e.g. WGC). Accept the id so UI can track it.
    if (sourceId.rfind("window:", 0) == 0) {
        selectedSourceId_ = sourceId;
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Start / Stop
// ---------------------------------------------------------------------------

bool ScreenCaptureWin::start(FrameCallback frameCallback) {
    if (capturing_.load()) return false;

    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        frameCallback_ = std::move(frameCallback);
    }

    if (!initD3D()) return false;
    if (!initDuplication()) return false;

    capturing_.store(true);
    captureThread_ = std::thread(&ScreenCaptureWin::captureLoop, this);
    return true;
}

void ScreenCaptureWin::stop() {
    if (!capturing_.load()) return;

    capturing_.store(false);
    if (captureThread_.joinable()) {
        captureThread_.join();
    }

    duplication_.Reset();
}

bool ScreenCaptureWin::isCapturing() const {
    return capturing_.load();
}

void ScreenCaptureWin::setCursorCallback(CursorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    cursorCallback_ = std::move(callback);
}

// ---------------------------------------------------------------------------
// Capture loop (runs on captureThread_)
// ---------------------------------------------------------------------------

void ScreenCaptureWin::captureLoop() {
    ComInit comGuard; // COM init for this thread
    (void)comGuard;

    auto lastCursorReport = std::chrono::steady_clock::now();
    constexpr auto kCursorInterval = std::chrono::milliseconds(100);

    // Staging texture for CPU read-back (created once, reused each frame).
    Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTex;

    while (capturing_.load()) {
        // ---- Acquire frame ------------------------------------------------
        Microsoft::WRL::ComPtr<IDXGIResource> desktopResource;
        DXGI_OUTDUPL_FRAME_INFO frameInfo{};

        HRESULT hr = duplication_->AcquireNextFrame(
            16, // ~60 fps timeout in ms
            &frameInfo,
            desktopResource.GetAddressOf());

        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            continue; // no new frame yet
        }

        if (FAILED(hr)) {
            // Lost access (e.g. resolution change, secure desktop).
            // Re-init duplication and retry.
            duplication_.Reset();
            if (!initDuplication()) {
                // Could not recover — stop capturing.
                capturing_.store(false);
                break;
            }
            continue;
        }

        // ---- Get GPU texture -----------------------------------------------
        Microsoft::WRL::ComPtr<ID3D11Texture2D> gpuTex;
        hr = desktopResource.As(&gpuTex);
        if (FAILED(hr)) {
            duplication_->ReleaseFrame();
            continue;
        }

        D3D11_TEXTURE2D_DESC texDesc{};
        gpuTex->GetDesc(&texDesc);

        // ---- Create or recreate staging texture if needed ------------------
        if (!stagingTex) {
            D3D11_TEXTURE2D_DESC stageDesc = texDesc;
            stageDesc.Usage = D3D11_USAGE_STAGING;
            stageDesc.BindFlags = 0;
            stageDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            stageDesc.MiscFlags = 0;

            hr = device_->CreateTexture2D(&stageDesc, nullptr,
                                          stagingTex.ReleaseAndGetAddressOf());
            if (FAILED(hr)) {
                duplication_->ReleaseFrame();
                continue;
            }
        }

        // ---- Copy GPU -> staging -------------------------------------------
        context_->CopyResource(stagingTex.Get(), gpuTex.Get());

        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr = context_->Map(stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (SUCCEEDED(hr)) {
            CaptureFrame frame;
            frame.data = static_cast<const uint8_t*>(mapped.pData);
            frame.width = static_cast<int>(texDesc.Width);
            frame.height = static_cast<int>(texDesc.Height);
            frame.stride = static_cast<int>(mapped.RowPitch);
            frame.timestampMs = nowMs();

            {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (frameCallback_) {
                    frameCallback_(frame);
                }
            }

            context_->Unmap(stagingTex.Get(), 0);
        }

        duplication_->ReleaseFrame();

        // ---- Cursor position (throttled) -----------------------------------
        auto now = std::chrono::steady_clock::now();
        if (now - lastCursorReport >= kCursorInterval) {
            lastCursorReport = now;

            POINT pt{};
            if (GetCursorPos(&pt) && captureWidth_ > 0 && captureHeight_ > 0) {
                double cx = static_cast<double>(pt.x) / captureWidth_;
                double cy = static_cast<double>(pt.y) / captureHeight_;

                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (cursorCallback_) {
                    cursorCallback_(cx, cy, nowMs());
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<ScreenCapture> ScreenCapture::create() {
    return std::make_unique<ScreenCaptureWin>();
}

} // namespace openscreen

#endif // _WIN32
