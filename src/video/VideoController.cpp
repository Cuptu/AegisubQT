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

#include "VideoController.h"
#include "AstraVideoProvider.h"
#include "DummyVideoProvider.h"
#include "VideoFrameImageProvider.h"
#include "AstraCoreBridge.h"
#include <cmath>
#include <algorithm>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QFile>
#include <QUrl>
#include <QProcess>
#include <QCoreApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QPainter>
#include <QStandardPaths>
#include <QRegularExpression>

VideoController::VideoController(QObject *parent)
    : QObject(parent)
{
    // Keyframe markers are populated strictly from keyframe tables or explicit index files.
    m_keyframes.clear();

    m_timer.setInterval(static_cast<int>(1000.0 / m_fps));
    connect(&m_timer, &QTimer::timeout, this, &VideoController::onPlaybackTick);
}

QString VideoController::timeAndFrameString() const
{
    return QString("%1 - %2").arg(formatTime(m_currentTime)).arg(m_currentFrame);
}

QString VideoController::relativeTimeString() const
{
    int currMs = static_cast<int>(std::round(m_currentTime * 1000.0));
    int diffStart = currMs - m_activeSubStart;
    int diffEnd = currMs - m_activeSubEnd;
    QString signStart = diffStart >= 0 ? "+" : "";
    QString signEnd = diffEnd >= 0 ? "+" : "";
    return QString("%1%2ms; %3%4ms").arg(signStart).arg(diffStart).arg(signEnd).arg(diffEnd);
}

void VideoController::setZoom(const QString &z)
{
    if (m_zoom != z) {
        m_zoom = z;
        Q_EMIT zoomChanged();
        Q_EMIT videoInfoChanged();
    }
}

void VideoController::setAutoScroll(bool val)
{
    if (m_autoScroll != val) {
        m_autoScroll = val;
        Q_EMIT videoInfoChanged();
    }
}

void VideoController::setActiveSubtitle(int startMs, int endMs, const QString &text)
{
    m_activeSubStart = startMs;
    m_activeSubEnd = endMs;
    m_activeSubText = text;
    Q_EMIT positionChanged();
    Q_EMIT subtitleSyncChanged();
}

void VideoController::parseAndSetActiveSubtitle(const QString &startStr, const QString &endStr, const QString &text)
{
    int startMs = parseAssTime(startStr);
    int endMs = parseAssTime(endStr);
    setActiveSubtitle(startMs, endMs, text);
}

void VideoController::play()
{
    m_playLineMode = false;
    if (!m_isPlaying) {
        m_isPlaying = true;
        if (m_fps > 0) {
            m_timer.setInterval(static_cast<int>(1000.0 / m_fps));
        }
        m_timer.start();
        Q_EMIT playbackStateChanged();
    }
}

void VideoController::pause()
{
    if (m_isPlaying) {
        m_isPlaying = false;
        m_timer.stop();
        m_playLineMode = false;
        Q_EMIT playbackStateChanged();
    }
}

void VideoController::togglePlay()
{
    if (m_isPlaying) {
        pause();
    } else {
        play();
    }
}

void VideoController::playCurrentLine()
{
    seekTime(m_activeSubStart / 1000.0);
    m_playLineMode = true;
    m_playLineEndTime = m_activeSubEnd / 1000.0;
    m_isPlaying = true;
    if (m_isDummy) {
        m_timer.start();
    }
    Q_EMIT playbackStateChanged();
}

double VideoController::frameToTime(int frame) const
{
    if (!m_timecodes.isEmpty()) {
        int clamped = std::clamp(frame, 0, static_cast<int>(m_timecodes.size()) - 1);
        return m_timecodes[clamped];
    }
    return m_fps > 0 ? (frame / m_fps) : 0.0;
}

int VideoController::timeToFrame(double sec) const
{
    if (std::isnan(sec) || sec <= 0.0) return 0;
    if (!m_timecodes.isEmpty()) {
        const int size = static_cast<int>(m_timecodes.size());
        if (sec <= m_timecodes.first()) return 0;
        if (sec >= m_timecodes.last()) return size - 1;
        auto it = std::lower_bound(m_timecodes.begin(), m_timecodes.end(), sec);
        int idx = static_cast<int>(std::distance(m_timecodes.begin(), it));
        if (idx >= size) return size - 1;
        if (idx > 0 && (sec - m_timecodes[idx - 1]) < (m_timecodes[idx] - sec)) {
            return idx - 1;
        }
        return idx;
    }
    return m_fps > 0 ? static_cast<int>(std::round(sec * m_fps)) : 0;
}

