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
#include <QThreadPool>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <cmath>
#include <utility>

struct AstraVideoProvider::WorkerState : std::enable_shared_from_this<WorkerState> {
    AstraCoreBridge *bridge = nullptr;
    QString path;
    int width = 0, height = 0;
    void *session = nullptr;
    std::mutex sessionMutex;
    std::atomic<bool> stopping{false};
    std::atomic<bool> scrubMode{false};
    std::atomic<uint64_t> latestRequest{0};
    std::mutex requestMutex;
    int pendingFrame = 0;
    double pendingTime = 0;
    bool hasPending = false, decodeInFlight = false;

    // The receiver is read only while holding this gate. close() clears it
    // before QObject destruction, so workers cannot enqueue into a dead object.
    // All provider access is confined to the queued callback on its GUI thread.
    std::mutex deliveryMutex;
    AstraVideoProvider *receiver = nullptr;

    ~WorkerState() {
        if (!session) return;
        auto *nativeBridge = bridge;
        void *nativeSession = session;
        // Even a session with no active decoder may take time to close. Its
        // final shared reference can be released on the GUI thread, so always
        // defer the native close. QCoreApplication drains the global pool on
        // shutdown before the process-static bridge unloads its library.
        if (auto *pool = QThreadPool::globalInstance())
            pool->start([nativeBridge, nativeSession] { nativeBridge->closeVideoSession(nativeSession); });
        else
            nativeBridge->closeVideoSession(nativeSession); // application already destroyed
    }

    template<class Callback> void post(Callback callback) {
        std::lock_guard<std::mutex> lock(deliveryMutex);
        if (!receiver || stopping.load()) return;
        auto *target = receiver;
        std::weak_ptr<WorkerState> generation = shared_from_this();
        QMetaObject::invokeMethod(target, [target, generation, callback = std::move(callback)]() mutable {
            auto state = generation.lock();
            if (!state || state->stopping.load() || target->m_state != state) return;
            callback(target, *state);
        }, Qt::QueuedConnection);
    }
};

AstraVideoProvider::AstraVideoProvider(QObject *parent) : VideoProvider(parent) {}
AstraVideoProvider::~AstraVideoProvider() { close(); }

bool AstraVideoProvider::open(const QString &source)
{
    close();
    auto *bridge = AstraCoreBridge::instance();
    if (!bridge || !bridge->isAvailable()) {
        Q_EMIT providerError(QStringLiteral("AstraCore native library is not available."));
        return false;
    }
    MediaInfo info;
    if (!bridge->probe(source, info) || !info.hasVideo) {
        Q_EMIT providerError(QStringLiteral("Failed to probe video stream from: ") + source);
        return false;
    }
    auto state = std::make_shared<WorkerState>();
    state->bridge = bridge;
    state->path = source;
    state->width = info.width;
    state->height = info.height;
    state->receiver = this;
    state->session = bridge->openVideoSession(source);
    m_state = std::move(state);
    m_sourcePath = source;
    m_width = info.width;
    m_height = info.height;
    m_fps = info.fps > 0.0 ? info.fps : 23.976;
    m_duration = info.duration;
    m_totalFrames = static_cast<int>(std::round(m_duration * m_fps));
    bridge->probeHdr(source, m_isHdr, m_bitDepth, m_colorPrimaries, m_colorTransfer, &m_colorSpace, &m_colorRange);
    m_loaded = true;
    Q_EMIT providerLoaded();
    return true;
}

void AstraVideoProvider::close()
{
    if (m_state) {
        m_state->stopping.store(true);
        {
            std::lock_guard<std::mutex> lock(m_state->deliveryMutex);
            m_state->receiver = nullptr;
        }
        // Active or queued tasks retain only this old generation's state.
        // No native call or worker completion is waited for by the GUI.
        m_state.reset();
    }
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_cachedKeyframes.clear();
        m_cachedTimecodes.clear();
    }
    m_loaded = false;
    m_sourcePath.clear();
    m_width = m_height = m_totalFrames = 0;
    m_duration = 0.0;
    m_fps = 23.976;
    m_isHdr = false;
    m_bitDepth = 8;
    m_colorPrimaries = m_colorTransfer = 0;
    m_colorSpace = -1;
    m_colorRange = 0;
}

QVector<int64_t> AstraVideoProvider::getKeyframes() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_cachedKeyframes;
}
QVector<double> AstraVideoProvider::getTimecodes() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_cachedTimecodes;
}

