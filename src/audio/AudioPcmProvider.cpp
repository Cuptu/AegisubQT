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

#include "AudioPcmProvider.h"
#include "AstraCoreBridge.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QDataStream>
#include <QDebug>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDateTime>
#include <algorithm>
#include <cstring>

namespace {
// Maximum number of cached audio files preserved on disk before LRU eviction.
constexpr int kAudioCacheMaxEntries = 8;

void evictAudioCacheLru(const QDir &cacheDir)
{
    QFileInfoList entries = cacheDir.entryInfoList({QStringLiteral("*.wav")}, QDir::Files, QDir::Time);
    while (entries.size() > kAudioCacheMaxEntries) {
        QFile::remove(entries.takeLast().absoluteFilePath());
    }
}
} // namespace

bool AudioPcmProvider::loadAudioFile(const QString &filePath, int targetSampleRate)
{
    if (filePath.endsWith(".wav", Qt::CaseInsensitive) && loadWav(filePath)) {
        return true;
    }

    QFileInfo fi(filePath);
    // Locate standard platform cache directory (Windows: %LOCALAPPDATA%, macOS: ~/Library/Caches, Linux: ~/.cache).
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir cacheDir(cacheRoot + QStringLiteral("/audio"));
    cacheDir.mkpath(QStringLiteral("."));

    // Cache key derived from source file fingerprint (canonical path, size, mtime) and target sample rate.
    const QString rateTag = targetSampleRate > 0 ? QString::number(targetSampleRate) : QStringLiteral("native");
    const QString fingerprint = fi.absoluteFilePath() + QLatin1Char('|')
        + QString::number(fi.size()) + QLatin1Char('|')
        + QString::number(fi.lastModified().toMSecsSinceEpoch()) + QLatin1Char('|') + rateTag;
    const QString cacheKey = QString::fromLatin1(
        QCryptographicHash::hash(fingerprint.toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
    QString targetWav = cacheDir.filePath(cacheKey + QStringLiteral(".wav"));

    if (!QFile::exists(targetWav)) {
        bool ok = false;
        if (targetSampleRate > 0) {
            ok = AstraCoreBridge::instance()->extractAudioWav(filePath, targetWav, targetSampleRate, 1);
        }
        if (!ok || !QFile::exists(targetWav)) {
            qInfo() << "[AudioPcmProvider] Extracting audio with ffmpeg from" << filePath << "to" << targetWav;
            QStringList args = {"-y", "-i", filePath, "-vn", "-ac", "1"};
            if (targetSampleRate > 0) {
                args << "-ar" << QString::number(targetSampleRate);
            }
            args << targetWav;
            int res = QProcess::execute("ffmpeg", args);
            if (res != 0 || !QFile::exists(targetWav)) {
                qWarning() << "[AudioPcmProvider] audio extraction failed, code:" << res;
                return false;
            }
        }
        evictAudioCacheLru(cacheDir);
    }

    return loadWav(targetWav);
}

bool AudioPcmProvider::loadWav(const QString &filePath)
{
    auto file = std::make_unique<QFile>(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        qWarning() << "AudioPcmProvider: failed to open file:" << filePath;
        return false;
    }

    const qint64 fileSize = file->size();
    if (fileSize < 44) {
        qWarning() << "AudioPcmProvider: file too small for WAV header:" << filePath;
        return false;
    }

    // Map entire WAV into virtual memory to avoid copying hundreds of megabytes into RAM heap.
    // The operating system page cache transparently demand-pages active frames into memory.
    uchar *raw = file->map(0, fileSize);
    if (!raw) {
        qWarning() << "AudioPcmProvider: failed to memory-map WAV file:" << filePath;
        return false;
    }

    if (std::memcmp(raw, "RIFF", 4) != 0 || std::memcmp(raw + 8, "WAVE", 4) != 0) {
        file->unmap(raw);
        qWarning() << "AudioPcmProvider: invalid RIFF/WAVE header:" << filePath;
        return false;
    }

    int audioFormat = 1;
    int channels = 1;
    int sampleRate = 16000;
    int bitsPerSample = 16;
    size_t pcmOffset = 0;
    uint32_t pcmBytes = 0;

    // Walk RIFF chunks to locate 'fmt ' and 'data'.
    size_t offset = 12;
    while (offset + 8 <= static_cast<size_t>(fileSize)) {
        char chunkId[5] = {0};
        std::memcpy(chunkId, raw + offset, 4);
        uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, raw + offset + 4, 4);
        offset += 8;

        if (std::strcmp(chunkId, "fmt ") == 0 && chunkSize >= 16 && offset + 16 <= static_cast<size_t>(fileSize)) {
            uint16_t fmt = 0, ch = 0, bits = 0;
            uint32_t sr = 0;
            std::memcpy(&fmt, raw + offset, 2);
            std::memcpy(&ch, raw + offset + 2, 2);
            std::memcpy(&sr, raw + offset + 4, 4);
            std::memcpy(&bits, raw + offset + 14, 2);
            // WAVE_FORMAT_EXTENSIBLE (0xFFFE): Actual encoding format is encoded in the first 2 bytes
            // of SubFormat GUID (after cbSize(2) + validBits(2) + channelMask(4) at offset 24 of fmt payload).
            // 1 = PCM, 3 = IEEE float.
            if (fmt == 0xFFFE && chunkSize >= 40 && offset + 26 <= static_cast<size_t>(fileSize)) {
                uint16_t subFmt = 0;
                std::memcpy(&subFmt, raw + offset + 24, 2);
                if (subFmt != 0) {
                    fmt = subFmt;
                }
            }
            audioFormat = fmt;
            channels = ch;
            sampleRate = sr;
            bitsPerSample = bits;
        } else if (std::strcmp(chunkId, "data") == 0) {
            pcmOffset = offset;
            pcmBytes = std::min<uint32_t>(chunkSize, static_cast<uint32_t>(fileSize - offset));
            break;
        }
        const uint64_t step = static_cast<uint64_t>(chunkSize) + (chunkSize & 1);
        if (offset + step < offset || offset + step > static_cast<size_t>(fileSize)) {
            break;
        }
        offset += step;
    }

    if (pcmOffset == 0 || pcmBytes == 0 || channels <= 0) {
        file->unmap(raw);
        qWarning() << "AudioPcmProvider: missing or empty data chunk in WAV:" << filePath;
        return false;
    }

    auto data = std::make_shared<AudioData>();
    data->sampleRate = sampleRate;

    // Fast path: 16-bit mono PCM (Aegisub default extraction).
    // Word-aligned chunk payload mapped directly with zero copies.
    if (bitsPerSample == 16 && channels == 1 && (audioFormat == 1 || audioFormat == 0xFFFE)) {
        data->file = std::move(file);
        data->mappedBytes = raw;
        data->samples = reinterpret_cast<const int16_t*>(raw + pcmOffset);
        data->sampleCount = pcmBytes / sizeof(int16_t);
        data->channels = 1;
        m_data = std::move(data);

        qInfo() << "AudioPcmProvider: zero-copy memory-mapped WAV:" << filePath
                << "rate:" << m_data->sampleRate << "channels:" << m_data->channels
                << "samples:" << m_data->sampleCount << "duration:" << duration() << "s";
        return true;
    }

    // Fallback path: multi-channel downmix or bit-depth conversion.
    size_t bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample == 0) {
        file->unmap(raw);
        qWarning() << "AudioPcmProvider: invalid bits per sample:" << bitsPerSample;
        return false;
    }

    size_t totalFrames = pcmBytes / (bytesPerSample * channels);
    data->fallbackBuffer.resize(totalFrames);
    const char *pcmData = reinterpret_cast<const char*>(raw + pcmOffset);

    if (bitsPerSample == 16 && (audioFormat == 1 || audioFormat == 0xFFFE)) {
        const int16_t *src = reinterpret_cast<const int16_t*>(pcmData);
        for (size_t i = 0; i < totalFrames; ++i) {
            int sum = 0;
            for (int c = 0; c < channels; ++c) {
                sum += *src++;
            }
            data->fallbackBuffer[i] = static_cast<int16_t>(sum / channels);
        }
    } else if (bitsPerSample == 32 && audioFormat == 3) {
        const float *src = reinterpret_cast<const float*>(pcmData);
        for (size_t i = 0; i < totalFrames; ++i) {
            float sum = 0.0f;
            for (int c = 0; c < channels; ++c) {
                sum += *src++;
            }
            sum /= channels;
            sum = std::clamp(sum, -1.0f, 1.0f);
            data->fallbackBuffer[i] = static_cast<int16_t>(sum * 32767.0f);
        }
    } else if (bitsPerSample == 24 && audioFormat == 1) {
        // 24-bit PCM: sign-extend 3-byte little-endian samples and take upper 16 bits.
        const char *src = pcmData;
        for (size_t i = 0; i < totalFrames; ++i) {
            int sum = 0;
            for (int c = 0; c < channels; ++c) {
                int32_t v = static_cast<uint8_t>(src[0])
                          | (static_cast<uint8_t>(src[1]) << 8)
                          | (static_cast<uint8_t>(src[2]) << 16);
                if (v & 0x800000) v |= ~0xFFFFFF;
                src += 3;
                sum += v >> 8;
            }
            data->fallbackBuffer[i] = static_cast<int16_t>(std::clamp(sum / channels, -32768, 32767));
        }
    } else if (bitsPerSample == 8) {
        const uint8_t *src = reinterpret_cast<const uint8_t*>(pcmData);
        for (size_t i = 0; i < totalFrames; ++i) {
            int sum = 0;
            for (int c = 0; c < channels; ++c) {
                sum += (static_cast<int>(*src++) - 128) * 256;
            }
            data->fallbackBuffer[i] = static_cast<int16_t>(sum / channels);
        }
    } else {
        file->unmap(raw);
        qWarning() << "AudioPcmProvider: unsupported WAV format" << audioFormat << "bits:" << bitsPerSample;
        return false;
    }

    // Release memory mapping since converted audio now lives in fallbackBuffer.
    file->unmap(raw);
    file->close();

    data->samples = data->fallbackBuffer.data();
    data->sampleCount = data->fallbackBuffer.size();
    data->channels = 1;
    m_data = std::move(data);

    qInfo() << "AudioPcmProvider: converted WAV to fallback buffer:" << filePath
            << "rate:" << m_data->sampleRate << "channels:" << m_data->channels
            << "samples:" << m_data->sampleCount << "duration:" << duration() << "s";
    return true;
}