void VideoController::seekFrame(int frame)
{
    frame = std::max(0, std::min(m_totalFrames - 1, frame));
    seekTime(frameToTime(frame));
}

void VideoController::seekTime(double sec)
{
    sec = std::max(0.0, std::min(m_duration, sec));
    m_currentTime = sec;
    m_currentFrame = timeToFrame(sec);

    if (m_playLineMode && sec >= m_playLineEndTime) {
        pause();
    }

    if (m_provider && (m_currentFrame != m_lastRequestedFrame || !m_isPlaying)) {
        m_lastRequestedFrame = m_currentFrame;
        m_provider->requestFrameAsync(m_currentFrame, m_currentTime);
    }

    Q_EMIT positionChanged();
}

void VideoController::stepFrame(int delta)
{
    seekFrame(m_currentFrame + delta);
}

void VideoController::setScrubbing(bool on)
{
    if (!m_provider) return;
    m_provider->setScrubMode(on);
    // Restore full-resolution frame on scrub release to prevent staying on a downscaled proxy frame.
    if (!on && !m_isPlaying) {
        m_provider->requestFrameAsync(m_currentFrame, m_currentTime);
    }
}

void VideoController::openVideo(const QString &path)
{
    QString clean = path;
    if (clean.startsWith("file:///")) clean = QUrl(clean).toLocalFile();
    else if (clean.startsWith("file://")) clean = QUrl(clean).toLocalFile();
    openVideoFile(clean);
}

void VideoController::openVideoFile(const QString &path)
{
    QString clean = path;
    if (clean.startsWith("file:///")) clean = QUrl(clean).toLocalFile();
    else if (clean.startsWith("file://")) clean = QUrl(clean).toLocalFile();

    if (m_provider) {
        m_provider->close();
        m_provider.reset();
    }

    m_timecodes.clear();
    m_hasTimecodes = false;
    m_timecodesPath.clear();
    m_keyframes.clear();
    m_keyframes.insert(0);

    if (clean.startsWith(QLatin1String("?dummy:"))) {
        auto dummy = std::make_unique<DummyVideoProvider>(this);
        connect(dummy.get(), &VideoProvider::keyframesReady, this, &VideoController::onProviderKeyframesReady);
        connect(dummy.get(), &VideoProvider::timecodesReady, this, &VideoController::onProviderTimecodesReady);
        connect(dummy.get(), &VideoProvider::frameReady, this, &VideoController::onProviderFrameReady);
        dummy->open(clean);
        m_provider = std::move(dummy);
    } else {
        auto astra = std::make_unique<AstraVideoProvider>(this);
        connect(astra.get(), &VideoProvider::keyframesReady, this, &VideoController::onProviderKeyframesReady);
        connect(astra.get(), &VideoProvider::timecodesReady, this, &VideoController::onProviderTimecodesReady);
        connect(astra.get(), &VideoProvider::frameReady, this, &VideoController::onProviderFrameReady);
        connect(astra.get(), &VideoProvider::providerError, this, &VideoController::onProviderError);
        if (!astra->open(clean)) {
            qWarning() << "[VideoController] Failed to open video via AstraVideoProvider:" << clean;
        }
        m_provider = std::move(astra);
    }

    if (m_provider && m_provider->isLoaded()) {
        m_hasVideo = true;
        m_isDummy = m_provider->isDummy();
        if (m_isDummy) {
            auto *dummy = dynamic_cast<DummyVideoProvider*>(m_provider.get());
            if (dummy) {
                m_dummyColor = dummy->dummyColor();
            }
        } else {
            m_dummyColor = QStringLiteral("#000000");
        }
        m_videoPath = m_provider->sourcePath();
        m_width = m_provider->width();
        m_height = m_provider->height();
        m_fps = m_provider->fps();
        m_duration = m_provider->duration();
        m_totalFrames = m_provider->totalFrames();
    } else {
        // Open failure: maintain uninitialized video state and report actual error; do not fabricate fake metadata.
        m_hasVideo = false;
        m_isDummy = false;
        m_dummyColor = QStringLiteral("#000000");
        m_videoPath.clear();
        m_width = 0;
        m_height = 0;
        m_fps = 0.0;
        m_duration = 0.0;
        m_totalFrames = 0;
        m_provider.reset();
        // providerError -> videoError already reports root causes during probing; avoid duplicate notifications.
    }

    m_currentFrame = 0;
    m_currentTime = 0.0;
    if (m_fps > 0) {
        m_timer.setInterval(static_cast<int>(1000.0 / m_fps));
    }

    // Search for pre-extracted keyframe index files adjacent to the media source first
    QFileInfo vfi(clean);
    QString base = vfi.completeBaseName();
    QString baseLower = base.toLower().replace(' ', '_');
    QString kfPath1 = vfi.dir().filePath(base + "_keyframes.txt");
    QString kfPath2 = vfi.dir().filePath(baseLower + "_keyframes.txt");
    QString kfPath3 = QCoreApplication::applicationDirPath() + "/" + baseLower + "_keyframes.txt";
    QString kfFile = QFile::exists(kfPath1) ? kfPath1 : (QFile::exists(kfPath2) ? kfPath2 : (QFile::exists(kfPath3) ? kfPath3 : ""));

    bool hasExternalKf = false;
    if (!kfFile.isEmpty()) {
        QFile kf(kfFile);
        if (kf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&kf);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.startsWith('#') || line.startsWith("fps")) continue;
                bool ok = false;
                int f = line.toInt(&ok);
                if (ok && f >= 0 && f < m_totalFrames) {
                    m_keyframes.insert(f);
                }
            }
            if (m_keyframes.size() > 1) {
                hasExternalKf = true;
                qInfo() << "[VideoController] Loaded" << m_keyframes.size() << "keyframes from external index file:" << kfFile;
            }
        }
    }

    // If no external keyframes, trigger asynchronous background extraction
    if (!hasExternalKf && m_provider) {
        m_provider->extractKeyframesAsync();
    }

    // Trigger asynchronous timecode extraction if applicable
    if (m_provider && !m_isDummy) {
        m_provider->extractTimecodesAsync();
    }

    // Request initial preview frame asynchronously without freezing the GUI
    if (m_provider) {
        m_provider->requestFrameAsync(0, 0.0);
    }

    m_defaultKeyframes = m_keyframes;
    m_hasCustomKeyframes = false;
    m_keyframesPath.clear();

    Q_EMIT videoInfoChanged();
    Q_EMIT positionChanged();
    Q_EMIT keyframesChanged();
    Q_EMIT timecodesChanged();
}

