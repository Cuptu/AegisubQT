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

#include "AegisubStftCore.h"
#include <QColor>
#include <QDebug>
#include <QElapsedTimer>
#include <cmath>
#include <algorithm>

AegisubStftCore::AegisubStftCore()
{
    generatePaletteTexture();
}

static inline uint8_t clip_colorval(int val) {
    return static_cast<uint8_t>(std::clamp(val, 0, 255));
}

static void aegisub_hsl_to_rgb(int H, int S, int L, uint8_t *R, uint8_t *G, uint8_t *B)
{
    if (S == 0) {
        *R = L;
        *G = L;
        *B = L;
        return;
    }

    float h = H / 255.f;
    float s = S / 255.f;
    float l = L / 255.f;

    float temp2 = (l < 0.5f) ? (l * (1.f + s)) : (l + s - l * s);
    float temp1 = 2.f * l - temp2;

    auto calcChannel = [temp1, temp2](float tc) {
        if (tc > 1.f) tc -= 1.f;
        if (tc < 0.f) tc += 1.f;
        if (6.f * tc < 1.f) return temp1 + (temp2 - temp1) * 6.f * tc;
        if (2.f * tc < 1.f) return temp2;
        if (3.f * tc < 2.f) return temp1 + (temp2 - temp1) * ((2.f / 3.f) - tc) * 6.f;
        return temp1;
    };

    *R = clip_colorval(static_cast<int>(calcChannel(h + 1.f / 3.f) * 255.f));
    *G = clip_colorval(static_cast<int>(calcChannel(h) * 255.f));
    *B = clip_colorval(static_cast<int>(calcChannel(h - 1.f / 3.f) * 255.f));
}

void AegisubStftCore::generatePaletteTexture()
{
    // 12-bit precision color lookup table: factor = 1 << 12 = 4096 entries + 1 guard point.
    // Normalized sample magnitude t in [0.0, 1.0] maps to index t * 4096.
    constexpr int kFactor = 4096;

    // Rows correspond to AudioRenderingStyle: Normal, Inactive, Selected, Primary.
    // Coefficients define linear interpolation endpoints for H, S, L channels ("Icy Blue").
    struct SchemeRow { double h0, hs, s0, ss, l0, ls; };
    static const SchemeRow icyBlue[4] = {
        {191.0, -128.0, 127.0, 128.0,  0.0, 255.0},  // 0 Normal
        {191.0, -128.0,  63.0, 192.0, 32.0, 192.0},  // 1 Inactive
        {191.0, -128.0, 127.0, 128.0, 32.0, 192.0},  // 2 Selected
        {191.0, -128.0, 127.0, 128.0, 32.0, 223.0},  // 3 Primary
    };

    m_paletteImage = QImage(kFactor + 1, 4, QImage::Format_RGBA8888);

    for (int style = 0; style < 4; ++style) {
        const SchemeRow &s = icyBlue[style];
        uint32_t *line = reinterpret_cast<uint32_t*>(m_paletteImage.scanLine(style));
        for (int i = 0; i <= kFactor; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(kFactor);
            const int h = std::clamp(static_cast<int>(s.h0 + t * s.hs), 0, 255);
            const int sat = std::clamp(static_cast<int>(s.s0 + t * s.ss), 0, 255);
            const int l = std::clamp(static_cast<int>(s.l0 + t * s.ls), 0, 255);

            uint8_t r, g, b;
            aegisub_hsl_to_rgb(h, sat, l, &r, &g, &b);
            line[i] = (255u << 24) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(g) << 8) | r;
        }
    }
}

