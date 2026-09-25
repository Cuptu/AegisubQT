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

#include "AudioController.h"
#include "AudioSliceDevice.h"
#include "AegisubCoreBridge.h"
#include <QAudioSink>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QAudioDevice>
#include <cmath>
#include <algorithm>
#include <QRandomGenerator>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QThreadPool>
#include <QRunnable>
#include <QDebug>

namespace {

// Waveform Similarity Overlap-Add (WSOLA) pitch-preserving time stretching.
// Scales playback duration by 1.0 / speed while keeping vocal pitch constant.
std::vector<int16_t> timeStretchWsola(const int16_t *input, size_t inputLen, double speed, int sampleRate)
{
    if (inputLen == 0 || speed <= 0.1 || speed > 4.0 || std::abs(speed - 1.0) < 0.01) {
        return std::vector<int16_t>(input, input + inputLen);
    }

    const int frameSize = std::max(64, sampleRate / 50); // 20ms frame
    const int hopOut = frameSize / 2;                   // 10ms hop
    const int hopIn = static_cast<int>(std::round(hopOut * speed));
    const int maxDelta = frameSize / 4;                 // 5ms search range

    size_t estOutLen = static_cast<size_t>(inputLen / speed) + frameSize;
    std::vector<int16_t> output;
    output.reserve(estOutLen);

    if (inputLen < static_cast<size_t>(frameSize + maxDelta)) {
        return std::vector<int16_t>(input, input + inputLen);
    }

    output.insert(output.end(), input, input + hopOut);

    int inPos = hopIn;
    while (inPos + frameSize + maxDelta < static_cast<int>(inputLen)) {
        int bestDelta = 0;
        int64_t bestCorr = INT64_MIN;

        const int16_t *ref = &output[output.size() - hopOut];
        for (int d = -maxDelta; d <= maxDelta; ++d) {
            int candidatePos = inPos + d;
            if (candidatePos < 0 || candidatePos + hopOut > static_cast<int>(inputLen)) continue;
            const int16_t *cand = &input[candidatePos];
            int64_t corr = 0;
            for (int k = 0; k < hopOut; k += 2) {
                corr += static_cast<int64_t>(ref[k]) * cand[k];
            }
            if (corr > bestCorr) {
                bestCorr = corr;
                bestDelta = d;
            }
        }

        int targetIn = inPos + bestDelta;
        for (int k = 0; k < hopOut; ++k) {
            float fade = static_cast<float>(k) / hopOut;
            int16_t prev = output[output.size() - hopOut + k];
            int16_t next = input[targetIn + k];
            output[output.size() - hopOut + k] = static_cast<int16_t>(prev * (1.0f - fade) + next * fade);
        }
        output.insert(output.end(), input + targetIn + hopOut, input + targetIn + frameSize);

        inPos += hopIn;
    }

    return output;
}

} // namespace

AudioController::AudioController(VideoController *videoController, QObject *parent)
    : QObject(parent)
    , m_videoController(videoController)
{
    m_playbackTimer.setInterval(16);
    connect(&m_playbackTimer, &QTimer::timeout, this, &AudioController::onPlaybackTimerTick);

    if (m_videoController) {
        m_selectionStart = m_videoController->activeSubStart();
        m_selectionEnd = m_videoController->activeSubEnd();
        m_currentTime = m_videoController->currentTime();

        connect(m_videoController, &VideoController::positionChanged, this, [this]() {
            if (!m_isPlaying) {
                m_currentTime = m_videoController->currentTime();
                Q_EMIT positionChanged();

                if (m_autoScroll && m_videoController->isPlaying()) {
                    int curPixel = static_cast<int>((m_currentTime * 1000.0) / m_msPerPixel);
                    int viewRight = m_scrollLeft + m_viewportWidth;
                    if (curPixel >= viewRight - 40) {
                        scrollBy(curPixel - (m_scrollLeft + 100));
                    }
                }
            }
        });

        connect(m_videoController, &VideoController::videoInfoChanged, this, [this]() {
            // When no audio is loaded, timeline duration reflects video duration (dynamic duration()).
            // Once audio finishes loading, audio duration takes precedence.
            if (!m_hasAudio) {
                Q_EMIT audioInfoChanged();
            }
            Q_EMIT keyframesChanged();
        });

        connect(m_videoController, &VideoController::playbackStateChanged, this, [this]() {
            // Stop audio playback if video controller was paused or stopped while audio was playing.
            // Do not set m_isPlaying to true when only video starts; AudioController should only be marked
            // as playing when its own audio sink / slice device is actively generating audio.
            if (!m_videoController->isPlaying() && m_isPlaying) {
                stop();
            }
        });

        connect(m_videoController, &VideoController::subtitleSyncChanged, this, [this]() {
            m_selectionStart = m_videoController->activeSubStart();
            m_selectionEnd = m_videoController->activeSubEnd();
            Q_EMIT selectionChanged();
        });

        connect(m_videoController, &VideoController::keyframesChanged, this, &AudioController::keyframesChanged);
    }

    setZoomLevel(0);
    updateAmplitudeScale();
    // Default spectrum frequency curve mapping (Audio/Renderer/Spectrum/FreqCurve = 0, Linear).
    applyFreqCurve(0);
}

