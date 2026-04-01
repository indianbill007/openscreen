#include "RecordingEncoder.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>

namespace openscreen {

// ---------------------------------------------------------------------------
// Bitrate computation matching Electron version
// ---------------------------------------------------------------------------

static constexpr int64_t kFourKPixels = 3840 * 2160;
static constexpr int64_t kQhdPixels  = 2560 * 1440;

static constexpr int64_t kBitrate4K   = 45'000'000;
static constexpr int64_t kBitrateQhd  = 28'000'000;
static constexpr int64_t kBitrateBase = 18'000'000;

static constexpr double kHighFpsBoost = 1.7;

int64_t computeBitrate(int width, int height, int fps) {
    const int64_t pixels = static_cast<int64_t>(width) * height;
    const double boost = (fps >= 60) ? kHighFpsBoost : 1.0;

    if (pixels >= kFourKPixels) {
        return static_cast<int64_t>(kBitrate4K * boost);
    }
    if (pixels >= kQhdPixels) {
        return static_cast<int64_t>(kBitrateQhd * boost);
    }
    return static_cast<int64_t>(kBitrateBase * boost);
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

RecordingEncoder::RecordingEncoder(const EncoderConfig& config)
    : config_(config) {}

RecordingEncoder::~RecordingEncoder() {
    if (open_.load()) {
        try {
            close();
        } catch (...) {
            spdlog::error("RecordingEncoder: exception during destructor close");
        }
    }
    freeResources();
}

// ---------------------------------------------------------------------------
// open / close / status
// ---------------------------------------------------------------------------

bool RecordingEncoder::open() {
    if (open_.load()) {
        spdlog::warn("RecordingEncoder: already open");
        return false;
    }

    // Allocate output format context for MP4
    int ret = avformat_alloc_output_context2(&formatCtx_, nullptr, "mp4",
                                              config_.outputPath.c_str());
    if (ret < 0 || !formatCtx_) {
        spdlog::error("RecordingEncoder: failed to create MP4 output context (err={})", ret);
        return false;
    }

    if (!initVideo()) {
        spdlog::error("RecordingEncoder: video initialization failed");
        freeResources();
        return false;
    }

    if (!initAudio()) {
        spdlog::error("RecordingEncoder: audio initialization failed");
        freeResources();
        return false;
    }

    // Open output file
    if (!(formatCtx_->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&formatCtx_->pb, config_.outputPath.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            spdlog::error("RecordingEncoder: failed to open output file '{}' (err={})",
                          config_.outputPath, ret);
            freeResources();
            return false;
        }
    }

    // Write file header
    ret = avformat_write_header(formatCtx_, nullptr);
    if (ret < 0) {
        spdlog::error("RecordingEncoder: failed to write header (err={})", ret);
        freeResources();
        return false;
    }

    // Allocate shared packet
    packet_ = av_packet_alloc();
    if (!packet_) {
        spdlog::error("RecordingEncoder: failed to allocate packet");
        freeResources();
        return false;
    }

    encodedFrames_.store(0);
    videoPts_ = 0;
    audioPts_ = 0;
    audioResidual_.clear();
    open_.store(true);

    spdlog::info("RecordingEncoder: opened '{}' ({}x{} @{}fps, video={}bps, audio={}bps)",
                 config_.outputPath, config_.width, config_.height, config_.fps,
                 config_.videoBitrate, config_.audioBitrate);
    return true;
}

std::string RecordingEncoder::close() {
    if (!open_.load()) {
        return config_.outputPath;
    }
    open_.store(false);

    // Flush delayed frames
    flushVideo();
    flushAudio();

    // Write trailer
    if (formatCtx_) {
        int ret = av_write_trailer(formatCtx_);
        if (ret < 0) {
            spdlog::error("RecordingEncoder: failed to write trailer (err={})", ret);
        }
    }

    freeResources();

    spdlog::info("RecordingEncoder: closed '{}' ({} frames encoded)",
                 config_.outputPath, encodedFrames_.load());
    return config_.outputPath;
}

bool RecordingEncoder::isOpen() const {
    return open_.load();
}

int64_t RecordingEncoder::encodedFrames() const {
    return encodedFrames_.load();
}

int64_t RecordingEncoder::encodedDurationMs() const {
    const int64_t frames = encodedFrames_.load();
    if (config_.fps <= 0) return 0;
    return (frames * 1000) / config_.fps;
}

// ---------------------------------------------------------------------------
// Video initialization
// ---------------------------------------------------------------------------

bool RecordingEncoder::tryHardwareEncoder(const std::string& codecName) {
    const AVCodec* codec = avcodec_find_encoder_by_name(codecName.c_str());
    if (!codec) {
        spdlog::info("RecordingEncoder: HW encoder '{}' not found", codecName);
        return false;
    }

    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        return false;
    }

    ctx->width = config_.width;
    ctx->height = config_.height;
    ctx->time_base = {1, config_.fps};
    ctx->framerate = {config_.fps, 1};
    ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx->bit_rate = config_.videoBitrate;
    ctx->gop_size = config_.fps * 2;
    ctx->max_b_frames = 2;

    if (formatCtx_->oformat->flags & AVFMT_GLOBALHEADER) {
        ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    int ret = avcodec_open2(ctx, codec, nullptr);
    if (ret < 0) {
        spdlog::info("RecordingEncoder: HW encoder '{}' failed to open (err={})",
                     codecName, ret);
        avcodec_free_context(&ctx);
        return false;
    }

    videoCodecCtx_ = ctx;
    spdlog::info("RecordingEncoder: using HW encoder '{}'", codecName);
    return true;
}

bool RecordingEncoder::initVideo() {
    bool hwOpened = false;

    if (config_.useHardwareAccel) {
        // Platform-specific hardware encoder candidates
#if defined(_WIN32)
        const std::vector<std::string> hwEncoders = {
            "h264_nvenc", "h264_qsv", "h264_amf"
        };
#elif defined(__APPLE__)
        const std::vector<std::string> hwEncoders = {
            "h264_videotoolbox"
        };
#else
        const std::vector<std::string> hwEncoders = {
            "h264_vaapi"
        };
#endif
        for (const auto& name : hwEncoders) {
            if (tryHardwareEncoder(name)) {
                hwOpened = true;
                break;
            }
        }
    }

    // Fallback to software encoder
    if (!hwOpened) {
        const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
        if (!codec) {
            spdlog::error("RecordingEncoder: libx264 encoder not found");
            return false;
        }

        videoCodecCtx_ = avcodec_alloc_context3(codec);
        if (!videoCodecCtx_) {
            spdlog::error("RecordingEncoder: failed to allocate video codec context");
            return false;
        }

        videoCodecCtx_->width = config_.width;
        videoCodecCtx_->height = config_.height;
        videoCodecCtx_->time_base = {1, config_.fps};
        videoCodecCtx_->framerate = {config_.fps, 1};
        videoCodecCtx_->pix_fmt = AV_PIX_FMT_YUV420P;
        videoCodecCtx_->bit_rate = config_.videoBitrate;
        videoCodecCtx_->gop_size = config_.fps * 2;
        videoCodecCtx_->max_b_frames = 2;

        if (formatCtx_->oformat->flags & AVFMT_GLOBALHEADER) {
            videoCodecCtx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        // Real-time encoding settings for libx264
        av_opt_set(videoCodecCtx_->priv_data, "preset", "medium", 0);
        av_opt_set(videoCodecCtx_->priv_data, "tune", "zerolatency", 0);

        int ret = avcodec_open2(videoCodecCtx_, codec, nullptr);
        if (ret < 0) {
            spdlog::error("RecordingEncoder: failed to open libx264 encoder (err={})", ret);
            return false;
        }
        spdlog::info("RecordingEncoder: using software encoder 'libx264'");
    }

    // Create video stream
    videoStream_ = avformat_new_stream(formatCtx_, nullptr);
    if (!videoStream_) {
        spdlog::error("RecordingEncoder: failed to create video stream");
        return false;
    }
    videoStream_->time_base = videoCodecCtx_->time_base;

    int ret = avcodec_parameters_from_context(videoStream_->codecpar, videoCodecCtx_);
    if (ret < 0) {
        spdlog::error("RecordingEncoder: failed to copy video codec params (err={})", ret);
        return false;
    }

    // Create SWS context for BGRA -> YUV420P conversion
    swsCtx_ = sws_getContext(
        config_.width, config_.height, AV_PIX_FMT_BGRA,
        config_.width, config_.height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsCtx_) {
        spdlog::error("RecordingEncoder: failed to create SWS context");
        return false;
    }

    // Allocate video frame (YUV420P)
    videoFrame_ = av_frame_alloc();
    if (!videoFrame_) {
        spdlog::error("RecordingEncoder: failed to allocate video frame");
        return false;
    }
    videoFrame_->format = AV_PIX_FMT_YUV420P;
    videoFrame_->width = config_.width;
    videoFrame_->height = config_.height;

    ret = av_frame_get_buffer(videoFrame_, 0);
    if (ret < 0) {
        spdlog::error("RecordingEncoder: failed to allocate video frame buffer (err={})", ret);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Audio initialization
// ---------------------------------------------------------------------------

bool RecordingEncoder::initAudio() {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        spdlog::error("RecordingEncoder: AAC encoder not found");
        return false;
    }

    audioCodecCtx_ = avcodec_alloc_context3(codec);
    if (!audioCodecCtx_) {
        spdlog::error("RecordingEncoder: failed to allocate audio codec context");
        return false;
    }

    audioCodecCtx_->sample_rate = config_.audioSampleRate;
    audioCodecCtx_->bit_rate = config_.audioBitrate;
    audioCodecCtx_->sample_fmt = AV_SAMPLE_FMT_FLTP;  // AAC expects planar float
    av_channel_layout_default(&audioCodecCtx_->ch_layout, config_.audioChannels);

    if (formatCtx_->oformat->flags & AVFMT_GLOBALHEADER) {
        audioCodecCtx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    int ret = avcodec_open2(audioCodecCtx_, codec, nullptr);
    if (ret < 0) {
        spdlog::error("RecordingEncoder: failed to open AAC encoder (err={})", ret);
        return false;
    }

    audioFrameSize_ = audioCodecCtx_->frame_size;
    if (audioFrameSize_ <= 0) {
        audioFrameSize_ = 1024;  // default AAC frame size
    }

    // Create audio stream
    audioStream_ = avformat_new_stream(formatCtx_, nullptr);
    if (!audioStream_) {
        spdlog::error("RecordingEncoder: failed to create audio stream");
        return false;
    }
    audioStream_->time_base = {1, config_.audioSampleRate};

    ret = avcodec_parameters_from_context(audioStream_->codecpar, audioCodecCtx_);
    if (ret < 0) {
        spdlog::error("RecordingEncoder: failed to copy audio codec params (err={})", ret);
        return false;
    }

    // Create SWR context: interleaved float32 -> planar float (FLTP)
    ret = swr_alloc_set_opts2(
        &swrCtx_,
        &audioCodecCtx_->ch_layout, AV_SAMPLE_FMT_FLTP, config_.audioSampleRate,
        &audioCodecCtx_->ch_layout, AV_SAMPLE_FMT_FLT,  config_.audioSampleRate,
        0, nullptr);
    if (ret < 0 || !swrCtx_) {
        spdlog::error("RecordingEncoder: failed to allocate SWR context (err={})", ret);
        return false;
    }

    ret = swr_init(swrCtx_);
    if (ret < 0) {
        spdlog::error("RecordingEncoder: failed to init SWR context (err={})", ret);
        return false;
    }

    // Allocate audio frame
    audioFrame_ = av_frame_alloc();
    if (!audioFrame_) {
        spdlog::error("RecordingEncoder: failed to allocate audio frame");
        return false;
    }
    audioFrame_->format = AV_SAMPLE_FMT_FLTP;
    audioFrame_->sample_rate = config_.audioSampleRate;
    av_channel_layout_copy(&audioFrame_->ch_layout, &audioCodecCtx_->ch_layout);
    audioFrame_->nb_samples = audioFrameSize_;

    ret = av_frame_get_buffer(audioFrame_, 0);
    if (ret < 0) {
        spdlog::error("RecordingEncoder: failed to allocate audio frame buffer (err={})", ret);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Video encoding
// ---------------------------------------------------------------------------

void RecordingEncoder::pushVideoFrame(const uint8_t* bgraData, int width,
                                       int height, int stride,
                                       int64_t /*timestampMs*/) {
    if (!open_.load()) return;
    std::lock_guard<std::mutex> lock(videoMutex_);
    encodeVideoFrame(bgraData, width, height, stride, 0);
}

void RecordingEncoder::encodeVideoFrame(const uint8_t* bgraData, int width,
                                         int height, int stride,
                                         int64_t /*timestampMs*/) {
    // Ensure frame buffer is writable
    int ret = av_frame_make_writable(videoFrame_);
    if (ret < 0) {
        spdlog::error("RecordingEncoder: video frame not writable (err={})", ret);
        return;
    }

    // Convert BGRA -> YUV420P
    const uint8_t* srcSlice[] = {bgraData};
    const int srcStride[] = {stride};

    sws_scale(swsCtx_, srcSlice, srcStride, 0, height,
              videoFrame_->data, videoFrame_->linesize);

    videoFrame_->pts = videoPts_++;

    // Send frame to encoder
    ret = avcodec_send_frame(videoCodecCtx_, videoFrame_);
    if (ret < 0) {
        spdlog::error("RecordingEncoder: avcodec_send_frame (video) failed (err={})", ret);
        return;
    }

    // Receive and write all available packets
    while (true) {
        ret = avcodec_receive_packet(videoCodecCtx_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            spdlog::error("RecordingEncoder: avcodec_receive_packet (video) failed (err={})", ret);
            break;
        }

        av_packet_rescale_ts(packet_, videoCodecCtx_->time_base,
                             videoStream_->time_base);
        packet_->stream_index = videoStream_->index;

        ret = av_interleaved_write_frame(formatCtx_, packet_);
        if (ret < 0) {
            spdlog::error("RecordingEncoder: av_interleaved_write_frame (video) failed (err={})", ret);
        }
        av_packet_unref(packet_);
    }

    encodedFrames_.fetch_add(1);
}

// ---------------------------------------------------------------------------
// Audio encoding
// ---------------------------------------------------------------------------

void RecordingEncoder::pushAudioSamples(const float* data, int frameCount,
                                         int channels, int /*sampleRate*/) {
    if (!open_.load()) return;
    std::lock_guard<std::mutex> lock(audioMutex_);

    // Append incoming samples to residual buffer
    const int totalSamples = frameCount * channels;
    audioResidual_.insert(audioResidual_.end(), data, data + totalSamples);

    // Process full frames from the residual buffer
    const int samplesPerFrame = audioFrameSize_ * config_.audioChannels;

    while (static_cast<int>(audioResidual_.size()) >= samplesPerFrame) {
        encodeAudioSamples(audioResidual_.data(), audioFrameSize_);

        // Remove consumed samples
        audioResidual_.erase(audioResidual_.begin(),
                             audioResidual_.begin() + samplesPerFrame);
    }
}

void RecordingEncoder::encodeAudioSamples(const float* data, int frameCount) {
    int ret = av_frame_make_writable(audioFrame_);
    if (ret < 0) {
        spdlog::error("RecordingEncoder: audio frame not writable (err={})", ret);
        return;
    }

    audioFrame_->nb_samples = frameCount;

    // Convert interleaved float32 -> planar float
    const uint8_t* srcData[] = {reinterpret_cast<const uint8_t*>(data)};
    ret = swr_convert(swrCtx_,
                      audioFrame_->data, frameCount,
                      srcData, frameCount);
    if (ret < 0) {
        spdlog::error("RecordingEncoder: swr_convert failed (err={})", ret);
        return;
    }

    audioFrame_->pts = audioPts_;
    audioPts_ += frameCount;

    // Send frame to encoder
    ret = avcodec_send_frame(audioCodecCtx_, audioFrame_);
    if (ret < 0) {
        spdlog::error("RecordingEncoder: avcodec_send_frame (audio) failed (err={})", ret);
        return;
    }

    // Receive and write all available packets
    while (true) {
        ret = avcodec_receive_packet(audioCodecCtx_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            spdlog::error("RecordingEncoder: avcodec_receive_packet (audio) failed (err={})", ret);
            break;
        }

        av_packet_rescale_ts(packet_, audioCodecCtx_->time_base,
                             audioStream_->time_base);
        packet_->stream_index = audioStream_->index;

        ret = av_interleaved_write_frame(formatCtx_, packet_);
        if (ret < 0) {
            spdlog::error("RecordingEncoder: av_interleaved_write_frame (audio) failed (err={})", ret);
        }
        av_packet_unref(packet_);
    }
}

// ---------------------------------------------------------------------------
// Flush
// ---------------------------------------------------------------------------

void RecordingEncoder::flushVideo() {
    if (!videoCodecCtx_) return;
    std::lock_guard<std::mutex> lock(videoMutex_);

    // Send NULL frame to flush delayed frames
    int ret = avcodec_send_frame(videoCodecCtx_, nullptr);
    if (ret < 0) {
        spdlog::warn("RecordingEncoder: flush video send_frame failed (err={})", ret);
        return;
    }

    while (true) {
        ret = avcodec_receive_packet(videoCodecCtx_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            spdlog::error("RecordingEncoder: flush video receive_packet failed (err={})", ret);
            break;
        }

        av_packet_rescale_ts(packet_, videoCodecCtx_->time_base,
                             videoStream_->time_base);
        packet_->stream_index = videoStream_->index;

        ret = av_interleaved_write_frame(formatCtx_, packet_);
        if (ret < 0) {
            spdlog::error("RecordingEncoder: flush video write_frame failed (err={})", ret);
        }
        av_packet_unref(packet_);
    }
}

void RecordingEncoder::flushAudio() {
    if (!audioCodecCtx_) return;
    std::lock_guard<std::mutex> lock(audioMutex_);

    // Send NULL frame to flush delayed frames
    int ret = avcodec_send_frame(audioCodecCtx_, nullptr);
    if (ret < 0) {
        spdlog::warn("RecordingEncoder: flush audio send_frame failed (err={})", ret);
        return;
    }

    while (true) {
        ret = avcodec_receive_packet(audioCodecCtx_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            spdlog::error("RecordingEncoder: flush audio receive_packet failed (err={})", ret);
            break;
        }

        av_packet_rescale_ts(packet_, audioCodecCtx_->time_base,
                             audioStream_->time_base);
        packet_->stream_index = audioStream_->index;

        ret = av_interleaved_write_frame(formatCtx_, packet_);
        if (ret < 0) {
            spdlog::error("RecordingEncoder: flush audio write_frame failed (err={})", ret);
        }
        av_packet_unref(packet_);
    }
}

// ---------------------------------------------------------------------------
// Resource cleanup
// ---------------------------------------------------------------------------

void RecordingEncoder::freeResources() {
    if (packet_) {
        av_packet_free(&packet_);
        packet_ = nullptr;
    }

    if (videoFrame_) {
        av_frame_free(&videoFrame_);
        videoFrame_ = nullptr;
    }

    if (audioFrame_) {
        av_frame_free(&audioFrame_);
        audioFrame_ = nullptr;
    }

    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }

    if (swrCtx_) {
        swr_free(&swrCtx_);
        swrCtx_ = nullptr;
    }

    if (videoCodecCtx_) {
        avcodec_free_context(&videoCodecCtx_);
        videoCodecCtx_ = nullptr;
    }

    if (audioCodecCtx_) {
        avcodec_free_context(&audioCodecCtx_);
        audioCodecCtx_ = nullptr;
    }

    if (formatCtx_) {
        if (formatCtx_->pb && !(formatCtx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&formatCtx_->pb);
        }
        avformat_free_context(formatCtx_);
        formatCtx_ = nullptr;
    }

    // Reset stream pointers (owned by formatCtx_, already freed)
    videoStream_ = nullptr;
    audioStream_ = nullptr;
}

} // namespace openscreen
