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

#include <QObject>
#include <QString>
#include <QImage>
#include <QVector>
#include <cstdint>

/// Abstract base provider for video decoding, metadata inspection, and frame extraction.
/// Mirrors Aegisub's classic VideoProvider interface while decoupling GUI event loops
/// via asynchronous task delegation and Qt signal-slot dispatch.
class VideoProvider : public QObject {
    Q_OBJECT

public:
    explicit VideoProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~VideoProvider() override = default;

    virtual bool open(const QString &source) = 0;
    virtual void close() = 0;
    virtual bool isLoaded() const = 0;
    virtual bool isDummy() const { return false; }
    virtual QString sourcePath() const = 0;

    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual double fps() const = 0;
    virtual double duration() const = 0;
    virtual int totalFrames() const = 0;

    virtual bool isHdr() const { return false; }
    virtual int bitDepth() const { return 8; }
    virtual int colorPrimaries() const { return 0; }
    virtual int colorTransfer() const { return 0; }

    virtual QVector<int64_t> getKeyframes() const = 0;
    virtual QVector<double> getTimecodes() const = 0;

    /// Synchronous frame extraction. Callers must be mindful of potential decoding latency.
    virtual QImage getFrame(int frameNumber, double timeSeconds) = 0;

    /// Non-blocking frame dispatch offloaded to background thread pool.
    /// Emits frameReady() on the main Qt thread upon completion.
    virtual void requestFrameAsync(int frameNumber, double timeSeconds) = 0;

    /// Switches to low-resolution decoding mode during timeline scrubbing to minimize per-frame
    /// latency and improve interactive responsiveness. Default implementation is a no-op.
    virtual void setScrubMode(bool on) { Q_UNUSED(on); }

    /// Asynchronous keyframe extraction off the GUI thread.
    virtual void extractKeyframesAsync() = 0;

    /// Asynchronous VFR timecode extraction off the GUI thread.
    virtual void extractTimecodesAsync() = 0;

Q_SIGNALS:
    void providerLoaded();
    void providerError(const QString &errorMessage);
    void keyframesReady(const QVector<int64_t> &indices, const QVector<double> &timestamps);
    void timecodesReady(const QVector<double> &timecodes);
    void frameReady(int frameNumber, const QImage &image, double actualPts);
};