AudioController::~AudioController()
{
    stop();
    delete m_audioSink;
    m_audioSink = nullptr;
    delete m_sliceDevice;
    m_sliceDevice = nullptr;
}

int AudioController::getZoomLevelFactor(int level)
{
    // Matches Aegisub AudioDisplay::GetZoomLevelFactor piecewise scaling.
    int factor = 100;
    if (level > 0) {
        factor += 25 * level;
    } else if (level < 0) {
        if (level >= -5)
            factor += 10 * level;
        else if (level >= -11)
            factor = 50 + (level + 5) * 5;
        else
            factor = 20 + level + 11;
        if (factor <= 0)
            factor = 1;
    }
    return factor;
}

void AudioController::updateAmplitudeScale()
{
    // Cubic scaling matching Aegisub: pow(mid(1, VerticalZoom, 100) / 50.0, 3).
    int clamped = std::clamp(m_verticalZoom, 1, 100);
    m_amplitudeScale = std::pow(clamped / 50.0, 3.0);
    Q_EMIT verticalZoomChanged();
}

QVariantList AudioController::keyframes() const
{
    QVariantList list;
    if (m_videoController) {
        for (int f : m_videoController->keyframes()) {
            int ms = static_cast<int>(std::round(m_videoController->frameToTime(f) * 1000.0));
            list.append(ms);
        }
    }
    return list;
}

void AudioController::setSelection(int startMs, int endMs)
{
    startMs = std::max(0, startMs);
    endMs = std::max(startMs, endMs);
    if (m_selectionStart != startMs || m_selectionEnd != endMs) {
        m_selectionStart = startMs;
        m_selectionEnd = endMs;
        if (m_videoController) {
            m_videoController->setActiveSubtitle(startMs, endMs, m_videoController->activeSubText());
        }
        Q_EMIT selectionChanged();
    }
}

void AudioController::setSelectionStart(int startMs)
{
    setSelection(startMs, m_selectionEnd);
}

void AudioController::setSelectionEnd(int endMs)
{
    setSelection(m_selectionStart, endMs);
}

void AudioController::shiftSelection(int deltaMs)
{
    int dur = selectionDuration();
    int newStart = std::max(0, m_selectionStart + deltaMs);
    setSelection(newStart, newStart + dur);
}

void AudioController::scrollBy(int deltaPx)
{
    scrollTo(m_scrollLeft + deltaPx);
}

void AudioController::scrollTo(int pixelPos)
{
    int maxPixel = static_cast<int>((duration() * 1000.0) / m_msPerPixel);
    pixelPos = std::max(0, std::min(maxPixel, pixelPos));
    if (m_scrollLeft != pixelPos) {
        m_scrollLeft = pixelPos;
        Q_EMIT scrollChanged();
    }
}

void AudioController::scrollTimeToCenter(int timeMs)
{
    int px = static_cast<int>(timeMs / m_msPerPixel);
    scrollTo(px - m_viewportWidth / 2); // Center within the actual audio display viewport.
}

void AudioController::scrollRangeInView(int startMs, int endMs, int viewportWidth)
{
    if (viewportWidth <= 0) {
        viewportWidth = m_viewportWidth; // Use the viewport width reported by AudioDisplayController.
    }
    int startPx = static_cast<int>(startMs / m_msPerPixel);
    int endPx = static_cast<int>(endMs / m_msPerPixel);
    if (startPx < m_scrollLeft) {
        scrollTo(startPx - 20);
    } else if (endPx > m_scrollLeft + viewportWidth) {
        scrollTo(endPx - viewportWidth + 20);
    }
}

void AudioController::setViewportWidth(int width)
{
    width = std::max(200, width);
    m_viewportWidth = width;
}

void AudioController::setZoomLevel(int level)
{
    level = std::clamp(level, -50, 30);
    if (m_zoomLevel != level) {
        m_zoomLevel = level;
        int factor = getZoomLevelFactor(level);
        const double basePixelsPerSecond = 50.0;
        const double baseMsPerPixel = 1000.0 / basePixelsPerSecond;
        m_msPerPixel = (100.0 * baseMsPerPixel) / factor;
        Q_EMIT zoomChanged();
    }
}