void VideoController::openDummyVideo(double fps, int frameCount, int width, int height, const QString &color)
{
    if (m_provider) {
        m_provider->close();
        m_provider.reset();
    }

    m_timecodes.clear();
    m_hasTimecodes = false;
    m_timecodesPath.clear();

    auto dummy = std::make_unique<DummyVideoProvider>(this);
    connect(dummy.get(), &VideoProvider::keyframesReady, this, &VideoController::onProviderKeyframesReady);
    connect(dummy.get(), &VideoProvider::timecodesReady, this, &VideoController::onProviderTimecodesReady);
    connect(dummy.get(), &VideoProvider::frameReady, this, &VideoController::onProviderFrameReady);
    dummy->setup(fps, frameCount, width, height, color);

    m_hasVideo = true;
    m_isDummy = true;
    m_fps = dummy->fps();
    m_totalFrames = dummy->totalFrames();
    m_duration = dummy->duration();
    m_width = dummy->width();
    m_height = dummy->height();
    m_dummyColor = color;
    m_videoPath = dummy->sourcePath();
    m_currentFrame = 0;
    m_currentTime = 0.0;
    m_timer.setInterval(static_cast<int>(1000.0 / m_fps));

    m_keyframes.clear();
    m_keyframes.insert(0);

    m_provider = std::move(dummy);
    m_provider->extractKeyframesAsync();
    m_provider->requestFrameAsync(0, 0.0);

    m_defaultKeyframes = m_keyframes;
    m_hasCustomKeyframes = false;
    m_keyframesPath.clear();

    Q_EMIT videoInfoChanged();
    Q_EMIT positionChanged();
    Q_EMIT keyframesChanged();
    Q_EMIT timecodesChanged();
}

