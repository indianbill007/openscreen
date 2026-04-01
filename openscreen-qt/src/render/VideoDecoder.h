#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace openscreen {

struct DecodedFrame {
    std::vector<uint8_t> data;  // BGRA pixel data (owned)
    int width = 0;
    int height = 0;
    int stride = 0;
    int64_t timestampMs = 0;
    int64_t durationMs = 0;
};

struct VideoInfo {
    int width = 0;
    int height = 0;
    double fps = 0.0;
    int64_t durationMs = 0;
    int64_t totalFrames = 0;
    std::string codecName;
};

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

    /// Open a video file for decoding
    bool open(const std::string& filePath);

    /// Close the file and free resources
    void close();

    /// Whether a file is currently open
    bool isOpen() const;

    /// Get video metadata
    VideoInfo info() const;

    /// Decode the next frame. Returns nullptr when EOF.
    std::unique_ptr<DecodedFrame> decodeNextFrame();

    /// Seek to the nearest keyframe at or before the given time.
    /// After seeking, call decodeNextFrame() to get the frame.
    bool seekTo(int64_t timestampMs);

    /// Seek to exact time by seeking to keyframe then walking forward.
    /// Returns the decoded frame at the target time.
    std::unique_ptr<DecodedFrame> seekAndDecode(int64_t timestampMs);

    /// Get the current decode position in milliseconds
    int64_t currentPositionMs() const;

private:
    bool decodePacket(AVPacket* packet);
    std::unique_ptr<DecodedFrame> convertFrame();
    int64_t ptsToMs(int64_t pts) const;
    int64_t msToTs(int64_t ms) const;

    AVFormatContext* formatCtx_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    SwsContext* swsCtx_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVFrame* bgraFrame_ = nullptr;
    AVPacket* packet_ = nullptr;

    int videoStreamIndex_ = -1;
    VideoInfo info_;
    int64_t currentPtsMs_ = 0;
    bool open_ = false;
};

}  // namespace openscreen
