// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT
#include "AsyncSpectrumWindow.h"
#include <QElapsedTimer>
#include <QRunnable>
#include <QThread>
#include <algorithm>

AsyncSpectrumWindow::AsyncSpectrumWindow(std::function<void()> ready, QThreadPool *pool)
    : m_ready(std::move(ready)), m_pool(pool) {}

bool AsyncSpectrumWindow::same(const Request &a, const Request &b)
{
    return a.source == b.source && a.derivationSize == b.derivationSize &&
        a.derivationDist == b.derivationDist && a.center == b.center && a.span == b.span;
}

void AsyncSpectrumWindow::request(Request request)
{
    bool launch = false;
    {
        std::lock_guard lock(m_mutex);
        if (m_stopped || (m_hasRequest && same(request, m_latest))) return;
        bool covered = false;
        if (m_result && m_result->source == request.source && m_latest.derivationSize == request.derivationSize &&
            m_latest.derivationDist == request.derivationDist && m_latest.span == request.span) {
            const int64_t halfSpan = (int64_t(std::max(1, request.span)) + 1) / 2;
            const auto first = std::clamp<int64_t>(int64_t(request.center) - halfSpan, 0, m_result->totalFrames);
            const auto end = std::clamp<int64_t>(int64_t(request.center) + halfSpan, 0, m_result->totalFrames);
            covered = m_result->start <= first && int64_t(m_result->start) + int64_t(m_result->count) * m_result->step >= end;
        }
        m_latest = std::move(request);
        ++m_serial;
        m_hasRequest = m_pending = true;
        // A valid image covering this viewport remains visible while the worker recenters.
        // An unrelated window is hidden immediately, even when its FFT is still in flight.
        if (!covered) m_result.reset();
        if (!m_running) m_running = launch = true;
    }
    if (launch) m_pool->start(QRunnable::create([self = shared_from_this()] { self->run(); }));
}

std::shared_ptr<const AsyncSpectrumWindow::Result> AsyncSpectrumWindow::result() const
{
    std::lock_guard lock(m_mutex);
    return m_result;
}

quint64 AsyncSpectrumWindow::activeRevision() const
{
    std::lock_guard lock(m_mutex);
    return m_activeSerial;
}

void AsyncSpectrumWindow::invalidate()
{
    std::lock_guard lock(m_mutex);
    ++m_serial;
    m_hasRequest = m_pending = false;
    m_latest = {};
    m_result.reset();
}

void AsyncSpectrumWindow::stop()
{
    std::lock_guard lock(m_mutex);
    m_stopped = true;
    ++m_serial;
    m_hasRequest = m_pending = false;
    m_latest = {};
    m_result.reset();
}

void AsyncSpectrumWindow::run()
{
    if (!m_core) m_core = std::make_unique<AegisubStftCore>();
    auto &core = *m_core; // Used exclusively by the active worker, never locked by the renderer.
    for (;;) {
        Request request;
        quint64 serial;
        {
            std::lock_guard lock(m_mutex);
            if (m_stopped || !m_pending) { m_running = false; m_activeSerial = 0; return; }
            request = m_latest;
            serial = m_serial;
            m_activeSerial = serial;
            m_pending = false;
        }
        QElapsedTimer timer;
        timer.start();
        if (!m_initialized || m_coreSource != request.source || core.derivationSize != request.derivationSize ||
            core.derivationDist != request.derivationDist) {
            core.derivationSize = request.derivationSize;
            core.derivationDist = request.derivationDist;
            m_initialized = core.processAudio(request.provider);
            m_coreSource = request.source;
        }
        const bool recomputed = m_initialized && core.ensureWindow(request.provider, request.center, request.span);
        auto output = std::make_shared<Result>();
        if (m_initialized) output->image = core.stftTexture(); // QImage detaches on the worker's next write.
        output->source = m_coreSource;
        output->revision = serial;
        output->imageRevision = core.revision();
        output->start = core.windowStartFrame();
        output->count = core.windowFrameCount();
        output->step = core.windowFrameStep();
        output->totalFrames = core.totalFrames();
        output->workerThread = reinterpret_cast<quintptr>(QThread::currentThreadId());
        output->computeNanoseconds = timer.nsecsElapsed();
        output->recomputed = recomputed;
        bool accepted = false;
        {
            std::lock_guard lock(m_mutex);
            if (!m_stopped && m_hasRequest && m_serial == serial) {
                m_result = std::move(output);
                accepted = true;
            }
        }
        if (accepted && m_ready) m_ready(); // Caller posts notification to its GUI context.
    }
}
