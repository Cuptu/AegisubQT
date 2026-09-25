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

#include "AstraVideoProvider.h"
#include "AstraCoreBridge.h"
#include <QRunnable>
#include <QPointer>
#include <QDebug>
#include <cmath>

AstraVideoProvider::AstraVideoProvider(QObject *parent)
    : VideoProvider(parent)
{
}

AstraVideoProvider::~AstraVideoProvider()
{
    close();
}

bool AstraVideoProvider::open(const QString &source)
{
    close();
    m_sync = std::make_shared<WorkerSync>();
    m_sourcePath = source;

    AstraCoreBridge *bridge = AstraCoreBridge::instance();
    if (!bridge || !bridge->isAvailable()) {
        Q_EMIT providerError(QStringLiteral("AstraCore native library is not available."));
        return false;
    }

    MediaInfo info;
    if (!bridge->probe(m_sourcePath, info) || !info.hasVideo) {
        Q_EMIT providerError(QStringLiteral("Failed to probe video stream from: ") + m_sourcePath);
        return false;
    }

    m_width = info.width;
    m_height = info.height;
    m_fps = info.fps > 0.0 ? info.fps : 23.976;
    m_duration = info.duration;
    m_totalFrames = static_cast<int>(std::round(m_duration * m_fps));

    // Probe HDR10 / PQ / HLG color properties
    bridge->probeHdr(m_sourcePath, m_isHdr, m_bitDepth, m_colorPrimaries, m_colorTransfer);

    // Open persistent decoding session for fast sequential and scrubbing playback
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        m_videoSession = bridge->openVideoSession(m_sourcePath);
    }

    m_loaded = true;
    Q_EMIT providerLoaded();
    return true;
}

void AstraVideoProvider::close()
{
    m_sync->stopping.store(true);
    // Invalidate inflight decoding tasks by incrementing the sequence token.
    m_latestFrameRequestId.fetch_add(1, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(m_requestMutex);
        m_hasPending = false;
    }

    // Wait for in-flight workers to exit safely before invalidating the session
    {
        std::unique_lock<std::mutex> lk(m_sync->mutex);
        m_sync->cv.wait(lk, [this]() {
            return m_sync->activeWorkers == 0;
        });
    }

    void *sessionToClose = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        sessionToClose = m_videoSession;
        m_videoSession = nullptr;
    }
    if (sessionToClose) {
        AstraCoreBridge::instance()->closeVideoSession(sessionToClose);
    }

    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_cachedKeyframes.clear();
        m_cachedTimecodes.clear();
    }

    m_loaded = false;
    m_sourcePath.clear();
    m_width = 0;
    m_height = 0;
    m_duration = 0.0;
    m_totalFrames = 0;
}

QVector<int64_t> AstraVideoProvider::getKeyframes() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_cachedKeyframes;
}

QVector<double> AstraVideoProvider::getTimecodes() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_cachedTimecodes;
}

QImage AstraVideoProvider::getFrame(int frameNumber, double timeSeconds)
{
    Q_UNUSED(frameNumber);
    if (!m_loaded) return QImage();

    double actualSec = 0.0;
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    if (m_videoSession) {
        return AstraCoreBridge::instance()->grabSessionFrame(m_videoSession, timeSeconds, m_width, m_height, &actualSec);
    }
    return AstraCoreBridge::instance()->grabFrameImage(m_sourcePath, timeSeconds, m_width, m_height, &actualSec);
}

void AstraVideoProvider::requestFrameAsync(int frameNumber, double timeSeconds)
{
    if (!m_loaded) return;

    // Coalesce requests: at most one decode job in flight, keeping only the latest target frame.
    {
        std::lock_guard<std::mutex> lock(m_requestMutex);
        m_pendingFrame = frameNumber;
        m_pendingTime = timeSeconds;
        m_hasPending = true;
        ++m_latestFrameRequestId;
        if (m_decodeInFlight) return;   // Decode active; worker will pick up newest frame on loop completion.
        m_decodeInFlight = true;
    }

    startDecodeLoop();
}

void AstraVideoProvider::setScrubMode(bool on)
{
    m_scrubMode.store(on, std::memory_order_relaxed);
}

