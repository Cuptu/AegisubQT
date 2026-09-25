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

#include <QImage>
#include <vector>
#include <memory>
#include <cstdint>
#include <mutex>
#include "AudioPcmProvider.h"

class AegisubStftCore {
public:
    AegisubStftCore();
    ~AegisubStftCore() = default;

    // FFT analysis parameters, matching Aegisub's default spectrum quality
    // (config: Audio/Renderer/Spectrum/Quality = 1 -> width 9, distance 8):
    // N = 2^derivationSize frequency bins (512 bins, 1024-point FFT)
    // Hop = 2^derivationDist samples (256 samples = 16 ms @16 kHz)
    size_t derivationSize = 9;
    size_t derivationDist = 8;
    float maxFreq = 20000.0f;
    float freqRef = 1000.0f;
    float posFref = 0.001f;

    /// Prepares STFT audio buffers and initializes spectral parameters from PCM provider.
    bool processAudio(const AudioPcmProvider &provider);

    /// Maximum horizontal dimension (in frames) allocated for the STFT cache texture.
    /// Spans exceeding this limit are downsampled via windowFrameStep to cover any zoom level
    /// without viewport clipping or GPU memory exhaustion.
    static constexpr int MAX_TEXTURE_FRAMES = 4096;

    int windowStartFrame() const { return m_windowStartFrame; }
    int windowFrameCount() const { return m_windowFrameCount; }
    int windowFrameStep() const { return m_windowFrameStep; }

    /// Adjusts the sliding window to cover the requested viewport span.
    /// @return True if the texture buffer was recomputed and requires GPU re-upload.
    bool ensureWindow(const AudioPcmProvider &provider, int centerFrame, int spanFrames);

    /// Monotonically increasing revision counter to guard against redundant GPU uploads.
    quint64 revision() const { return m_revision; }

    const QImage& stftTexture() const { return m_stftImage; }
    const QImage& paletteTexture() const { return m_paletteImage; }

    int totalFrames() const { return m_totalFrames; }
    int nbrBins() const { return 1 << derivationSize; }
    int sampleRate() const { return m_sampleRate; }
    int hopSamples() const { return 1 << derivationDist; }

private:
    void computeWindow(const AudioPcmProvider &provider, int startFrame, int count, int step);
    void generatePaletteTexture();

    // In-place Cooley-Tukey radix-2 FFT implementation.
    void fftTransform(size_t n_samples, const float *input, float *output_r, float *output_i);
    bool isPowerOfTwo(unsigned int x);
    unsigned int numberOfBitsNeeded(unsigned int n_samples);
    unsigned int reverseBits(unsigned int index, unsigned int bits);

    int m_totalFrames = 0;
    int m_sampleRate = 16000;
    int m_windowStartFrame = 0;
    int m_windowFrameCount = 0;
    int m_windowFrameStep = 1;
    quint64 m_revision = 0;
    QImage m_stftImage;
    QImage m_paletteImage;

    // Cached scanline pointers to eliminate repeated QImage::scanLine overhead.
    std::vector<uint32_t*> m_scanlines;

    std::vector<float> m_fftInput;
    std::vector<float> m_fftReal;
    std::vector<float> m_fftImag;
    mutable std::mutex m_mutex;
};
