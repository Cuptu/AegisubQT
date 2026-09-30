// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT
#pragma once

#include "AegisubStftCore.h"
#include <QThreadPool>
#include <functional>
#include <mutex>

// One worker-owned FFT cache, one replaceable pending request, immutable output.
// Neither submitting a viewport nor reading its image waits for an FFT.
class AsyncSpectrumWindow : public std::enable_shared_from_this<AsyncSpectrumWindow> {
public:
    struct Request {
        AudioPcmProvider provider; // Shared immutable PCM, survives source replacement.
        quint64 source = 0;
        size_t derivationSize = 9, derivationDist = 8;
        int center = 0, span = 0;
    };
    struct Result {
        QImage image;
        quint64 source = 0, revision = 0, imageRevision = 0;
        int start = 0, count = 0, step = 1, totalFrames = 0;
        quintptr workerThread = 0;
        qint64 computeNanoseconds = 0;
        bool recomputed = false;
    };

    explicit AsyncSpectrumWindow(std::function<void()> ready, QThreadPool *pool = QThreadPool::globalInstance());
    void request(Request request);
    std::shared_ptr<const Result> result() const;
    quint64 activeRevision() const;
    void invalidate();
    void stop();

private:
    void run();
    static bool same(const Request &a, const Request &b);
    mutable std::mutex m_mutex;
    Request m_latest;
    std::shared_ptr<const Result> m_result;
    quint64 m_serial = 0;
    quint64 m_activeSerial = 0;
    bool m_hasRequest = false, m_pending = false, m_running = false, m_stopped = false;
    std::function<void()> m_ready;
    QThreadPool *m_pool;
    // Only touched by the single active worker, retained between runnable invocations.
    std::unique_ptr<AegisubStftCore> m_core;
    quint64 m_coreSource = 0;
    bool m_initialized = false;
};