void AudioController::setMsPerPixel(double val)
{
    if (val > 0.1 && std::abs(m_msPerPixel - val) > 0.001) {
        m_msPerPixel = val;
        Q_EMIT zoomChanged();
    }
}

void AudioController::setVolume(int vol)
{
    vol = std::clamp(vol, 0, 100);
    if (m_volume != vol) {
        m_volume = vol;
        if (m_audioSink) {
            m_audioSink->setVolume(vol / 100.0f);
        }
        if (m_linkVolumeAndZoom) {
            m_verticalZoom = vol;
            updateAmplitudeScale();
        }
        Q_EMIT volumeChanged();
    }
}

void AudioController::setVerticalZoom(int zoom)
{
    zoom = std::clamp(zoom, 0, 100);
    if (m_verticalZoom != zoom) {
        m_verticalZoom = zoom;
        if (m_linkVolumeAndZoom) {
            m_volume = zoom;
            // Synchronize active playback volume when linked to keep slider and audio level in lockstep.
            if (m_audioSink) {
                m_audioSink->setVolume(m_volume / 100.0f);
            }
            Q_EMIT volumeChanged();
        }
        updateAmplitudeScale();
    }
}

void AudioController::setLinkVolumeAndZoom(bool link)
{
    if (m_linkVolumeAndZoom != link) {
        m_linkVolumeAndZoom = link;
        if (link) {
            m_volume = m_verticalZoom;
            // Immediately apply vertical zoom as playback volume when linkage is engaged.
            if (m_audioSink) {
                m_audioSink->setVolume(m_volume / 100.0f);
            }
            Q_EMIT volumeChanged();
        }
        Q_EMIT linkChanged();
    }
}

void AudioController::setSpectrumMode(bool val)
{
    if (m_spectrumMode != val) {
        m_spectrumMode = val;
        Q_EMIT spectrumModeChanged();
    }
}

void AudioController::toggleSpectrumMode()
{
    setSpectrumMode(!m_spectrumMode);
}

void AudioController::setAutoScroll(bool val)
{
    if (m_autoScroll != val) {
        m_autoScroll = val;
        Q_EMIT autoScrollChanged();
    }
}

void AudioController::setAutoCommit(bool val)
{
    if (m_autoCommit != val) {
        m_autoCommit = val;
        Q_EMIT autoCommitChanged();
    }
}

void AudioController::setAutoNext(bool val)
{
    if (m_autoNext != val) {
        m_autoNext = val;
        Q_EMIT autoNextChanged();
    }
}

void AudioController::setKaraokeMode(bool val)
{
    if (m_karaokeMode != val) {
        m_karaokeMode = val;
        Q_EMIT karaokeModeChanged();
    }
}

void AudioController::setMedusaMode(bool val)
{
    if (m_medusaMode != val) {
        m_medusaMode = val;
        Q_EMIT medusaModeChanged();
    }
}

void AudioController::toggleMedusaMode()
{
    setMedusaMode(!m_medusaMode);
}

void AudioController::setPlaybackSpeed(double speed)
{
    speed = std::clamp(speed, 0.25, 4.0);
    if (std::abs(m_playbackSpeed - speed) > 0.001) {
        m_playbackSpeed = speed;
        Q_EMIT playbackSpeedChanged();
    }
}

void AudioController::setupAudioSink(int sampleRate)
{
    QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    if (defaultDevice.isNull()) {
        qWarning() << "[AudioController] No default audio output device available!";
        return;
    }

    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    if (m_audioSink) {
        if (m_audioSink->format() == format) {
            return;
        }
        delete m_audioSink;
        m_audioSink = nullptr;
    }

    m_audioSink = new QAudioSink(defaultDevice, format, this);
    connect(m_audioSink, &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (state == QAudio::IdleState) {
            double curSec = m_currentTime;
            double endSec = m_playbackEndMs / 1000.0;
            if (curSec >= (endSec - 0.05)) {
                stop();
            }
        } else if (state == QAudio::StoppedState) {
            if (m_audioSink && m_audioSink->error() != QAudio::NoError) {
                qWarning() << "[AudioController] QAudioSink stopped with error:" << m_audioSink->error();
            }
        }
    });
}

