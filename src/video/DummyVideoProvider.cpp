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

#include "DummyVideoProvider.h"
#include <QStringList>
#include <cmath>

DummyVideoProvider::DummyVideoProvider(QObject *parent)
    : VideoProvider(parent)
{
}

bool DummyVideoProvider::open(const QString &source)
{
    // Format: ?dummy:fps:frames:width:height:color
    if (!source.startsWith(QLatin1String("?dummy:"))) {
        return false;
    }

    QStringList parts = source.mid(7).split(QLatin1Char(':'));
    if (parts.size() < 5) {
        return false;
    }

    double fps = parts[0].toDouble();
    int frames = parts[1].toInt();
    int width = parts[2].toInt();
    int height = parts[3].toInt();
    QString color = parts[4];

    return setup(fps, frames, width, height, color);
}

bool DummyVideoProvider::setup(double fps, int frameCount, int width, int height, const QString &colorHex)
{
    close();

    m_fps = fps > 0.0 ? fps : 23.976;
    m_totalFrames = frameCount > 0 ? frameCount : 40000;
    m_width = width > 0 ? width : 1920;
    m_height = height > 0 ? height : 1080;
    m_colorHex = colorHex.isEmpty() ? QStringLiteral("#000000") : colorHex;
    m_color = QColor(m_colorHex);
    if (!m_color.isValid()) {
        m_color = Qt::black;
        m_colorHex = QStringLiteral("#000000");
    }

    m_duration = static_cast<double>(m_totalFrames) / m_fps;
    m_sourceUri = QStringLiteral("?dummy:%1:%2:%3:%4:%5")
                      .arg(m_fps, 0, 'f', 6)
                      .arg(m_totalFrames)
                      .arg(m_width)
                      .arg(m_height)
                      .arg(m_colorHex);

    m_cachedFrame = QImage(m_width, m_height, QImage::Format_RGB32);
    m_cachedFrame.fill(m_color);

    m_loaded = true;
    Q_EMIT providerLoaded();
    return true;
}

void DummyVideoProvider::close()
{
    m_loaded = false;
    m_sourceUri.clear();
    m_cachedFrame = QImage();
    m_width = 0;
    m_height = 0;
    m_duration = 0.0;
    m_totalFrames = 0;
}

QVector<int64_t> DummyVideoProvider::getKeyframes() const
{
    QVector<int64_t> kf;
    if (m_totalFrames <= 0) return kf;

    kf.append(0);
    int step = static_cast<int>(5.0 * m_fps);
    if (step > 0) {
        for (int f = step; f < m_totalFrames; f += step) {
            kf.append(f);
        }
    }
    return kf;
}

QVector<double> DummyVideoProvider::getTimecodes() const
{
    QVector<double> tc;
    if (m_totalFrames <= 0 || m_fps <= 0.0) return tc;

    tc.reserve(m_totalFrames);
    for (int i = 0; i < m_totalFrames; ++i) {
        tc.append(static_cast<double>(i) / m_fps);
    }
    return tc;
}

QImage DummyVideoProvider::getFrame(int frameNumber, double timeSeconds)
{
    Q_UNUSED(frameNumber);
    Q_UNUSED(timeSeconds);
    return m_cachedFrame;
}

void DummyVideoProvider::requestFrameAsync(int frameNumber, double timeSeconds)
{
    // Dummy frames are synthesized instantly in memory without decoding lag.
    QImage frame = m_cachedFrame;
    Q_EMIT frameReady(frameNumber, frame, timeSeconds);
}

void DummyVideoProvider::extractKeyframesAsync()
{
    QVector<int64_t> kf = getKeyframes();
    QVector<double> ts;
    ts.reserve(kf.size());
    for (int64_t idx : kf) {
        ts.append(m_fps > 0.0 ? static_cast<double>(idx) / m_fps : 0.0);
    }
    Q_EMIT keyframesReady(kf, ts);
}

void DummyVideoProvider::extractTimecodesAsync()
{
    QVector<double> tc = getTimecodes();
    Q_EMIT timecodesReady(tc);
}