void AstraVideoProvider::startDecodeLoop()
{
    if (m_sync->stopping.load()) return;
    const QString path = m_sourcePath;
    const int targetWidth = m_width;
    const int targetHeight = m_height;
    QPointer<AstraVideoProvider> self(this);
    auto sync = m_sync;

    {
        std::lock_guard<std::mutex> lk(sync->mutex);
        sync->activeWorkers++;
    }

    QThreadPool::globalInstance()->start(QRunnable::create([self, sync, path, targetWidth, targetHeight]() {
        struct WorkerGuard {
            std::shared_ptr<WorkerSync> s;
            ~WorkerGuard() {
                if (s) {
                    std::lock_guard<std::mutex> lk(s->mutex);
                    s->activeWorkers--;
                    s->cv.notify_all();
                }
            }
        } guard{sync};

        // Loop until all pending requests are drained: converges to latest frame during active scrubbing.
        while (self && !sync->stopping.load()) {
            int frameNumber = 0;
            double timeSeconds = 0.0;
            {
                std::lock_guard<std::mutex> lock(self->m_requestMutex);
                if (!self->m_hasPending || sync->stopping.load()) {
                    self->m_decodeInFlight = false;
                    return;
                }
                frameNumber = self->m_pendingFrame;
                timeSeconds = self->m_pendingTime;
                self->m_hasPending = false;
            }

            if (sync->stopping.load()) return;

            double actualPts = 0.0;
            QImage frame;
            // Downscale frame during scrubbing: clamp width to 480 maintaining aspect ratio for faster decode.
            int targetW = targetWidth;
            int targetH = targetHeight;
            if (self->m_scrubMode.load(std::memory_order_relaxed) && targetWidth > 480) {
                targetW = 480;
                targetH = std::max(1, static_cast<int>(std::lround(double(targetHeight) * 480.0 / double(targetWidth))));
            }
            {
                std::lock_guard<std::mutex> lock(self->m_sessionMutex);
                if (self->m_videoSession && !sync->stopping.load()) {
                    frame = AstraCoreBridge::instance()->grabSessionFrame(
                        self->m_videoSession, timeSeconds, targetW, targetH, &actualPts);
                }
            }
            if (frame.isNull() && !sync->stopping.load()) {
                frame = AstraCoreBridge::instance()->grabFrameImage(
                    path, timeSeconds, targetW, targetH, &actualPts);
            }

            // Dispatch decoded frame: provides continuous visual feedback during scrubbing.
            // Stale frames are filtered upstream by frameNumber sequence tokens.
            if (frame.isNull() || sync->stopping.load()) continue;

            QMetaObject::invokeMethod(self, [self, sync, frameNumber, frame, actualPts]() {
                if (self && !sync->stopping.load()) {
                    Q_EMIT self->frameReady(frameNumber, frame, actualPts);
                }
            }, Qt::QueuedConnection);
        }
    }));
}

void AstraVideoProvider::extractKeyframesAsync()
{
    if (!m_loaded || m_sync->stopping.load()) return;

    const QString path = m_sourcePath;
    QPointer<AstraVideoProvider> self(this);
    auto sync = m_sync;

    {
        std::lock_guard<std::mutex> lk(sync->mutex);
        sync->activeWorkers++;
    }

    QThreadPool::globalInstance()->start(QRunnable::create([self, sync, path]() {
        struct WorkerGuard {
            std::shared_ptr<WorkerSync> s;
            ~WorkerGuard() {
                if (s) {
                    std::lock_guard<std::mutex> lk(s->mutex);
                    s->activeWorkers--;
                    s->cv.notify_all();
                }
            }
        } guard{sync};

        if (!self || sync->stopping.load()) return;

        QVector<int64_t> frameIndices;
        QVector<double> timestamps = AstraCoreBridge::instance()->extractKeyframes(path, &frameIndices);

        if (!self || sync->stopping.load()) return;

        {
            std::lock_guard<std::mutex> lock(self->m_cacheMutex);
            self->m_cachedKeyframes = frameIndices;
        }

        QMetaObject::invokeMethod(self, [self, sync, frameIndices, timestamps]() {
            if (self && !sync->stopping.load()) {
                Q_EMIT self->keyframesReady(frameIndices, timestamps);
            }
        }, Qt::QueuedConnection);
    }));
}

void AstraVideoProvider::extractTimecodesAsync()
{
    if (!m_loaded || m_sync->stopping.load()) return;

    const QString path = m_sourcePath;
    const int maxFrames = m_totalFrames > 0 ? m_totalFrames + 100 : 500000;
    QPointer<AstraVideoProvider> self(this);
    auto sync = m_sync;

    {
        std::lock_guard<std::mutex> lk(sync->mutex);
        sync->activeWorkers++;
    }

    QThreadPool::globalInstance()->start(QRunnable::create([self, sync, path, maxFrames]() {
        struct WorkerGuard {
            std::shared_ptr<WorkerSync> s;
            ~WorkerGuard() {
                if (s) {
                    std::lock_guard<std::mutex> lk(s->mutex);
                    s->activeWorkers--;
                    s->cv.notify_all();
                }
            }
        } guard{sync};

        if (!self || sync->stopping.load()) return;

        QVector<double> timecodes = AstraCoreBridge::instance()->extractTimecodes(path, QString(), maxFrames);

        if (!self || sync->stopping.load()) return;

        {
            std::lock_guard<std::mutex> lock(self->m_cacheMutex);
            self->m_cachedTimecodes = timecodes;
        }

        QMetaObject::invokeMethod(self, [self, sync, timecodes]() {
            if (self && !sync->stopping.load()) {
                Q_EMIT self->timecodesReady(timecodes);
            }
        }, Qt::QueuedConnection);
    }));
}
