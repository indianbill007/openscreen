#include "render/VideoDecoder.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

namespace openscreen {

VideoDecoder::VideoDecoder() = default;

VideoDecoder::~VideoDecoder() {
    if (open_) {
        close();
    }
}

bool VideoDecoder::open(const std::string& filePath) {
    if (open_) {
        spdlog::warn("VideoDecoder: already open, closing previous file");
        close();
    }

    // Open input file
    int ret = avformat_open_input(&formatCtx_, filePath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errBuf, sizeof(errBuf));
        spdlog::error("VideoDecoder: failed to open '{}': {}", filePath, errBuf);
        return false;
    }

    // Read stream info
    ret = avformat_find_stream_info(formatCtx_, nullptr);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errBuf, sizeof(errBuf));
        spdlog::error("VideoDecoder: failed to find stream info: {}", errBuf);
        close();
        return false;
    }

    // Find best video stream
    const AVCodec* decoder = nullptr;
    videoStreamIndex_ = av_find_best_stream(
        formatCtx_, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (videoStreamIndex_ < 0) {
        spdlog::error("VideoDecoder: no video stream found in '{}'", filePath);
        close();
        return false;
    }

    if (decoder == nullptr) {
        spdlog::error("VideoDecoder: no suitable decoder found");
        close();
        return false;
    }

    // Allocate and configure codec context
    const AVStream* stream = formatCtx_->streams[videoStreamIndex_];
    codecCtx_ = avcodec_alloc_context3(decoder);
    if (codecCtx_ == nullptr) {
        spdlog::error("VideoDecoder: failed to allocate codec context");
        close();
        return false;
    }

    ret = avcodec_parameters_to_context(codecCtx_, stream->codecpar);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errBuf, sizeof(errBuf));
        spdlog::error("VideoDecoder: failed to copy codec params: {}", errBuf);
        close();
        return false;
    }

    ret = avcodec_open2(codecCtx_, decoder, nullptr);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errBuf, sizeof(errBuf));
        spdlog::error("VideoDecoder: failed to open codec: {}", errBuf);
        close();
        return false;
    }

    // Populate VideoInfo
    info_.width = codecCtx_->width;
    info_.height = codecCtx_->height;
    info_.codecName = decoder->name;

    if (stream->r_frame_rate.den > 0) {
        info_.fps = static_cast<double>(stream->r_frame_rate.num) /
                    static_cast<double>(stream->r_frame_rate.den);
    }

    if (formatCtx_->duration > 0) {
        info_.durationMs = formatCtx_->duration / (AV_TIME_BASE / 1000);
    } else if (stream->duration > 0) {
        info_.durationMs = ptsToMs(stream->duration);
    }

    if (stream->nb_frames > 0) {
        info_.totalFrames = stream->nb_frames;
    } else if (info_.fps > 0.0 && info_.durationMs > 0) {
        info_.totalFrames =
            static_cast<int64_t>(info_.fps * static_cast<double>(info_.durationMs) / 1000.0);
    }

    // Create scaler context: source format -> BGRA
    swsCtx_ = sws_getContext(
        codecCtx_->width, codecCtx_->height, codecCtx_->pix_fmt,
        codecCtx_->width, codecCtx_->height, AV_PIX_FMT_BGRA,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (swsCtx_ == nullptr) {
        spdlog::error("VideoDecoder: failed to create SwsContext");
        close();
        return false;
    }

    // Allocate frames
    frame_ = av_frame_alloc();
    bgraFrame_ = av_frame_alloc();
    if (frame_ == nullptr || bgraFrame_ == nullptr) {
        spdlog::error("VideoDecoder: failed to allocate frames");
        close();
        return false;
    }

    bgraFrame_->format = AV_PIX_FMT_BGRA;
    bgraFrame_->width = codecCtx_->width;
    bgraFrame_->height = codecCtx_->height;

    ret = av_image_alloc(
        bgraFrame_->data, bgraFrame_->linesize,
        codecCtx_->width, codecCtx_->height,
        AV_PIX_FMT_BGRA, 32);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errBuf, sizeof(errBuf));
        spdlog::error("VideoDecoder: failed to allocate BGRA buffer: {}", errBuf);
        close();
        return false;
    }

    // Allocate packet
    packet_ = av_packet_alloc();
    if (packet_ == nullptr) {
        spdlog::error("VideoDecoder: failed to allocate packet");
        close();
        return false;
    }

    open_ = true;
    currentPtsMs_ = 0;

    spdlog::info("VideoDecoder: opened '{}' ({}x{}, {:.2f} fps, {} ms, codec={})",
                 filePath, info_.width, info_.height, info_.fps,
                 info_.durationMs, info_.codecName);
    return true;
}

void VideoDecoder::close() {
    if (swsCtx_ != nullptr) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }

    if (bgraFrame_ != nullptr) {
        // Free the buffer allocated by av_image_alloc
        if (bgraFrame_->data[0] != nullptr) {
            av_freep(&bgraFrame_->data[0]);
        }
        av_frame_free(&bgraFrame_);
        bgraFrame_ = nullptr;
    }

    if (frame_ != nullptr) {
        av_frame_free(&frame_);
        frame_ = nullptr;
    }

    if (packet_ != nullptr) {
        av_packet_free(&packet_);
        packet_ = nullptr;
    }

    if (codecCtx_ != nullptr) {
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
    }

    if (formatCtx_ != nullptr) {
        avformat_close_input(&formatCtx_);
        formatCtx_ = nullptr;
    }

    videoStreamIndex_ = -1;
    info_ = {};
    currentPtsMs_ = 0;
    open_ = false;
}