void AudioController::playRange(int startMs, int endMs)
{
    int maxMs = durationMs();
    if (maxMs <= 0) maxMs = 3600000;
    startMs = std::clamp(startMs, 0, maxMs);
    endMs = std::clamp(endMs, startMs, maxMs);

    if (startMs >= endMs) {
        return;
    }

    // Stop current playback cleanly without resetting target state
    if (m_playbackTimer.isActive()) m_playbackTimer.stop();
    if (m_audioSink) m_audioSink->reset();
    if (m_sliceDevice) {
        m_sliceDevice->close();
        m_sliceDevice->deleteLater();
        m_sliceDevice = nullptr;
    }

    m_playbackStartMs = startMs;
    m_playbackEndMs = endMs;
    m_currentTime = startMs / 1000.0;
    m_isPlaying = true;

    if (m_videoController) {
        m_videoController->seekTime(m_currentTime);
        m_videoController->play();
    }
    Q_EMIT positionChanged();
    Q_EMIT playbackChanged();

    const auto &samples = m_pcmProvider.samples();
    int sampleRate = m_pcmProvider.sampleRate();
    if (sampleRate <= 0) sampleRate = 16000;

    bool audioHardwareStarted = false;
    if (m_pcmProvider.isVirtual()) {
        // Blank/noise audio: stream synthesized samples instead of a materialized buffer.
        const int64_t totalSamples = m_pcmProvider.numSamples();
        int64_t startSample = static_cast<int64_t>((static_cast<double>(startMs) / 1000.0) * sampleRate);
        int64_t endSample = static_cast<int64_t>((static_cast<double>(endMs) / 1000.0) * sampleRate);
        startSample = std::clamp<int64_t>(startSample, 0, totalSamples);
        endSample = std::clamp<int64_t>(endSample, startSample, totalSamples);
        const int64_t sampleCount = endSample - startSample;

        if (sampleCount > 0) {
            setupAudioSink(sampleRate);
            if (m_audioSink) {
                const auto kind = (m_pcmProvider.virtualKind() == AudioPcmProvider::VirtualKind::Noise)
                    ? VirtualAudioSliceDevice::Kind::Noise
                    : VirtualAudioSliceDevice::Kind::Blank;
                m_sliceDevice = new VirtualAudioSliceDevice(kind, sampleCount, this);
                float vol = std::clamp(m_volume / 100.0f, 0.0f, 1.0f);
                m_audioSink->setVolume(vol);
                m_audioSink->start(m_sliceDevice);
                audioHardwareStarted = (m_audioSink->state() != QAudio::StoppedState);
            }
        }
    } else if (!samples.empty()) {
        int64_t startSample = static_cast<int64_t>((static_cast<double>(startMs) / 1000.0) * sampleRate);
        int64_t endSample = static_cast<int64_t>((static_cast<double>(endMs) / 1000.0) * sampleRate);
        startSample = std::clamp<int64_t>(startSample, 0, static_cast<int64_t>(samples.size()));
        endSample = std::clamp<int64_t>(endSample, startSample, static_cast<int64_t>(samples.size()));
        int64_t sampleCount = endSample - startSample;

        if (sampleCount > 0) {
            setupAudioSink(sampleRate);
            if (m_audioSink) {
                if (std::abs(m_playbackSpeed - 1.0) > 0.01) {
                    m_playbackBuffer = timeStretchWsola(samples.data() + startSample, static_cast<size_t>(sampleCount), m_playbackSpeed, sampleRate);
                    m_sliceDevice = new AudioSliceDevice(m_playbackBuffer.data(), static_cast<qint64>(m_playbackBuffer.size()), this);
                } else {
                    m_sliceDevice = new AudioSliceDevice(samples.data() + startSample, sampleCount, this);
                }
                float vol = std::clamp(m_volume / 100.0f, 0.0f, 1.0f);
                m_audioSink->setVolume(vol);
                m_audioSink->start(m_sliceDevice);
                audioHardwareStarted = (m_audioSink->state() != QAudio::StoppedState);
            }
        }
    }

    if (!audioHardwareStarted) {
        m_fallbackTimer.start();
    }

    m_playbackTimer.start();
}

void AudioController::onPlaybackTimerTick()
{
    if (!m_isPlaying) {
        m_playbackTimer.stop();
        return;
    }

    double elapsedSec = 0.0;
    bool hasHardwareSink = (m_audioSink && m_audioSink->state() == QAudio::ActiveState);
    if (hasHardwareSink) {
        qint64 uSecs = m_audioSink->processedUSecs();
        elapsedSec = (static_cast<double>(uSecs) / 1000000.0) * m_playbackSpeed;
    } else {
        elapsedSec = (static_cast<double>(m_fallbackTimer.elapsed()) / 1000.0) * m_playbackSpeed;
    }

    double newTime = (m_playbackStartMs / 1000.0) + elapsedSec;
    double endSec = m_playbackEndMs / 1000.0;

    bool finished = (newTime >= endSec);
    if (m_audioSink && m_audioSink->state() == QAudio::IdleState && newTime >= (endSec - 0.05)) {
        finished = true;
    }

    if (finished) {
        m_currentTime = endSec;
        if (m_videoController) {
            m_videoController->seekTime(m_currentTime);
        }
        Q_EMIT positionChanged();
        stop();
        return;
    }

    m_currentTime = newTime;
    if (m_videoController) {
        m_videoController->seekTime(m_currentTime);
    }

    if (m_autoScroll) {
        int curPixel = static_cast<int>((m_currentTime * 1000.0) / m_msPerPixel);
        int viewRight = m_scrollLeft + m_viewportWidth;
        if (curPixel >= viewRight - 40) {
            scrollBy(curPixel - (m_scrollLeft + 100));
        }
    }

    Q_EMIT positionChanged();
}

