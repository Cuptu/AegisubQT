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

#include <QIODevice>
#include <QMutex>
#include <QMutexLocker>
#include <cstdint>
#include <cstring>
#include <algorithm>

/// Zero-copy read-only QIODevice that streams a slice of 16-bit PCM samples to QAudioSink.
class AudioSliceDevice : public QIODevice {
public:
    explicit AudioSliceDevice(const int16_t *data, qint64 sampleCount, QObject *parent = nullptr)
        : QIODevice(parent)
        , m_data(data)
        , m_totalBytes(std::max<qint64>(0, sampleCount) * static_cast<qint64>(sizeof(int16_t)))
        , m_pos(0)
    {
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }

    ~AudioSliceDevice() override {
        close();
    }

    qint64 readData(char *data, qint64 maxlen) override {
        QMutexLocker locker(&m_mutex);
        if (!m_data || m_pos >= m_totalBytes || maxlen <= 0) {
            return 0;
        }
        qint64 bytesToRead = std::min(maxlen, m_totalBytes - m_pos);
        std::memcpy(data, reinterpret_cast<const char*>(m_data) + m_pos, static_cast<size_t>(bytesToRead));
        m_pos += bytesToRead;
        return bytesToRead;
    }

    qint64 writeData(const char *, qint64) override {
        return -1; // Read-only streaming device.
    }

    qint64 bytesAvailable() const override {
        QMutexLocker locker(&m_mutex);
        return (m_totalBytes - m_pos);
    }

    bool isSequential() const override {
        return false;
    }

    qint64 size() const override {
        return m_totalBytes;
    }

    qint64 pos() const override {
        QMutexLocker locker(&m_mutex);
        return m_pos;
    }

    bool seek(qint64 pos) override {
        QMutexLocker locker(&m_mutex);
        if (pos < 0 || pos > m_totalBytes) {
            return false;
        }
        m_pos = pos;
        QIODevice::seek(pos);
        return true;
    }

    bool atEnd() const override {
        QMutexLocker locker(&m_mutex);
        return m_pos >= m_totalBytes;
    }

    bool reset() override {
        QMutexLocker locker(&m_mutex);
        m_pos = 0;
        QIODevice::reset();
        return true;
    }

private:
    const int16_t *m_data = nullptr;
    qint64 m_totalBytes = 0;
    qint64 m_pos = 0;
    mutable QMutex m_mutex;
};