void VideoController::closeVideo()
{
    pause();
    if (m_provider) {
        m_provider->close();
        m_provider.reset();
    }
    m_hasVideo = false;
    m_isDummy = false;
    m_dummyColor = QStringLiteral("#000000");
    m_videoPath.clear();
    m_frameImageSource.clear();
    m_keyframes.clear();
    m_defaultKeyframes.clear();
    m_hasCustomKeyframes = false;
    m_keyframesPath.clear();

    m_timecodes.clear();
    m_hasTimecodes = false;
    m_timecodesPath.clear();

    m_duration = 0.0;
    m_totalFrames = 0;
    m_currentFrame = 0;
    m_currentTime = 0.0;
    m_currentFrameImage = QImage();
    m_lastRequestedFrame = -1;
    Q_EMIT videoInfoChanged();
    Q_EMIT positionChanged();
    Q_EMIT frameImageChanged();
    Q_EMIT keyframesChanged();
    Q_EMIT timecodesChanged();
}

bool VideoController::openKeyframesFile(const QString &path)
{
    QString clean = path;
    if (clean.startsWith("file:///")) clean = QUrl(clean).toLocalFile();
    else if (clean.startsWith("file://")) clean = QUrl(clean).toLocalFile();

    QFile file(clean);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[VideoController] Failed to open keyframes file:" << clean;
        return false;
    }

    QTextStream in(&file);
    QSet<int> newKfs;
    bool isAegiV1 = false;
    int lineIdx = 0;
    int frameCounter = 0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        lineIdx++;

        if (lineIdx == 1 && line.startsWith("# keyframe format v1", Qt::CaseInsensitive)) {
            isAegiV1 = true;
            continue;
        }

        if (isAegiV1 && line.startsWith("fps", Qt::CaseInsensitive)) {
            continue;
        }

        if (line.startsWith('#')) continue;

        // XviD 2-pass stats line parsing: starts with 'i' / 'I' for keyframes
        if (line.length() >= 2 && (line[0] == 'i' || line[0] == 'I' || line[0] == 'p' || line[0] == 'P' || line[0] == 'b' || line[0] == 'B') && (line[1].isSpace() || line[1] == ',')) {
            if (line[0] == 'i' || line[0] == 'I') {
                newKfs.insert(frameCounter);
            }
            frameCounter++;
            continue;
        }

        bool ok = false;
        int f = line.toInt(&ok);
        if (ok && f >= 0) {
            newKfs.insert(f);
        }
    }

    if (newKfs.isEmpty() && frameCounter == 0) {
        qWarning() << "[VideoController] No valid keyframes found in:" << clean;
        return false;
    }

    m_keyframes = newKfs;
    m_hasCustomKeyframes = true;
    m_keyframesPath = clean;
    Q_EMIT keyframesChanged();
    return true;
}

bool VideoController::saveKeyframesFile(const QString &path)
{
    QString clean = path;
    if (clean.startsWith("file:///")) clean = QUrl(clean).toLocalFile();
    else if (clean.startsWith("file://")) clean = QUrl(clean).toLocalFile();

    QFile file(clean);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[VideoController] Failed to write keyframes file:" << clean;
        return false;
    }

    QTextStream out(&file);
    out << "# keyframe format v1\n";
    out << "fps " << QString::number(m_fps, 'f', 6) << "\n";
    QList<int> sorted = m_keyframes.values();
    std::sort(sorted.begin(), sorted.end());
    for (int f : sorted) {
        out << f << "\n";
    }
    return true;
}

void VideoController::closeKeyframes()
{
    m_hasCustomKeyframes = false;
    m_keyframesPath.clear();
    if (!m_defaultKeyframes.isEmpty()) {
        m_keyframes = m_defaultKeyframes;
    } else if (m_hasVideo) {
        m_keyframes.clear();
        m_keyframes.insert(0);
        int step = static_cast<int>(10.0 * m_fps);
        if (step > 0) {
            for (int f = step; f < m_totalFrames; f += step) {
                m_keyframes.insert(f);
            }
        }
    } else {
        m_keyframes.clear();
    }
    Q_EMIT keyframesChanged();
}

