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
#include <limits>
#include <numeric>
#include <libaegisub/fs.h>
#include <libaegisub/ass/time.h>
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

QVariantMap VideoController::exportFramerateContext() const {
    const auto &output = activeFramerate();
    QVariantList timestamps;
    if (output.IsVFR())
        for (int milliseconds : output.Timecodes()) timestamps.append(milliseconds);
    return {{"available", output.IsLoaded()},
            {"inputFps", m_nativeFramerate.IsLoaded() ? m_nativeFramerate.FPS() : output.FPS()},
            {"outputFps", output.FPS()}, {"isVfr", output.IsVFR()}, {"timecodes", timestamps}};
}

QVariantMap VideoController::videoDetails() const {
    if (!m_hasVideo || !m_provider) return {{"hasVideo", false}};
    const int divisor = std::gcd(m_width, m_height);
    const QString aspect = divisor > 0 ? QStringLiteral("%1:%2").arg(m_width / divisor).arg(m_height / divisor)
                                       : QStringLiteral("Unknown");
    QString matrix = QStringLiteral("Unknown");
    if (m_isDummy) matrix = QStringLiteral("RGB");
    else switch (m_provider->colorSpace()) {
    case 0: matrix = QStringLiteral("RGB"); break;
    case 1: matrix = QStringLiteral("BT.709"); break;
    case 4: matrix = QStringLiteral("FCC"); break;
    case 5: case 6: matrix = QStringLiteral("BT.601"); break;
    case 7: matrix = QStringLiteral("SMPTE 240M"); break;
    case 8: matrix = QStringLiteral("YCgCo"); break;
    case 9: matrix = QStringLiteral("BT.2020 NCL"); break;
    case 10: matrix = QStringLiteral("BT.2020 CL"); break;
    default: break;
    }
    const int lengthMs = m_totalFrames > 0 && activeFramerate().IsLoaded()
        ? std::max(0, timeAtFrameMs(m_totalFrames - 1))
        : static_cast<int>(std::clamp(std::round(m_duration * 1000.0), 0.0,
                                     static_cast<double>(std::numeric_limits<int>::max())));
    const QString range = m_isDummy ? QStringLiteral("Full (RGB)")
        : m_provider->colorRange() == 1 ? QStringLiteral("Limited")
        : m_provider->colorRange() == 2 ? QStringLiteral("Full") : QStringLiteral("Unknown");
    return {{"hasVideo", true}, {"fileName", m_videoPath},
            {"fps", activeFramerate().IsLoaded() ? activeFramerate().FPS() : m_fps},
            {"width", m_width}, {"height", m_height}, {"aspectRatio", aspect},
            {"frameCount", m_totalFrames},
            {"length", QString::fromStdString(agi::Time(lengthMs).GetAssFormatted(true))},
            {"colorMatrix", matrix},
            {"colorRange", range},
            {"decoder", m_isDummy ? QStringLiteral("Dummy Video") : QStringLiteral("AstraCore / FFmpeg")}};
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

int VideoController::timeAtFrameMs(int frame, int timeType) const
{
    if (!activeFramerate().IsLoaded()) return 0;
    const auto type = timeType == 1 ? agi::vfr::START : timeType == 2 ? agi::vfr::END : agi::vfr::EXACT;
    return activeFramerate().TimeAtFrame(frame, type);
}

int VideoController::frameAtTimeMs(int ms, int timeType) const
{
    if (!activeFramerate().IsLoaded()) return 0;
    const auto type = timeType == 1 ? agi::vfr::START : timeType == 2 ? agi::vfr::END : agi::vfr::EXACT;
    return activeFramerate().FrameAtTime(ms, type);
}

double VideoController::frameToTime(int frame) const
{
    return timeAtFrameMs(frame) / 1000.0;
}

int VideoController::timeToFrame(double sec) const
{
    if (!std::isfinite(sec)) return 0;
    // Floor to the containing millisecond; tolerate floating point noise from frameToTime().
    const double ms = std::floor(sec * 1000.0 + 1e-7);
    return frameAtTimeMs(static_cast<int>(std::clamp(ms, double(std::numeric_limits<int>::min() + 1),
                                                    double(std::numeric_limits<int>::max()))));
}

QVariantMap VideoController::currentSceneBoundsMs() const
{
    if (!m_hasVideo || m_keyframes.isEmpty() || m_totalFrames <= 0) return {};
    QVector<int> frames(m_keyframes.begin(), m_keyframes.end());
    std::sort(frames.begin(), frames.end());
    const auto next = std::upper_bound(frames.cbegin(), frames.cend(), m_currentFrame);
    const int startFrame = next == frames.cbegin() ? 0 : *(next - 1);
    const int endFrame = next == frames.cend() ? m_totalFrames : *next;
    const int startMs = std::max(0, timeAtFrameMs(startFrame, 1));
    const int endMs = timeAtFrameMs(endFrame - 1, 2);
    if (endMs <= startMs) return {};
    return {{QStringLiteral("start"), startMs}, {QStringLiteral("end"), endMs}};
}

void VideoController::seekFrame(int frame)
{
    frame = std::max(0, std::min(m_totalFrames - 1, frame));
    seekTime(frameToTime(frame));
}

void VideoController::seekTime(double sec)
{
    if (!m_hasVideo || m_totalFrames <= 0 || !std::isfinite(sec)) return;
    sec = std::max(0.0, std::min(m_duration, sec));
    m_currentTime = sec;
    m_currentFrame = std::clamp(timeToFrame(sec), 0, m_totalFrames - 1);

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
    m_nativeTimecodes.clear();
    m_nativeFramerate = agi::vfr::Framerate();
    m_overrideFramerate = agi::vfr::Framerate();
    m_hasCustomTimecodes = false;
    m_hasTimecodes = false;
    m_timecodesPath.clear();
    m_keyframes.clear();

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
        m_nativeDuration = m_duration;
        m_nativeFramerate = agi::vfr::Framerate(std::isfinite(m_fps) && m_fps > 0 && m_fps <= 1000 ? m_fps : 0.);
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
        m_nativeDuration = 0.0;
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
    m_nativeTimecodes.clear();
    m_nativeFramerate = agi::vfr::Framerate();
    m_overrideFramerate = agi::vfr::Framerate();
    m_hasCustomTimecodes = false;
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
    m_nativeDuration = m_duration;
    m_nativeFramerate = agi::vfr::Framerate(m_fps);
    m_width = dummy->width();
    m_height = dummy->height();
    m_dummyColor = color;
    m_videoPath = dummy->sourcePath();
    m_currentFrame = 0;
    m_currentTime = 0.0;
    m_timer.setInterval(static_cast<int>(1000.0 / m_fps));

    m_keyframes.clear();

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
    m_nativeTimecodes.clear();
    m_nativeFramerate = agi::vfr::Framerate();
    m_overrideFramerate = agi::vfr::Framerate();
    m_hasCustomTimecodes = false;
    m_hasTimecodes = false;
    m_timecodesPath.clear();

    m_duration = 0.0;
    m_nativeDuration = 0.0;
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
    m_keyframes = m_defaultKeyframes;
    Q_EMIT keyframesChanged();
}

bool VideoController::openTimecodesFile(const QString &path)
{
    const QString clean = path.startsWith("file:") ? QUrl(path).toLocalFile() : path;
    try {
        if (QFileInfo(clean).size() > 200'000'000)
            throw agi::vfr::InvalidFramerate("Timecode file exceeds supported size");
        const agi::vfr::Framerate parsed(agi::fs::path(clean.toUtf8().toStdString()));
        QVector<double> times;
        times.reserve(static_cast<qsizetype>(parsed.Timecodes().size()));
        for (int milliseconds : parsed.Timecodes()) times.append(milliseconds / 1000.0);
        const double duration = parsed.TimeAtFrame(m_totalFrames) / 1000.0;
        m_overrideFramerate = parsed;
        m_hasCustomTimecodes = true;
        m_timecodes = std::move(times);
        m_hasTimecodes = true;
        m_timecodesPath = clean;
        if (m_hasVideo) {
            m_duration = duration;
            m_currentTime = frameToTime(m_currentFrame);
        }
    } catch (const std::exception &error) {
        Q_EMIT videoError(tr("Failed to load timecodes: %1").arg(QString::fromUtf8(error.what())));
        return false;
    }
    Q_EMIT timecodesChanged();
    Q_EMIT videoInfoChanged();
    Q_EMIT positionChanged();
    return true;
}

bool VideoController::saveTimecodesFile(const QString &path)
{
    const QString clean = path.startsWith("file:") ? QUrl(path).toLocalFile() : path;
    try {
        if (!activeFramerate().IsLoaded()) return false;
        activeFramerate().Save(agi::fs::path(clean.toUtf8().toStdString()), m_totalFrames);
    } catch (const std::exception &error) {
        Q_EMIT videoError(tr("Failed to save timecodes: %1").arg(QString::fromUtf8(error.what())));
        return false;
    }
    return true;
}

void VideoController::closeTimecodesFile()
{
    m_hasCustomTimecodes = false;
    m_overrideFramerate = agi::vfr::Framerate();
    m_timecodes = m_nativeTimecodes;
    m_hasTimecodes = !m_nativeTimecodes.isEmpty();
    m_timecodesPath.clear();
    m_duration = m_nativeDuration;
    m_currentTime = frameToTime(m_currentFrame);
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
        QFont font = QGuiApplication::font();
        font.setFamilies({"Microsoft YaHei", "PingFang SC", "Noto Sans CJK SC", "WenQuanYi Micro Hei", "sans-serif"});
        font.setPointSize(32);
        font.setBold(true);
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
        QFont font = QGuiApplication::font();
        font.setFamilies({"Microsoft YaHei", "PingFang SC", "Noto Sans CJK SC", "WenQuanYi Micro Hei", "sans-serif"});
        font.setPointSize(32);
        font.setBold(true);
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
    Q_UNUSED(actualPts);
    if (!m_hasVideo || frameNumber != m_currentFrame || image.isNull()) return;

    m_currentFrameImage = image;
    if (auto *provider = VideoFrameImageProvider::instance()) {
        provider->setFrame(image);
    }

    m_frameImageSource = QStringLiteral("image://videoframe/%1").arg(++m_frameRevision);
    Q_EMIT frameImageChanged();
}

void VideoController::onProviderKeyframesReady(const QVector<int64_t> &indices, const QVector<double> &timestamps)
{
    QSet<int> parsed;
    for (int64_t f : indices) {
        if (f >= 0 && f < m_totalFrames) {
            parsed.insert(static_cast<int>(f));
        }
    }

    if (parsed.isEmpty() && m_nativeFramerate.IsLoaded()) {
        for (double ts : timestamps) {
            if (!std::isfinite(ts) || ts < 0 || ts * 1000.0 > std::numeric_limits<int>::max()) continue;
            int f = m_nativeFramerate.FrameAtTime(static_cast<int>(std::round(ts * 1000.0)));
            if (f >= 0 && f < m_totalFrames) {
                parsed.insert(f);
            }
        }
    }

    // Retain native results even while a custom table is active, so Close restores them.
    m_defaultKeyframes = parsed;
    if (m_hasCustomKeyframes) return;
    m_keyframes = std::move(parsed);
    Q_EMIT keyframesChanged();
}

void VideoController::onProviderTimecodesReady(const QVector<double> &timecodes)
{
    if (timecodes.size() < 2) return;
    try {
        std::vector<int> milliseconds;
        milliseconds.reserve(timecodes.size());
        for (double seconds : timecodes) {
            if (!std::isfinite(seconds) || seconds < 0 || seconds * 1000.0 > std::numeric_limits<int>::max() - .5)
                throw agi::vfr::InvalidFramerate("Invalid provider timestamp");
            milliseconds.push_back(static_cast<int>(std::round(seconds * 1000.0)));
        }
        // Preserve frame order; sorting timestamps would associate times with the wrong frames.
        m_nativeFramerate = agi::vfr::Framerate(std::move(milliseconds));
        m_nativeTimecodes = timecodes;
        if (!m_hasCustomTimecodes) {
            m_timecodes = m_nativeTimecodes;
            m_hasTimecodes = true;
            m_currentTime = frameToTime(m_currentFrame);
            Q_EMIT timecodesChanged();
            Q_EMIT positionChanged();
        }
    } catch (const std::exception &error) {
        Q_EMIT videoError(tr("Invalid video timestamps: %1").arg(QString::fromUtf8(error.what())));
    }
}

void VideoController::onProviderError(const QString &errorMessage)
{
    qWarning() << "[VideoController] Provider error:" << errorMessage;
    // Forward decoder backend errors to UI status notifications.
    Q_EMIT videoError(errorMessage);
}