void AudioController::stop()
{
    if (m_playbackTimer.isActive()) {
        m_playbackTimer.stop();
    }
    if (m_audioSink) {
        m_audioSink->reset();
    }
    if (m_sliceDevice) {
        m_sliceDevice->close();
        m_sliceDevice->deleteLater();
        m_sliceDevice = nullptr;
    }
    m_playbackBuffer.clear();
    if (m_isPlaying) {
        m_isPlaying = false;
        Q_EMIT playbackChanged();
    }
    if (m_videoController && m_videoController->isPlaying()) {
        m_videoController->pause();
    }
}

void AudioController::playSelection()
{
    playRange(m_selectionStart, m_selectionEnd);
}

void AudioController::playCurrentLine()
{
    if (m_videoController && m_videoController->activeSubEnd() > m_videoController->activeSubStart()) {
        playRange(m_videoController->activeSubStart(), m_videoController->activeSubEnd());
    } else {
        playRange(m_selectionStart, m_selectionEnd);
    }
}

void AudioController::play500msBefore()
{
    int start = std::max(0, m_selectionStart - 500);
    int end = m_selectionStart;
    playRange(start, end);
}

void AudioController::play500msAfter()
{
    int start = m_selectionEnd;
    int end = std::min(durationMs(), m_selectionEnd + 500);
    playRange(start, end);
}

void AudioController::playFirst500ms()
{
    int start = m_selectionStart;
    int end = std::min(m_selectionEnd, m_selectionStart + 500);
    playRange(start, end);
}

void AudioController::playLast500ms()
{
    int start = std::max(m_selectionStart, m_selectionEnd - 500);
    int end = m_selectionEnd;
    playRange(start, end);
}

void AudioController::playToEnd()
{
    playRange(m_selectionStart, durationMs());
}

void AudioController::commit()
{
    Q_EMIT committed();
    if (m_autoNext) {
        // Advance to next line upon commit (matches native Auto/Next Line on Commit option).
        Q_EMIT nextLineRequested();
    }
}

void AudioController::leadIn(int deltaMs)
{
    setSelectionStart(std::max(0, m_selectionStart - deltaMs));
}

void AudioController::leadOut(int deltaMs)
{
    setSelectionEnd(std::min(durationMs(), m_selectionEnd + deltaMs));
}

bool AudioController::hasVideo() const
{
    return m_videoController && m_videoController->hasVideo();
}

bool AudioController::isDummyVideo() const
{
    return !m_videoController || m_videoController->isDummy();
}

bool AudioController::drawKeyframes() const
{
    // Follows native AudioMarkerProviderKeyframes::Update preconditions:
    // Requires an authentic keyframe source (video keyframes or loaded keyframe file).
    if (!m_drawKeyframesEnabled) return false;
    if (!m_videoController || m_videoController->isDummy()) return false;
    return m_videoController->hasKeyframes();
}

int AudioController::videoFrame() const
{
    return m_videoController ? m_videoController->currentFrame() : 0;
}

double AudioController::frameTimeMs(int frame) const
{
    if (!m_videoController) return 0.0;
    return m_videoController->frameToTime(frame) * 1000.0;
}

void AudioController::setSubtitleModel(SubtitleModel *model)
{
    if (m_subtitleModel == model) return;
    if (m_subtitleModel) disconnect(m_subtitleModel, nullptr, this, nullptr);
    m_subtitleModel = model;
    if (m_subtitleModel) {
        // Invalidate overlay markers when dialogue line count or order changes.
        connect(m_subtitleModel, &SubtitleModel::countChanged, this, [this]() {
            ++m_overlayRevision;
            Q_EMIT drawOptionsChanged();
        });
    }
    ++m_overlayRevision;
    Q_EMIT drawOptionsChanged();
}

void AudioController::setDrawSeconds(bool val)
{
    if (m_drawSeconds == val) return;
    m_drawSeconds = val;
    ++m_overlayRevision;
    Q_EMIT drawOptionsChanged();
}

void AudioController::setDrawKeyframesEnabled(bool val)
{
    if (m_drawKeyframesEnabled == val) return;
    m_drawKeyframesEnabled = val;
    ++m_overlayRevision;
    Q_EMIT drawOptionsChanged();
}