bool AegisubStftCore::processAudio(const AudioPcmProvider &provider)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!provider.isLoaded() || provider.numSamples() <= 0) {
        return false;
    }

    m_sampleRate = provider.sampleRate();
    const size_t n_fft = 2 << derivationSize; // 1024
    const size_t hop = 1 << derivationDist;   // 256 (matches derivationDist invariant in AegisubStftCore.h)

    const int64_t totalSamples = provider.numSamples();
    m_totalFrames = static_cast<int>((totalSamples + static_cast<int64_t>(hop) - 1) / static_cast<int64_t>(hop));
    if (m_totalFrames <= 0) return false;

    m_fftInput.resize(n_fft);
    m_fftReal.resize(n_fft);
    m_fftImag.resize(n_fft);

    // Dynamic sliding window populated on demand based on viewport span.
    m_windowStartFrame = 0;
    m_windowFrameCount = 0;
    m_windowFrameStep = 1;
    m_scanlines.clear();
    m_stftImage = QImage();

    qInfo() << "AegisubStftCore: STFT ready," << m_totalFrames << "total frames @"
            << m_sampleRate << "Hz, hop" << hop << "samples (window built on demand)";
    return true;
}

void AegisubStftCore::computeWindow(const AudioPcmProvider &provider, int startFrame, int count, int step)
{
    const size_t n_fft = 2 << derivationSize;
    const size_t n_bins = 1 << derivationSize;

    if (m_stftImage.width() != count || m_stftImage.height() != static_cast<int>(n_bins)) {
        m_stftImage = QImage(count, static_cast<int>(n_bins), QImage::Format_RGBA8888);
    }
    m_stftImage.fill(0);

    // Cache scanline addresses upfront to avoid per-bin pointer calculation in the hot loop.
    m_scanlines.resize(n_bins);
    for (size_t b = 0; b < n_bins; ++b) {
        m_scanlines[b] = reinterpret_cast<uint32_t*>(m_stftImage.scanLine(static_cast<int>(b)));
    }

    const float scale_factor = 9.0f / std::sqrt(2.0f * static_cast<float>(n_fft));
    std::vector<float> framePcm(n_fft);

    for (int i = 0; i < count; ++i) {
        const int64_t frame = static_cast<int64_t>(startFrame) + static_cast<int64_t>(i) * step;
        if (frame >= m_totalFrames) break;

        const int64_t first_sample = (frame << derivationDist) - (static_cast<int64_t>(1) << derivationSize);
        provider.getAudio(framePcm.data(), first_sample, n_fft);

        fftTransform(n_fft, framePcm.data(), m_fftReal.data(), m_fftImag.data());

        for (size_t bin = 0; bin < n_bins; ++bin) {
            const float re = m_fftReal[bin];
            const float im = m_fftImag[bin];
            const float mag = std::sqrt(re * re + im * im);
            float power = std::log10(mag * scale_factor + 1.0f);
            power = std::clamp(power, 0.0f, 1.0f);

            const uint8_t byteVal = static_cast<uint8_t>(power * 255.0f);
            m_scanlines[bin][i] = (255u << 24) | (static_cast<uint32_t>(byteVal) << 16)
                                | (static_cast<uint32_t>(byteVal) << 8) | byteVal;
        }
    }

    m_windowStartFrame = startFrame;
    m_windowFrameCount = count;
    m_windowFrameStep = step;
    ++m_revision;
}

