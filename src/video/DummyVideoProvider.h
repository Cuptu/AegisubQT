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
#include <QColor>

/// Procedural dummy video provider generating solid-color frames and regular keyframe grids
/// without requiring actual multimedia files or codec overhead.
class DummyVideoProvider : public VideoProvider {
    Q_OBJECT

public:
    explicit DummyVideoProvider(QObject *parent = nullptr);
    ~DummyVideoProvider() override = default;

    bool open(const QString &source) override;
    void close() override;
    bool isLoaded() const override { return m_loaded; }
    bool isDummy() const override { return true; }
    QString sourcePath() const override { return m_sourceUri; }

    int width() const override { return m_width; }
    int height() const override { return m_height; }
    double fps() const override { return m_fps; }
    double duration() const override { return m_duration; }
    int totalFrames() const override { return m_totalFrames; }

    QString dummyColor() const { return m_colorHex; }

    QVector<int64_t> getKeyframes() const override;
    QVector<double> getTimecodes() const override;

    QImage getFrame(int frameNumber, double timeSeconds) override;
    void requestFrameAsync(int frameNumber, double timeSeconds) override;
    void extractKeyframesAsync() override;
    void extractTimecodesAsync() override;

    bool setup(double fps, int frameCount, int width, int height, const QString &colorHex);

private:
    bool m_loaded = false;
    QString m_sourceUri;
    QString m_colorHex = QStringLiteral("#000000");
    QColor m_color = Qt::black;

    int m_width = 1920;
    int m_height = 1080;
    double m_fps = 23.976;
    double m_duration = 0.0;
    int m_totalFrames = 0;

    QImage m_cachedFrame;
};