bool VideoDecoder::isOpen() const {
    return open_;
}

VideoInfo VideoDecoder::info() const {
    return info_;
}

std::unique_ptr<DecodedFrame> VideoDecoder::decodeNextFrame() {
    if (!open_) {
        return nullptr;
    }

    while (true) {
        int ret = av_read_frame(formatCtx_, packet_);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                // Flush the decoder with a null packet
                ret = avcodec_send_packet(codecCtx_, nullptr);
                if (ret >= 0) {
                    ret = avcodec_receive_frame(codecCtx_, frame_);
                    if (ret >= 0) {
                        return convertFrame();
                    }
                }
                return nullptr;
            }
            char errBuf[AV_ERROR_MAX_STRING_SIZE]{};
            av_strerror(ret, errBuf, sizeof(errBuf));
            spdlog::error("VideoDecoder: av_read_frame failed: {}", errBuf);
            return nullptr;
        }

        // Skip non-video packets
        if (packet_->stream_index != videoStreamIndex_) {
            av_packet_unref(packet_);
            continue;
        }

        ret = avcodec_send_packet(codecCtx_, packet_);
        av_packet_unref(packet_);

        if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
                // Decoder full, try receiving before sending more
            } else if (ret == AVERROR_EOF) {
                return nullptr;
            } else {
                char errBuf[AV_ERROR_MAX_STRING_SIZE]{};
                av_strerror(ret, errBuf, sizeof(errBuf));
                spdlog::error("VideoDecoder: avcodec_send_packet failed: {}", errBuf);
                return nullptr;
            }
        }

        ret = avcodec_receive_frame(codecCtx_, frame_);
        if (ret == 0) {
            return convertFrame();
        }
        if (ret == AVERROR(EAGAIN)) {
            // Need more packets
            continue;
        }
        if (ret == AVERROR_EOF) {
            return nullptr;
        }

        char errBuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errBuf, sizeof(errBuf));
        spdlog::error("VideoDecoder: avcodec_receive_frame failed: {}", errBuf);
        return nullptr;
    }
}

bool VideoDecoder::seekTo(int64_t timestampMs) {
    if (!open_) {
        spdlog::warn("VideoDecoder: seekTo called on closed decoder");
        return false;
    }

    const int64_t ts = msToTs(timestampMs);
    const int ret = av_seek_frame(
        formatCtx_, videoStreamIndex_, ts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errBuf, sizeof(errBuf));
        spdlog::error("VideoDecoder: seek to {} ms failed: {}", timestampMs, errBuf);
        return false;
    }

    avcodec_flush_buffers(codecCtx_);
    return true;
}

std::unique_ptr<DecodedFrame> VideoDecoder::seekAndDecode(int64_t timestampMs) {
    if (!seekTo(timestampMs)) {
        return nullptr;
    }

    std::unique_ptr<DecodedFrame> result;
    while (true) {
        auto decoded = decodeNextFrame();
        if (decoded == nullptr) {
            // EOF reached before target time; return last decoded frame
            break;
        }
        result = std::move(decoded);
        if (result->timestampMs >= timestampMs) {
            break;
        }
    }
    return result;
}

int64_t VideoDecoder::currentPositionMs() const {
    return currentPtsMs_;
}

bool VideoDecoder::decodePacket(AVPacket* packet) {
    const int ret = avcodec_send_packet(codecCtx_, packet);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, errBuf, sizeof(errBuf));
        spdlog::error("VideoDecoder: avcodec_send_packet failed: {}", errBuf);
        return false;
    }
    return true;
}

std::unique_ptr<DecodedFrame> VideoDecoder::convertFrame() {
    // Scale/convert to BGRA
    sws_scale(
        swsCtx_,
        frame_->data, frame_->linesize,
        0, codecCtx_->height,
        bgraFrame_->data, bgraFrame_->linesize);

    const int stride = bgraFrame_->linesize[0];
    const int height = codecCtx_->height;
    const auto dataSize = static_cast<size_t>(stride * height);

    auto decoded = std::make_unique<DecodedFrame>();
    decoded->width = codecCtx_->width;
    decoded->height = height;
    decoded->stride = stride;

    // Copy pixel data into owned buffer
    decoded->data.resize(dataSize);
    std::memcpy(decoded->data.data(), bgraFrame_->data[0], dataSize);

    // Timestamp from best_effort_timestamp
    const int64_t pts = frame_->best_effort_timestamp;
    if (pts != AV_NOPTS_VALUE) {
        decoded->timestampMs = ptsToMs(pts);
    } else {
        decoded->timestampMs = currentPtsMs_;
    }

    // Duration
    const int64_t dur = frame_->duration;
    if (dur > 0) {
        decoded->durationMs = ptsToMs(dur);
    } else if (info_.fps > 0.0) {
        decoded->durationMs = static_cast<int64_t>(1000.0 / info_.fps);
    }

    currentPtsMs_ = decoded->timestampMs;
    return decoded;
}

int64_t VideoDecoder::ptsToMs(int64_t pts) const {
    if (videoStreamIndex_ < 0 || formatCtx_ == nullptr) {
        return 0;
    }
    const AVRational tb = formatCtx_->streams[videoStreamIndex_]->time_base;
    return pts * tb.num * 1000 / tb.den;
}

int64_t VideoDecoder::msToTs(int64_t ms) const {
    if (videoStreamIndex_ < 0 || formatCtx_ == nullptr) {
        return 0;
    }
    const AVRational tb = formatCtx_->streams[videoStreamIndex_]->time_base;
    if (tb.num == 0) {
        return 0;
    }
    return ms * tb.den / (tb.num * 1000);
}

}  // namespace openscreen