bool VideoController::openTimecodesFile(const QString &path)
{
    QString clean = path;
    if (clean.startsWith("file:///")) clean = QUrl(clean).toLocalFile();
    else if (clean.startsWith("file://")) clean = QUrl(clean).toLocalFile();

    QFile file(clean);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[VideoController] Failed to open timecodes file:" << clean;
        return false;
    }

    QTextStream in(&file);
    QString firstLine;
    while (!in.atEnd() && firstLine.isEmpty()) {
        firstLine = in.readLine().trimmed();
    }

    QVector<double> parsedTimecodes;

    if (firstLine.contains("# timecode format v2", Qt::CaseInsensitive)) {
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;
            bool ok = false;
            double ms = line.toDouble(&ok);
            if (ok) {
                parsedTimecodes.append(ms / 1000.0);
            }
        }
    } else if (firstLine.contains("# timecode format v1", Qt::CaseInsensitive)) {
        double defaultFps = m_fps > 0 ? m_fps : 23.976;
        struct Range {
            int start;
            int end;
            double fps;
        };
        QVector<Range> ranges;
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;
            if (line.startsWith("Assume", Qt::CaseInsensitive)) {
                QString val = line.mid(6).trimmed();
                bool ok = false;
                double fpsVal = val.toDouble(&ok);
                if (ok && fpsVal > 0) defaultFps = fpsVal;
            } else if (line.contains(',')) {
                QStringList parts = line.split(',');
                if (parts.size() >= 3) {
                    bool ok1 = false, ok2 = false, ok3 = false;
                    int s = parts[0].trimmed().toInt(&ok1);
                    int e = parts[1].trimmed().toInt(&ok2);
                    double rFps = parts[2].trimmed().toDouble(&ok3);
                    if (ok1 && ok2 && ok3 && rFps > 0 && e >= s) {
                        ranges.append({s, e, rFps});
                    }
                }
            }
        }
        std::sort(ranges.begin(), ranges.end(), [](const Range &a, const Range &b) {
            return a.start < b.start;
        });

        // Upper bound protection: corrupted or malicious v1 files can specify excessive ranges (e.g. INT_MAX).
        // Clamping prevents unbounded vector allocation and integer overflow on r.end + 1.
        // 10^7 frames (~80MB) provides ample headroom for any practical video.
        constexpr int kMaxTimecodeFrames = 10'000'000;
        int maxFrame = m_totalFrames > 0 ? m_totalFrames : 1000;
        for (auto &r : ranges) {
            if (r.start > kMaxTimecodeFrames) r.start = kMaxTimecodeFrames;
            if (r.end > kMaxTimecodeFrames) r.end = kMaxTimecodeFrames;
            if (r.end + 1 > maxFrame) maxFrame = r.end + 1;
        }
        maxFrame = std::min(maxFrame, kMaxTimecodeFrames);

        parsedTimecodes.resize(maxFrame);
        double curTime = 0.0;
        int rIdx = 0;
        for (int f = 0; f < maxFrame; ++f) {
            parsedTimecodes[f] = curTime;
            double fFps = defaultFps;
            while (rIdx < ranges.size() && ranges[rIdx].end < f) {
                rIdx++;
            }
            if (rIdx < ranges.size() && f >= ranges[rIdx].start && f <= ranges[rIdx].end) {
                fFps = ranges[rIdx].fps;
            }
            curTime += 1.0 / fFps;
        }
    } else {
        bool okFirst = false;
        double firstMs = firstLine.toDouble(&okFirst);
        if (okFirst) {
            parsedTimecodes.append(firstMs / 1000.0);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.isEmpty() || line.startsWith('#')) continue;
                bool ok = false;
                double ms = line.toDouble(&ok);
                if (ok) parsedTimecodes.append(ms / 1000.0);
            }
        }
    }

    if (parsedTimecodes.isEmpty()) {
        qWarning() << "[VideoController] No valid timecodes parsed from:" << clean;
        return false;
    }

    m_timecodes = parsedTimecodes;
    m_hasTimecodes = true;
    m_timecodesPath = clean;
    m_totalFrames = static_cast<int>(m_timecodes.size());
    m_duration = m_timecodes.last();

    Q_EMIT timecodesChanged();
    Q_EMIT videoInfoChanged();
    Q_EMIT positionChanged();
    return true;
}

