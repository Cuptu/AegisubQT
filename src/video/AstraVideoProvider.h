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

#include "VideoProvider.h"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <QThreadPool>

/// Hardware-accelerated and zero-copy video provider backed by AstraCore Native C ABI.
/// Offloads frame decoding, packet demuxing, and keyframe scanning to a worker thread pool,
/// preventing frame scrubbing and heavy I/O from stalling the Qt Quick GUI thread.
class AstraVideoProvider : public VideoProvider {
    Q_OBJECT

public:
    explicit AstraVideoProvider(QObject *parent = nullptr);
    ~AstraVideoProvider() override;

    bool open(const QString &source) override;
    void close() override;
    bool isLoaded() const override { return m_loaded; }
    QString sourcePath() const override { return m_sourcePath; }

    int width() const override { return m_width; }
    int height() const override { return m_height; }
    double fps() const override { return m_fps; }
    double duration() const override { return m_duration; }
    int totalFrames() const override { return m_totalFrames; }

    bool isHdr() const override { return m_isHdr; }
    int bitDepth() const override { return m_bitDepth; }
    int colorPrimaries() const override { return m_colorPrimaries; }
    int colorTransfer() const override { return m_colorTransfer; }

    QVector<int64_t> getKeyframes() const override;
    QVector<double> getTimecodes() const override;

    QImage getFrame(int frameNumber, double timeSeconds) override;
    void requestFrameAsync(int frameNumber, double timeSeconds) override;
    void extractKeyframesAsync() override;
    void extractTimecodesAsync() override;

private:
    QString m_sourcePath;
    bool m_loaded = false;

    int m_width = 0;
    int m_height = 0;
    double m_fps = 23.976;
    double m_duration = 0.0;
    int m_totalFrames = 0;

    bool m_isHdr = false;
    int m_bitDepth = 8;
    int m_colorPrimaries = 0;
    int m_colorTransfer = 0;

    mutable std::mutex m_cacheMutex;
    QVector<int64_t> m_cachedKeyframes;
    QVector<double> m_cachedTimecodes;

    // Sequence token used to drop obsolete frame decode jobs during rapid scrub operations.
    std::atomic<uint64_t> m_latestFrameRequestId{0};

    // Persistent native decoding session for low-latency scrubbing.
    void *m_videoSession = nullptr;
    mutable std::mutex m_sessionMutex;

    // Frame request coalescing (for scrubbing): ensures at most one decode job is in-flight,
    // retaining only the newest target frame to prevent thread pool starvation and stale backlogs.
    void startDecodeLoop();
    void setScrubMode(bool on) override;
    std::mutex m_requestMutex;
    int m_pendingFrame = 0;
    double m_pendingTime = 0.0;
    bool m_hasPending = false;
    bool m_decodeInFlight = false;
    // Decodes downscaled proxy frames during active scrubbing (max width 480); restored on release.
    std::atomic<bool> m_scrubMode{false};

    struct WorkerSync {
        std::mutex mutex;
        std::condition_variable cv;
        int activeWorkers = 0;
        std::atomic<bool> stopping{false};
    };
    std::shared_ptr<WorkerSync> m_sync = std::make_shared<WorkerSync>();
};