void AudioController::setDrawVideoPosition(bool val)
{
    if (m_drawVideoPosition == val) return;
    m_drawVideoPosition = val;
    ++m_overlayRevision;
    Q_EMIT drawOptionsChanged();
}

void AudioController::setDrawCursorTime(bool val)
{
    if (m_drawCursorTime == val) return;
    m_drawCursorTime = val;
    ++m_overlayRevision;
    Q_EMIT drawOptionsChanged();
}

void AudioController::setInactiveLinesMode(int mode)
{
    mode = std::clamp(mode, 0, 3);
    if (m_inactiveLinesMode == mode) return;
    m_inactiveLinesMode = mode;
    ++m_overlayRevision;
    Q_EMIT drawOptionsChanged();
}

void AudioController::setLineBoundaryThickness(int px)
{
    px = std::clamp(px, 1, 5);   // Domain of native Audio/Line Boundaries Thickness [1, 5]
    if (m_lineBoundaryThickness == px) return;
    m_lineBoundaryThickness = px;
    ++m_overlayRevision;
    Q_EMIT drawOptionsChanged();
}

void AudioController::setLeadInMs(int ms)
{
    ms = std::clamp(ms, 0, 36000);
    if (m_leadInMs == ms) return;
    m_leadInMs = ms;
    Q_EMIT drawOptionsChanged();
}

void AudioController::setLeadOutMs(int ms)
{
    ms = std::clamp(ms, 0, 36000);
    if (m_leadOutMs == ms) return;
    m_leadOutMs = ms;
    Q_EMIT drawOptionsChanged();
}

QVector<AudioController::LineBoundaryMark> AudioController::inactiveLineBoundaries() const
{
    QVector<LineBoundaryMark> out;
    if (!m_subtitleModel || m_inactiveLinesMode == 0) return out;

    const int rows = m_subtitleModel->rowCount();
    if (rows <= 0) return out;

    // Identify active row: locates the line matching current selection start and end milliseconds.
    int activeRow = -1;
    for (int i = 0; i < rows; ++i) {
        if (m_subtitleModel->getLineStartMs(i) == m_selectionStart
            && m_subtitleModel->getLineEndMs(i) == m_selectionEnd) {
            activeRow = i;
            break;
        }
    }

    auto appendRow = [&](int index) {
        if (index < 0 || index >= rows || index == activeRow) return;
        const QVariantMap line = m_subtitleModel->get(index);
        // Suppress boundaries for comment lines (matches native Audio/Display/Draw/Inactive Comments default).
        if (line.value(QStringLiteral("comment")).toBool()) return;
        LineBoundaryMark mark;
        // Dialogue start/end strings are ASS formatted; fetch raw millisecond integer values.
        mark.startMs = m_subtitleModel->getLineStartMs(index);
        mark.endMs = m_subtitleModel->getLineEndMs(index);
        out.append(mark);
    };

    if (m_inactiveLinesMode == 3 || activeRow < 0) {
        // Mode 3 = all lines; fallback when active row is unresolved.
        for (int i = 0; i < rows; ++i) appendRow(i);
    } else if (m_inactiveLinesMode == 1) {
        appendRow(activeRow - 1);
    } else if (m_inactiveLinesMode == 2) {
        appendRow(activeRow - 1);
        appendRow(activeRow + 1);
    }
    return out;
}

QVector<AudioController::SyllableMark> AudioController::syllableMarks() const
{
    QVector<SyllableMark> out;
    if (!m_karaokeMode || !m_subtitleModel) return out;

    // Fetch text of the currently active dialogue row matching the selection.
    QString text;
    const int rows = m_subtitleModel->rowCount();
    for (int i = 0; i < rows; ++i) {
        if (m_subtitleModel->getLineStartMs(i) == m_selectionStart
            && m_subtitleModel->getLineEndMs(i) == m_selectionEnd) {
            text = m_subtitleModel->get(i).value(QStringLiteral("text")).toString();
            break;
        }
    }
    if (text.isEmpty() || !text.contains(QLatin1Char('\\'))) return out;

    // Parse syllables via native libaegisub algorithm (matches ass_karaoke.cpp).
    const QList<QVariantMap> syls =
        AegisubCoreBridge::parseKaraokeLine(text, m_selectionStart, m_selectionEnd, false);
    if (syls.size() < 2) return out;

    out.reserve(syls.size());
    for (int i = 0; i < syls.size(); ++i) {
        const QVariantMap &s = syls[i];
        SyllableMark mark;
        mark.startMs = s.value(QStringLiteral("startTime")).toInt();
        mark.endMs = mark.startMs + s.value(QStringLiteral("duration")).toInt();
        mark.isFirst = (i == 0);
        mark.text = s.value(QStringLiteral("text")).toString();
        out.append(mark);
    }
    return out;
}

