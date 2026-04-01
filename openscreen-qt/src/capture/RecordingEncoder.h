#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// FFmpeg C headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace openscreen {

struct EncoderConfig {
    // Video
    int width = 1920;
    int height = 1080;
    int fps = 60;
    int64_t videoBitrate = 18'000'000;  // 18 Mbps default
    std::string videoCodec = "h264";     // h264, h265
    bool useHardwareAccel = true;

    // Audio
    int audioSampleRate = 48000;
    int audioChannels = 2;
    int64_t audioBitrate = 192'000;     // 192 kbps

    // Output
    std::string outputPath;             // file path for MP4 output
};

/// Bitrate computation matching Electron version:
/// 4K (3840x2160): 45 Mbps, QHD (2560x1440): 28 Mbps, Base: 18 Mbps
/// High frame rate (>=60fps): 1.7x boost
int64_t computeBitrate(int width, int height, int fps);

class RecordingEncoder {
public:
    explicit RecordingEncoder(const EncoderConfig& config);
    ~RecordingEncoder();

    // Non-copyable
    RecordingEncoder(const RecordingEncoder&) = delete;
    RecordingEncoder& operator=(const RecordingEncoder&) = delete;

    /// Initialize encoder and open output file. Returns false on failure.
    bool open();

    /// Submit a BGRA video frame for encoding
    void pushVideoFrame(const uint8_t* bgraData, int width, int height,
                        int stride, int64_t timestampMs);

    /// Submit interleaved float32 audio samples
    void pushAudioSamples(const float* data, int frameCount, int channels,
                          int sampleRate);

    /// Finalize encoding, flush buffers, close file. Returns output path.
    std::string close();

    /// Whether the encoder is open and accepting frames
    bool isOpen() const;

    /// Get encoding progress info
    int64_t encodedFrames() const;
    int64_t encodedDurationMs() const;

private:
    bool initVideo();
    bool initAudio();
    bool tryHardwareEncoder(const std::string& codecName);
    void encodeVideoFrame(const uint8_t* bgraData, int width, int height,
                          int stride, int64_t timestampMs);
    void encodeAudioSamples(const float* data, int frameCount);
    void flushVideo();
    void flushAudio();
    void freeResources();

    EncoderConfig config_;

    // FFmpeg contexts
    AVFormatContext* formatCtx_ = nullptr;
    AVCodecContext* videoCodecCtx_ = nullptr;
    AVCodecContext* audioCodecCtx_ = nullptr;
    AVStream* videoStream_ = nullptr;
    AVStream* audioStream_ = nullptr;
    SwsContext* swsCtx_ = nullptr;
    SwrContext* swrCtx_ = nullptr;
    AVFrame* videoFrame_ = nullptr;
    AVFrame* audioFrame_ = nullptr;
    AVPacket* packet_ = nullptr;

    std::atomic<bool> open_{false};
    std::atomic<int64_t> encodedFrames_{0};
    int64_t videoPts_ = 0;
    int64_t audioPts_ = 0;
    int audioFrameSize_ = 0;
    std::vector<float> audioResidual_;  // leftover samples between pushAudioSamples calls

    std::mutex videoMutex_;
    std::mutex audioMutex_;
};

} // namespace openscreen
