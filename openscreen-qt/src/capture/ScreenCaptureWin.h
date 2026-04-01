#pragma once
#ifdef _WIN32

#include "ScreenCapture.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace openscreen {

class ScreenCaptureWin : public ScreenCapture {
public:
    ScreenCaptureWin() = default;
    ~ScreenCaptureWin() override;

    ScreenCaptureWin(const ScreenCaptureWin&) = delete;
    ScreenCaptureWin& operator=(const ScreenCaptureWin&) = delete;

    std::vector<CaptureSource> enumerateSources() override;
    bool selectSource(const std::string& sourceId) override;
    bool start(FrameCallback frameCallback) override;
    void stop() override;
    bool isCapturing() const override;
    void setCursorCallback(CursorCallback callback) override;

private:
    bool initD3D();
    bool initDuplication();
    void captureLoop();

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;

    std::string selectedSourceId_;
    int outputIndex_ = 0;

    std::thread captureThread_;
    std::atomic<bool> capturing_{false};

    FrameCallback frameCallback_;
    CursorCallback cursorCallback_;
    std::mutex callbackMutex_;

    int captureWidth_ = 0;
    int captureHeight_ = 0;
};

} // namespace openscreen

#endif // _WIN32