void AudioController::jumpToTime(int timeMs)
{
    if (m_videoController) {
        m_videoController->seekTime(timeMs / 1000.0);
    }
}

void AudioController::updateCursor(int relX)
{
    m_trackCursorMs = static_cast<int>((m_scrollLeft + relX) * m_msPerPixel);
    m_trackCursorText = formatAssTime(m_trackCursorMs);
    Q_EMIT cursorChanged();
}

void AudioController::clearCursor()
{
    if (m_trackCursorMs != -1) {
        m_trackCursorMs = -1;
        m_trackCursorText.clear();
        Q_EMIT cursorChanged();
    }
}

QString AudioController::formatAssTime(int ms) const
{
    int h = ms / 3600000;
    int m = (ms % 3600000) / 60000;
    int s = (ms % 60000) / 1000;
    int cs = (ms % 1000) / 10;
    return QString("%1:%2:%3.%4")
        .arg(h)
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(cs, 2, 10, QChar('0'));
}

void AudioController::applyFreqCurve(int curve)
{
    // Reference frequency normalized vertical coordinate from Aegisub audio_display.cpp.
    static const float spectrumFrefPos[5] = { 0.001f, 0.125f, 0.333f, 0.425f, 0.999f };
    curve = std::clamp(curve, 0, 4);
    m_freqCurve = curve;
    m_stftCore.posFref = spectrumFrefPos[curve];
    Q_EMIT audioInfoChanged();
}

void AudioController::loadAudio(const QString &path, int targetSampleRate)
{
    stop();

    QString clean = path;
    if (clean.startsWith(QLatin1String("file:///"))) clean = QUrl(clean).toLocalFile();
    else if (clean.startsWith(QLatin1String("file://"))) clean = QUrl(clean).toLocalFile();

    if (clean.isEmpty() || !QFile::exists(clean)) {
        qWarning() << "[AudioController] loadAudio failed: file does not exist:" << clean;
        return;
    }

    const uint64_t reqId = ++m_audioLoadRequestId;
    m_isLoadingAudio = true;
    Q_EMIT audioInfoChanged();

    // Offload audio demuxing, decoding, disk caching, and envelope calculation to thread pool
    // to keep the Qt GUI thread running smoothly at 60 FPS without stutter or freeze.
    // Guard async worker: AudioController may be destroyed while decoding in thread pool.
    // Worker operates strictly on thread-local data and verifies guarded pointer on GUI thread.
    QPointer<AudioController> guard(this);
    QThreadPool::globalInstance()->start(QRunnable::create([guard, reqId, clean, targetSampleRate]() {
        AudioPcmProvider localProvider;
        // Explicitly report decode failure instead of falling back to silent synthetic audio.
        const bool ok = localProvider.loadAudioFile(clean, targetSampleRate);

        const auto &samples = localProvider.samples();
        const int sr = localProvider.sampleRate();
        QVector<float> peaks;
        if (sr > 0 && !samples.empty()) {
            const int samplesPerBucket = sr / 100; // 10ms per bucket
            const int totalBuckets = static_cast<int>(samples.size() / samplesPerBucket);
            peaks.resize(totalBuckets);
            for (int b = 0; b < totalBuckets; ++b) {
                float maxVal = 0.0f;
                const size_t startIdx = static_cast<size_t>(b) * samplesPerBucket;
                const size_t endIdx = std::min(samples.size(), startIdx + samplesPerBucket);
                for (size_t i = startIdx; i < endIdx; ++i) {
                    const float v = std::abs(static_cast<int>(samples[i])) / 32768.0f;
                    if (v > maxVal) maxVal = v;
                }
                peaks[b] = maxVal;
            }
        }

        AudioController *self = guard.data();
        if (!self) {
            return; // Controller destroyed during decode; discard result.
        }
        QMetaObject::invokeMethod(self, [self, reqId, clean, ok, provider = std::move(localProvider), peaks = std::move(peaks)]() mutable {
            if (self->m_audioLoadRequestId.load() != reqId) {
                return; // Discard superseded audio loading job
            }

            if (!ok) {
                // Decode failure: clear loading state and report error without corrupting subtitle UI state.
                self->m_isLoadingAudio = false;
                self->m_hasAudio = false;
                self->m_audioPath.clear();
                self->m_audioDuration = 0.0;
                Q_EMIT self->audioInfoChanged();
                Q_EMIT self->audioError(QStringLiteral("Failed to load audio: %1").arg(clean));
                return;
            }

            self->m_pcmProvider = std::move(provider);
            self->m_peaks = std::move(peaks);
            self->m_hasAudio = self->m_pcmProvider.isLoaded();
            self->m_audioPath = clean;
            self->m_audioDuration = self->m_pcmProvider.duration();
            self->m_stftCore.processAudio(self->m_pcmProvider);
            self->m_isLoadingAudio = false;

            Q_EMIT self->audioInfoChanged();
            Q_EMIT self->playbackChanged();
        }, Qt::QueuedConnection);
    }));
}