bool VideoController::saveTimecodesFile(const QString &path)
{
    QString clean = path;
    if (clean.startsWith("file:///")) clean = QUrl(clean).toLocalFile();
    else if (clean.startsWith("file://")) clean = QUrl(clean).toLocalFile();

    QFile file(clean);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[VideoController] Failed to write timecodes file:" << clean;
        return false;
    }

    QTextStream out(&file);
    out << "# timecode format v2\n";
    if (!m_timecodes.isEmpty()) {
        for (double t : m_timecodes) {
            out << QString::number(std::round(t * 1000.0), 'f', 0) << "\n";
        }
    } else {
        int total = m_totalFrames > 0 ? m_totalFrames : 1000;
        for (int f = 0; f < total; ++f) {
            double tMs = (f / m_fps) * 1000.0;
            out << QString::number(std::round(tMs), 'f', 0) << "\n";
        }
    }
    return true;
}

void VideoController::closeTimecodesFile()
{
    m_timecodes.clear();
    m_hasTimecodes = false;
    m_timecodesPath.clear();
    if (m_fps > 0 && m_totalFrames > 0) {
        m_duration = m_totalFrames / m_fps;
    }
    Q_EMIT timecodesChanged();
    Q_EMIT videoInfoChanged();
    Q_EMIT positionChanged();
}

void VideoController::onPlaybackTick()
{
    int nextFrame = m_currentFrame + 1;
    if (nextFrame >= m_totalFrames) {
        // Upstream behavior: pause on last frame when reaching playback end, do not loop to frame 0.
        pause();
        seekFrame(m_totalFrames - 1);
        return;
    }

    if (m_playLineMode && frameToTime(nextFrame) >= m_playLineEndTime) {
        seekTime(m_playLineEndTime);
        pause();
        return;
    }

    seekFrame(nextFrame);
}

QString VideoController::formatTime(double seconds) const
{
    int h = static_cast<int>(seconds / 3600);
    int m = static_cast<int>(std::fmod(seconds, 3600.0) / 60);
    int s = static_cast<int>(std::fmod(seconds, 60.0));
    int ms = static_cast<int>(std::round((seconds - static_cast<int>(seconds)) * 1000.0));
    return QString("%1:%2:%3.%4")
        .arg(h)
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(ms, 3, 10, QChar('0'));
}

int VideoController::parseAssTime(const QString &str) const
{
    QStringList parts = str.split(':');
    if (parts.size() < 3) return 0;
    int h = parts[0].toInt();
    int m = parts[1].toInt();
    QStringList sParts = parts[2].split('.');
    int s = sParts[0].toInt();
    int cs = sParts.size() > 1 ? sParts[1].toInt() : 0;
    return (h * 3600 + m * 60 + s) * 1000 + cs * 10;
}

QVariantList VideoController::keyframeList() const
{
    QVariantList list;
    for (int kf : m_keyframes) {
        list.append(kf);
    }
    return list;
}

void VideoController::copyFrame(bool raw, bool withSubs)
{
    Q_UNUSED(raw);
    QImage img;
    if (m_isDummy) {
        img = QImage(m_width, m_height, QImage::Format_RGB32);
        img.fill(QColor(m_dummyColor));
    } else if (!m_currentFrameImage.isNull()) {
        img = m_currentFrameImage.copy();
    } else if (m_provider) {
        img = m_provider->getFrame(m_currentFrame, m_currentTime);
    } else {
        img = QImage(m_width, m_height, QImage::Format_RGB32);
        img.fill(Qt::black);
    }
    if (img.isNull()) {
        img = QImage(m_width, m_height, QImage::Format_RGB32);
        img.fill(Qt::black);
    }

    if (withSubs && !m_activeSubText.isEmpty()) {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        QFont font("Microsoft YaHei", 32, QFont::Bold);
        p.setFont(font);
        QString cleanText = m_activeSubText;
        cleanText.remove(QRegularExpression("\\{[^}]*\\}"));
        QRect r(0, m_height - 120, m_width, 80);
        p.setPen(QPen(Qt::black, 3));
        p.drawText(r.translated(2, 2), Qt::AlignCenter, cleanText);
        p.setPen(Qt::white);
        p.drawText(r, Qt::AlignCenter, cleanText);
    }

    QGuiApplication::clipboard()->setImage(img);
}