bool AudioPcmProvider::loadVirtualAudio(VirtualKind kind, double durationSec, int sampleRate)
{
    if (kind == VirtualKind::None) return false;

    auto data = std::make_shared<AudioData>();
    data->isVirtual = true;
    data->virtualKind = static_cast<int>(kind);
    data->sampleRate = sampleRate > 0 ? sampleRate : 44100;
    data->channels = 1;

    const double dur = durationSec > 0.0 ? durationSec : 9000.0;
    const int64_t count = static_cast<int64_t>(dur * static_cast<double>(data->sampleRate));
    if (count <= 0) return false;

    data->sampleCount = static_cast<size_t>(count);
    m_data = std::move(data);
    return true;
}

void AudioPcmProvider::getAudio(float *dest, int64_t startSample, size_t count) const
{
    constexpr float kScale = 1.0f / 32768.0f;
    const int64_t total = numSamples();

    if (isVirtual()) {
        // On-demand synthesis: blank audio is silence; noise audio is a deterministic
        // xorshift white-noise stream seeded from the sample offset.
        const bool noise = (virtualKind() == VirtualKind::Noise);
        for (size_t i = 0; i < count; ++i) {
            const int64_t idx = startSample + static_cast<int64_t>(i);
            if (noise && idx >= 0 && idx < total) {
                uint32_t x = static_cast<uint32_t>(idx) * 2654435761u + 0x9E3779B9u;
                x ^= x >> 15;
                x *= 0x85EBCA6Bu;
                x ^= x >> 13;
                dest[i] = (static_cast<float>(static_cast<int16_t>(x)) * kScale) * 0.8f;
            } else {
                dest[i] = 0.0f;
            }
        }
        return;
    }

    const auto s = samples();
    for (size_t i = 0; i < count; ++i) {
        const int64_t idx = startSample + static_cast<int64_t>(i);
        if (idx >= 0 && idx < total && !s.empty()) {
            dest[i] = static_cast<float>(s[idx]) * kScale;
        } else {
            dest[i] = 0.0f;
        }
    }
}
