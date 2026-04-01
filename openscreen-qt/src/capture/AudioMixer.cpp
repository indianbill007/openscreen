#include "capture/AudioMixer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace openscreen {

AudioMixer::AudioMixer(int sampleRate, int channels)
    : sampleRate_(sampleRate)
    , channels_(channels)
{
}

void AudioMixer::setOutputCallback(MixedAudioCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    outputCallback_ = std::move(callback);
}

void AudioMixer::pushSystemAudio(const float* data, int frameCount, int channels, int sampleRate)
{
    if (data == nullptr || frameCount <= 0 || channels <= 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    const int totalInputSamples = frameCount * channels;

    // Convert to output channel count with gain applied
    if (channels == channels_) {
        // Channel count matches — apply gain and append directly
        const auto prevSize = systemBuffer_.size();
        systemBuffer_.resize(prevSize + static_cast<size_t>(totalInputSamples));
        for (int i = 0; i < totalInputSamples; ++i) {
            systemBuffer_[prevSize + static_cast<size_t>(i)] = data[i] * systemGain_;
        }
    } else if (channels == 1 && channels_ == 2) {
        // Mono to stereo: duplicate each sample
        const auto prevSize = systemBuffer_.size();
        systemBuffer_.resize(prevSize + static_cast<size_t>(frameCount * 2));
        for (int i = 0; i < frameCount; ++i) {
            const float sample = data[i] * systemGain_;
            systemBuffer_[prevSize + static_cast<size_t>(i * 2)] = sample;
            systemBuffer_[prevSize + static_cast<size_t>(i * 2 + 1)] = sample;
        }
    } else {
        // Fallback: take first channel (or duplicate) to fill output channels
        const auto prevSize = systemBuffer_.size();
        systemBuffer_.resize(prevSize + static_cast<size_t>(frameCount * channels_));
        for (int f = 0; f < frameCount; ++f) {
            const float sample = data[f * channels] * systemGain_;
            for (int c = 0; c < channels_; ++c) {
                systemBuffer_[prevSize + static_cast<size_t>(f * channels_ + c)] = sample;
            }
        }
    }

    mixAndOutput();
}

void AudioMixer::pushMicAudio(const float* data, int frameCount, int channels, int sampleRate)
{
    if (data == nullptr || frameCount <= 0 || channels <= 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    const int totalInputSamples = frameCount * channels;

    // Convert to output channel count with gain applied
    if (channels == channels_) {
        const auto prevSize = micBuffer_.size();
        micBuffer_.resize(prevSize + static_cast<size_t>(totalInputSamples));
        for (int i = 0; i < totalInputSamples; ++i) {
            micBuffer_[prevSize + static_cast<size_t>(i)] = data[i] * micGain_;
        }
    } else if (channels == 1 && channels_ == 2) {
        const auto prevSize = micBuffer_.size();
        micBuffer_.resize(prevSize + static_cast<size_t>(frameCount * 2));
        for (int i = 0; i < frameCount; ++i) {
            const float sample = data[i] * micGain_;
            micBuffer_[prevSize + static_cast<size_t>(i * 2)] = sample;
            micBuffer_[prevSize + static_cast<size_t>(i * 2 + 1)] = sample;
        }
    } else {
        const auto prevSize = micBuffer_.size();
        micBuffer_.resize(prevSize + static_cast<size_t>(frameCount * channels_));
        for (int f = 0; f < frameCount; ++f) {
            const float sample = data[f * channels] * micGain_;
            for (int c = 0; c < channels_; ++c) {
                micBuffer_[prevSize + static_cast<size_t>(f * channels_ + c)] = sample;
            }
        }
    }

    mixAndOutput();
}

void AudioMixer::setSystemGain(float gain)
{
    std::lock_guard<std::mutex> lock(mutex_);
    systemGain_ = gain;
}

void AudioMixer::setMicGain(float gain)
{
    std::lock_guard<std::mutex> lock(mutex_);
    micGain_ = gain;
}

void AudioMixer::mixAndOutput()
{
    // Caller must hold mutex_
    if (!outputCallback_) {
        return;
    }

    const auto chunkSamples = static_cast<size_t>(kMixChunkFrames * channels_);

    // Mix while both buffers have enough data
    while (systemBuffer_.size() >= chunkSamples && micBuffer_.size() >= chunkSamples) {
        mixBuffer_.resize(chunkSamples);

        for (size_t i = 0; i < chunkSamples; ++i) {
            const float mixed = systemBuffer_[i] + micBuffer_[i];
            mixBuffer_[i] = std::clamp(mixed, -1.0f, 1.0f);
        }

        outputCallback_(mixBuffer_.data(), kMixChunkFrames, channels_, sampleRate_);

        systemBuffer_.erase(systemBuffer_.begin(),
                            systemBuffer_.begin() + static_cast<ptrdiff_t>(chunkSamples));
        micBuffer_.erase(micBuffer_.begin(),
                         micBuffer_.begin() + static_cast<ptrdiff_t>(chunkSamples));
    }

    // If only one buffer has enough, output it alone (don't wait for the other)
    while (systemBuffer_.size() >= chunkSamples && micBuffer_.empty()) {
        outputCallback_(systemBuffer_.data(), kMixChunkFrames, channels_, sampleRate_);
        systemBuffer_.erase(systemBuffer_.begin(),
                            systemBuffer_.begin() + static_cast<ptrdiff_t>(chunkSamples));
    }

    while (micBuffer_.size() >= chunkSamples && systemBuffer_.empty()) {
        outputCallback_(micBuffer_.data(), kMixChunkFrames, channels_, sampleRate_);
        micBuffer_.erase(micBuffer_.begin(),
                         micBuffer_.begin() + static_cast<ptrdiff_t>(chunkSamples));
    }
}

void AudioMixer::flush()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!outputCallback_) {
        return;
    }

    // Output any remaining mixed content
    const auto systemSamples = systemBuffer_.size();
    const auto micSamples = micBuffer_.size();
    const auto commonSamples = std::min(systemSamples, micSamples);

    if (commonSamples > 0) {
        mixBuffer_.resize(commonSamples);
        for (size_t i = 0; i < commonSamples; ++i) {
            const float mixed = systemBuffer_[i] + micBuffer_[i];
            mixBuffer_[i] = std::clamp(mixed, -1.0f, 1.0f);
        }

        const int frames = static_cast<int>(commonSamples) / channels_;
        if (frames > 0) {
            outputCallback_(mixBuffer_.data(), frames, channels_, sampleRate_);
        }

        systemBuffer_.erase(systemBuffer_.begin(),
                            systemBuffer_.begin() + static_cast<ptrdiff_t>(commonSamples));
        micBuffer_.erase(micBuffer_.begin(),
                         micBuffer_.begin() + static_cast<ptrdiff_t>(commonSamples));
    }

    // Output any remaining tail from either buffer
    if (!systemBuffer_.empty()) {
        const int frames = static_cast<int>(systemBuffer_.size()) / channels_;
        if (frames > 0) {
            outputCallback_(systemBuffer_.data(), frames, channels_, sampleRate_);
        }
        systemBuffer_.clear();
    }

    if (!micBuffer_.empty()) {
        const int frames = static_cast<int>(micBuffer_.size()) / channels_;
        if (frames > 0) {
            outputCallback_(micBuffer_.data(), frames, channels_, sampleRate_);
        }
        micBuffer_.clear();
    }
}

void AudioMixer::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    systemBuffer_.clear();
    micBuffer_.clear();
    mixBuffer_.clear();
}

} // namespace openscreen