void VideoController::saveFrame(bool raw, bool withSubs)
{
    Q_UNUSED(raw);
    QString outDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (outDir.isEmpty()) outDir = ".";
    QString filename = QString("%1/frame_%2.png").arg(outDir).arg(m_currentFrame);

    QImage img;
    if (m_isDummy) {
        img = QImage(m_width, m_height, QImage::Format_RGB32);
        img.fill(QColor(m_dummyColor));
    } else if (!m_currentFrameImage.isNull()) {
        img = m_currentFrameImage.copy();
    } else if (m_provider) {
        img = m_provider->getFrame(m_currentFrame, m_currentTime);
    } else {
        img = QImage(m_width, m_height, QImage::Format_RGB32);
        img.fill(Qt::black);
    }
    if (img.isNull()) {
        img = QImage(m_width, m_height, QImage::Format_RGB32);
        img.fill(Qt::black);
    }

    if (withSubs && !m_activeSubText.isEmpty()) {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        QFont font("Microsoft YaHei", 32, QFont::Bold);
        p.setFont(font);
        QString cleanText = m_activeSubText;
        cleanText.remove(QRegularExpression("\\{[^}]*\\}"));
        QRect r(0, m_height - 120, m_width, 80);
        p.setPen(QPen(Qt::black, 3));
        p.drawText(r.translated(2, 2), Qt::AlignCenter, cleanText);
        p.setPen(Qt::white);
        p.drawText(r, Qt::AlignCenter, cleanText);
    }

    img.save(filename);
}

void VideoController::copyCoordinates(int x, int y)
{
    QString coord = QString("%1, %2").arg(x).arg(y);
    QGuiApplication::clipboard()->setText(coord);
}

bool VideoController::exportClip(const QString &outputPath, int startMs, int endMs, bool streamCopy)
{
    if (m_videoPath.isEmpty() || !m_hasVideo || m_isDummy) {
        return false;
    }
    if (startMs < 0) startMs = 0;
    if (endMs <= startMs) return false;

    double startSec = startMs / 1000.0;
    double durationSec = (endMs - startMs) / 1000.0;

    return AstraCoreBridge::instance()->trimMedia(m_videoPath, outputPath, startSec, durationSec, streamCopy);
}

void VideoController::onProviderFrameReady(int frameNumber, const QImage &image, double actualPts)
{
    Q_UNUSED(frameNumber);
    Q_UNUSED(actualPts);
    if (image.isNull()) return;

    m_currentFrameImage = image;
    if (auto *provider = VideoFrameImageProvider::instance()) {
        provider->setFrame(image);
    }

    m_frameImageSource = QStringLiteral("image://videoframe/%1").arg(++m_frameRevision);
    Q_EMIT frameImageChanged();
}

void VideoController::onProviderKeyframesReady(const QVector<int64_t> &indices, const QVector<double> &timestamps)
{
    if (indices.isEmpty() && timestamps.isEmpty()) return;

    // Do not overwrite if a custom keyframes file was explicitly supplied by user.
    if (m_hasCustomKeyframes) return;

    m_keyframes.clear();
    for (int64_t f : indices) {
        if (f >= 0 && f < m_totalFrames) {
            m_keyframes.insert(static_cast<int>(f));
        }
    }

    if (m_keyframes.isEmpty() && !timestamps.isEmpty() && m_fps > 0) {
        for (double ts : timestamps) {
            int f = static_cast<int>(std::round(ts * m_fps));
            if (f >= 0 && f < m_totalFrames) {
                m_keyframes.insert(f);
            }
        }
    }

    if (m_keyframes.isEmpty()) {
        m_keyframes.insert(0);
    }

    m_defaultKeyframes = m_keyframes;
    qInfo() << "[VideoController] Decoupled async load:" << m_keyframes.size() << "keyframes from VideoProvider";
    Q_EMIT keyframesChanged();
}

void VideoController::onProviderTimecodesReady(const QVector<double> &timecodes)
{
    if (timecodes.size() < 2 || m_hasTimecodes) return;

    // Timestamps are provided in seconds; sort in ascending frame order to guarantee monotonic indexing.
    // Aligns with frameToTime() contract in seconds without intermediate millisecond conversion.
    m_timecodes = timecodes;
    std::sort(m_timecodes.begin(), m_timecodes.end());
    m_hasTimecodes = true;
    qInfo() << "[VideoController] Decoupled async load:" << m_timecodes.size() << "VFR timecodes from VideoProvider";
    Q_EMIT timecodesChanged();
}

void VideoController::onProviderError(const QString &errorMessage)
{
    qWarning() << "[VideoController] Provider error:" << errorMessage;
    // Forward decoder backend errors to UI status notifications.
    Q_EMIT videoError(errorMessage);
}