void AudioController::openAudio(const QString &path)
{
    QString clean = path;
    if (clean.startsWith(QLatin1String("file:///"))) clean = QUrl(clean).toLocalFile();
    else if (clean.startsWith(QLatin1String("file://"))) clean = QUrl(clean).toLocalFile();
    loadAudio(clean, 0);
}

bool AudioController::openAudioFromVideo(const QString &customVideoPath)
{
    QString vPath = customVideoPath;
    if (vPath.isEmpty() && m_videoController && m_videoController->hasVideo()) {
        vPath = m_videoController->videoPath();
    }
    QString clean = vPath;
    if (clean.startsWith(QLatin1String("file:///"))) clean = QUrl(clean).toLocalFile();
    else if (clean.startsWith(QLatin1String("file://"))) clean = QUrl(clean).toLocalFile();

    if (clean.isEmpty() || !QFile::exists(clean)) {
        qWarning() << "[AudioController] openAudioFromVideo failed: media file not found:" << clean;
        return false;
    }

    loadAudio(clean, 0);
    return true;
}

bool AudioController::openBlankAudio()
{
    return openVirtualAudio(AudioPcmProvider::VirtualKind::Blank);
}

bool AudioController::openNoiseAudio()
{
    return openVirtualAudio(AudioPcmProvider::VirtualKind::Noise);
}

bool AudioController::openVirtualAudio(AudioPcmProvider::VirtualKind kind)
{
    if (kind == AudioPcmProvider::VirtualKind::None) return false;
    stop();

    const uint64_t reqId = ++m_audioLoadRequestId;

    AudioPcmProvider provider;
    if (!provider.loadVirtualAudio(kind)) return false;

    // Synthesized waveform peak envelope: blank is pure silence; uniform white noise
    // peaks near full scale for every 10ms bucket (mirrors the real-file envelope builder).
    QVector<float> peaks;
    const int64_t bucketCount = static_cast<int64_t>(provider.duration() * 100.0);
    const int64_t bucketCap = 2'000'000; // hard safety cap (~5.5h envelope)
    if (bucketCount > 0) {
        const int clamped = static_cast<int>(std::min<int64_t>(bucketCount, bucketCap));
        peaks.resize(clamped);
        peaks.fill(kind == AudioPcmProvider::VirtualKind::Noise ? 0.95f : 0.0f);
    }

    m_pcmProvider = std::move(provider);
    m_peaks = std::move(peaks);
    m_hasAudio = m_pcmProvider.isLoaded();
    m_audioPath = (kind == AudioPcmProvider::VirtualKind::Noise)
        ? QStringLiteral("virtual://noise")
        : QStringLiteral("virtual://blank");
    m_audioDuration = m_pcmProvider.duration();
    m_isLoadingAudio = false;
    (void)reqId; // Synchronous virtual load supersedes any queued decode jobs.
    m_stftCore.processAudio(m_pcmProvider);

    Q_EMIT audioInfoChanged();
    Q_EMIT playbackChanged();
    return true;
}

void AudioController::closeAudio()
{
    ++m_audioLoadRequestId;
    m_isLoadingAudio = false;
    stop();
    delete m_sliceDevice;
    m_sliceDevice = nullptr;
    delete m_audioSink;
    m_audioSink = nullptr;
    m_hasAudio = false;
    m_audioPath.clear();
    m_audioDuration = 0.0;
    m_peaks.clear();
    m_pcmProvider.reset();
    Q_EMIT audioInfoChanged();
    Q_EMIT playbackChanged();
}

QVariantList AudioController::getWaveformPeaks(int startMs, int endMs, int pixelWidth)
{
    QVariantList res;
    if (pixelWidth <= 0) return res;

    double msPerPixel = static_cast<double>(endMs - startMs) / pixelWidth;
    for (int x = 0; x < pixelWidth; ++x) {
        double curMs = startMs + x * msPerPixel;
        int bucket = static_cast<int>(curMs / 10.0);
        float peak = 0.0f;
        if (bucket >= 0 && bucket < m_peaks.size()) {
            peak = m_peaks[bucket];
        } else {
            peak = 0.02f;
        }
        float avg = peak * 0.55f;
        QVariantMap map;
        map["peak"] = peak;
        map["avg"] = avg;
        res.append(map);
    }
    return res;
}