QImage AstraVideoProvider::getFrame(int frameNumber, double timeSeconds)
{
    Q_UNUSED(frameNumber);
    auto state = m_state;
    if (!m_loaded || !state) return {};
    double actual = 0;
    std::lock_guard<std::mutex> lock(state->sessionMutex);
    if (state->session)
        return state->bridge->grabSessionFrame(state->session, timeSeconds, state->width, state->height, &actual);
    return state->bridge->grabFrameImage(state->path, timeSeconds, state->width, state->height, &actual);
}

void AstraVideoProvider::requestFrameAsync(int frameNumber, double timeSeconds)
{
    auto state = m_state;
    if (!m_loaded || !state || state->stopping.load()) return;
    {
        std::lock_guard<std::mutex> lock(state->requestMutex);
        state->pendingFrame = frameNumber;
        state->pendingTime = timeSeconds;
        state->hasPending = true;
        ++state->latestRequest;
        if (state->decodeInFlight) return;
        state->decodeInFlight = true;
    }
    startDecodeLoop();
}

void AstraVideoProvider::setScrubMode(bool on)
{
    if (m_state) m_state->scrubMode.store(on, std::memory_order_relaxed);
}

void AstraVideoProvider::startDecodeLoop()
{
    auto state = m_state;
    if (!state) return;
    QThreadPool::globalInstance()->start([state] {
        // The worker accesses shared native/request state, never the provider.
        // Every open gets new coalescing flags, fixing all stopped-loop exits.
        for (;;) {
            int frameNumber;
            double timeSeconds;
            uint64_t requestId;
            {
                std::lock_guard<std::mutex> lock(state->requestMutex);
                if (state->stopping.load() || !state->hasPending) {
                    state->decodeInFlight = false;
                    return;
                }
                frameNumber = state->pendingFrame;
                timeSeconds = state->pendingTime;
                requestId = state->latestRequest.load(std::memory_order_relaxed);
                state->hasPending = false;
            }
            int width = state->width, height = state->height;
            if (state->scrubMode.load(std::memory_order_relaxed) && width > 480) {
                height = std::max(1, static_cast<int>(std::lround(double(height) * 480.0 / width)));
                width = 480;
            }
            double actualPts = 0;
            QImage frame;
            {
                std::lock_guard<std::mutex> lock(state->sessionMutex);
                if (state->session && !state->stopping.load())
                    frame = state->bridge->grabSessionFrame(state->session, timeSeconds, width, height, &actualPts);
            }
            if (frame.isNull() && !state->stopping.load())
                frame = state->bridge->grabFrameImage(state->path, timeSeconds, width, height, &actualPts);
            if (frame.isNull() || state->stopping.load()) continue;
            state->post([requestId, frameNumber, frame, actualPts](AstraVideoProvider *provider, WorkerState &generation) {
                // Delivery-time checking also handles a newer request at the
                // same frame number, including full-resolution scrub release.
                if (generation.latestRequest.load(std::memory_order_relaxed) == requestId)
                    Q_EMIT provider->frameReady(frameNumber, frame, actualPts);
            });
        }
    });
}

void AstraVideoProvider::extractKeyframesAsync()
{
    auto state = m_state;
    if (!m_loaded || !state || state->stopping.load()) return;
    QThreadPool::globalInstance()->start([state] {
        if (state->stopping.load()) return;
        QVector<int64_t> indices;
        auto timestamps = state->bridge->extractKeyframes(state->path, &indices);
        state->post([indices, timestamps](AstraVideoProvider *provider, WorkerState &) {
            {
                std::lock_guard<std::mutex> lock(provider->m_cacheMutex);
                provider->m_cachedKeyframes = indices;
            }
            Q_EMIT provider->keyframesReady(indices, timestamps);
        });
    });
}

void AstraVideoProvider::extractTimecodesAsync()
{
    auto state = m_state;
    if (!m_loaded || !state || state->stopping.load()) return;
    const int maxFrames = m_totalFrames > 0 ? m_totalFrames + 100 : 500000;
    QThreadPool::globalInstance()->start([state, maxFrames] {
        if (state->stopping.load()) return;
        auto timecodes = state->bridge->extractTimecodes(state->path, QString(), maxFrames);
        state->post([timecodes](AstraVideoProvider *provider, WorkerState &) {
            {
                std::lock_guard<std::mutex> lock(provider->m_cacheMutex);
                provider->m_cachedTimecodes = timecodes;
            }
            Q_EMIT provider->timecodesReady(timecodes);
        });
    });
}
