#pragma once
#ifdef __linux__

#include "ScreenCapture.h"

namespace openscreen {

class ScreenCaptureLinux : public ScreenCapture {
public:
    std::vector<CaptureSource> enumerateSources() override { return {}; }
    bool selectSource(const std::string&) override { return false; }
    bool start(FrameCallback) override { return false; }
    void stop() override {}
    bool isCapturing() const override { return false; }
    void setCursorCallback(CursorCallback) override {}
};

} // namespace openscreen
#endif
