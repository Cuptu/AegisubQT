// Copyright (c) 2005 - 2026, Aegisub Project & Contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

#pragma once

#include <QString>
#include <QFile>
#include <vector>
#include <cstdint>
#include <cmath>
#include <span>
#include <memory>

class AudioPcmProvider {
public:
    // Upstream "Open 2h30 Blank/Noise Audio": virtual on-demand synthesis modes.
    // Samples are never materialized; getAudio() synthesizes and samples() stays empty.
    enum class VirtualKind {
        None = 0,
        Blank = 1,
        Noise = 2,
    };

    struct AudioData {
        std::unique_ptr<QFile> file;
        uchar *mappedBytes = nullptr;
        const int16_t *samples = nullptr;
        size_t sampleCount = 0;
        int sampleRate = 16000;
        int channels = 1;
        std::vector<int16_t> fallbackBuffer;
        bool isVirtual = false;
        int virtualKind = 0;

        ~AudioData() {
            if (file) {
                if (mappedBytes) {
                    file->unmap(mappedBytes);
                    mappedBytes = nullptr;
                }
                if (file->isOpen()) {
                    file->close();
                }
            }
        }
    };

    AudioPcmProvider() = default;
    ~AudioPcmProvider() = default;
    AudioPcmProvider(AudioPcmProvider &&) noexcept = default;
    AudioPcmProvider &operator=(AudioPcmProvider &&) noexcept = default;
    AudioPcmProvider(const AudioPcmProvider &) = default;
    AudioPcmProvider &operator=(const AudioPcmProvider &) = default;

    // targetSampleRate = 0 preserves the native stream sampling rate.
    bool loadAudioFile(const QString &filePath, int targetSampleRate = 0);
    bool loadWav(const QString &filePath);
    // Configures virtual synthesis audio (upstream blank/noise 2h30 providers).
    bool loadVirtualAudio(VirtualKind kind, double durationSec = 9000.0, int sampleRate = 44100);
    void reset() { m_data.reset(); }

    bool isLoaded() const { return m_data && m_data->sampleCount > 0; }
    bool isVirtual() const { return m_data && m_data->isVirtual; }
    VirtualKind virtualKind() const {
        return m_data && m_data->isVirtual ? static_cast<VirtualKind>(m_data->virtualKind)
                                           : VirtualKind::None;
    }
    int sampleRate() const { return m_data ? m_data->sampleRate : 0; }
    int numChannels() const { return m_data ? m_data->channels : 1; }
    int64_t numSamples() const { return m_data ? static_cast<int64_t>(m_data->sampleCount) : 0; }
    double duration() const {
        return (m_data && m_data->sampleRate > 0)
            ? static_cast<double>(m_data->sampleCount) / m_data->sampleRate
            : 0.0;
    }

    // Zero-copy span over 16-bit signed PCM samples.
    // Points directly into memory-mapped WAV cache on disk, avoiding hundreds
    // of megabytes of uncompressed PCM heap allocation.
    // Virtual audio synthesizes on demand in getAudio(); its span stays empty.
    std::span<const int16_t> samples() const {
        if (!m_data || m_data->isVirtual || !m_data->samples) {
            return std::span<const int16_t>();
        }
        return std::span<const int16_t>(m_data->samples, m_data->sampleCount);
    }

    // Fills dest with count samples normalized to [-1.0, 1.0], zero-padding out-of-range indices.
    void getAudio(float *dest, int64_t startSample, size_t count) const;

private:
    std::shared_ptr<AudioData> m_data;
};