bool AegisubStftCore::ensureWindow(const AudioPcmProvider &provider, int centerFrame, int spanFrames)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_totalFrames <= 0) return false;

    // Viewport margin: allocate double the visible span (50% headroom on either side)
    // to avoid thrashing during interactive scrolling.
    const int64_t minSpan = std::min<int64_t>(m_totalFrames, 1024);
    int64_t span = std::max<int64_t>(static_cast<int64_t>(std::max(spanFrames, 1)) * 2, minSpan);
    span = std::min<int64_t>(span, m_totalFrames);

    int step = 1;
    if (span > MAX_TEXTURE_FRAMES) {
        step = static_cast<int>((span + MAX_TEXTURE_FRAMES - 1) / MAX_TEXTURE_FRAMES);
    }
    const int count = static_cast<int>(std::max<int64_t>(1, (span + step - 1) / step));

    int64_t start = static_cast<int64_t>(centerFrame) - span / 2;
    start = std::max<int64_t>(0, start);
    start = std::min<int64_t>(start, std::max<int64_t>(0, static_cast<int64_t>(m_totalFrames) - span));
    start -= start % step; // Align to step boundary to avoid thrashing on sub-step shifts.

    const bool sameGeometry = (count == m_windowFrameCount && step == m_windowFrameStep && m_windowFrameCount > 0);
    if (sameGeometry) {
        // Reuse cached window if viewport center remains within the middle 50% margin.
        const int64_t quarter = span / 4;
        const int64_t rel = static_cast<int64_t>(centerFrame) - m_windowStartFrame;
        if (rel >= quarter && rel <= span - quarter) return false;
        if (start == m_windowStartFrame) return false;
    }

    QElapsedTimer timer;
    timer.start();
    computeWindow(provider, static_cast<int>(start), count, step);
    const qint64 elapsed = timer.elapsed();

    qInfo() << "AegisubStftCore: built window start" << start << "frames" << count << "step" << step
            << "in" << elapsed << "ms (" << (count > 0 ? elapsed * 1000.0 / count : 0.0) << "us/col )";
    return true;
}

// In-place Cooley-Tukey radix-2 decimation-in-time FFT.
void AegisubStftCore::fftTransform(size_t n_samples, const float *input, float *output_r, float *output_i)
{
    if (!isPowerOfTwo(static_cast<unsigned int>(n_samples))) return;

    float angle_num = 2.0f * 3.14159265358979323846f;
    unsigned int NumBits = numberOfBitsNeeded(static_cast<unsigned int>(n_samples));

    for (size_t i = 0; i < n_samples; ++i) {
        unsigned int j = reverseBits(static_cast<unsigned int>(i), NumBits);
        output_r[j] = input[i];
        output_i[j] = 0.0f;
    }

    unsigned int BlockEnd = 1;
    for (unsigned int BlockSize = 2; BlockSize <= n_samples; BlockSize <<= 1) {
        float delta_angle = angle_num / static_cast<float>(BlockSize);
        float sm2 = std::sin(-2.0f * delta_angle);
        float sm1 = std::sin(-delta_angle);
        float cm2 = std::cos(-2.0f * delta_angle);
        float cm1 = std::cos(-delta_angle);
        float w = 2.0f * cm1;

        for (size_t i = 0; i < n_samples; i += BlockSize) {
            float ar1 = cm1, ar2 = cm2;
            float ai1 = sm1, ai2 = sm2;

            for (size_t j = i, n = 0; n < BlockEnd; ++j, ++n) {
                size_t k = j + BlockEnd;
                float ar0 = w * ar1 - ar2;
                float ai0 = w * ai1 - ai2;
                ar2 = ar1; ai2 = ai1;
                ar1 = ar0; ai1 = ai0;

                float tr = ar0 * output_r[k] - ai0 * output_i[k];
                float ti = ar0 * output_i[k] + ai0 * output_r[k];

                output_r[k] = output_r[j] - tr;
                output_i[k] = output_i[j] - ti;
                output_r[j] += tr;
                output_i[j] += ti;
            }
        }
        BlockEnd = BlockSize;
    }
}

bool AegisubStftCore::isPowerOfTwo(unsigned int x)
{
    return (x >= 2) && ((x & (x - 1)) == 0);
}

unsigned int AegisubStftCore::numberOfBitsNeeded(unsigned int n_samples)
{
    if (n_samples < 2) return 0;
    for (int i = 0;; ++i) {
        if (n_samples & (1 << i)) return i;
    }
}

unsigned int AegisubStftCore::reverseBits(unsigned int index, unsigned int bits)
{
    unsigned int rev = 0;
    for (unsigned int i = 0; i < bits; ++i) {
        rev = (rev << 1) | (index & 1);
        index >>= 1;
    }
    return rev;
}
