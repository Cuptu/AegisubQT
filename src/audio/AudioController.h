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
#include <QVector>
#include <QVariantList>
#include <QTimer>
#include <QElapsedTimer>
#include <QPointer>
#include <atomic>
#include "VideoController.h"
#include "AstraCoreBridge.h"
#include "AudioPcmProvider.h"
#include "AegisubStftCore.h"
#include "SubtitleModel.h"

class QAudioSink;
class QIODevice;
class AudioSliceDevice;

class AudioController : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY audioInfoChanged)
    Q_PROPERTY(bool isLoadingAudio READ isLoadingAudio NOTIFY audioInfoChanged)
    Q_PROPERTY(QString audioPath READ audioPath NOTIFY audioInfoChanged)
    // Effective timeline duration: returns audio duration when loaded, falling back to video duration.
    Q_PROPERTY(double duration READ duration NOTIFY audioInfoChanged)
    Q_PROPERTY(double audioDuration READ audioDuration NOTIFY audioInfoChanged)
    Q_PROPERTY(double videoDuration READ videoDuration NOTIFY audioInfoChanged)
    Q_PROPERTY(int durationMs READ durationMs NOTIFY audioInfoChanged)
    Q_PROPERTY(double msPerPixel READ msPerPixel NOTIFY zoomChanged)
    Q_PROPERTY(int zoomLevel READ zoomLevel WRITE setZoomLevel NOTIFY zoomChanged)
    Q_PROPERTY(int scrollLeft READ scrollLeft WRITE scrollTo NOTIFY scrollChanged)
    Q_PROPERTY(int selectionStart READ selectionStart WRITE setSelectionStart NOTIFY selectionChanged)
    Q_PROPERTY(int selectionEnd READ selectionEnd WRITE setSelectionEnd NOTIFY selectionChanged)
    Q_PROPERTY(int selectionDuration READ selectionDuration NOTIFY selectionChanged)
    Q_PROPERTY(double currentTime READ currentTime NOTIFY positionChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playbackChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(int verticalZoom READ verticalZoom WRITE setVerticalZoom NOTIFY verticalZoomChanged)
    Q_PROPERTY(double amplitudeScale READ amplitudeScale NOTIFY verticalZoomChanged)
    Q_PROPERTY(bool linkVolumeAndZoom READ linkVolumeAndZoom WRITE setLinkVolumeAndZoom NOTIFY linkChanged)
    Q_PROPERTY(bool spectrumMode READ spectrumMode WRITE setSpectrumMode NOTIFY spectrumModeChanged)
    Q_PROPERTY(bool autoScroll READ autoScroll WRITE setAutoScroll NOTIFY autoScrollChanged)
    Q_PROPERTY(bool autoCommit READ autoCommit WRITE setAutoCommit NOTIFY autoCommitChanged)
    Q_PROPERTY(bool autoNext READ autoNext WRITE setAutoNext NOTIFY autoNextChanged)
    Q_PROPERTY(bool karaokeMode READ karaokeMode WRITE setKaraokeMode NOTIFY karaokeModeChanged)
    Q_PROPERTY(bool medusaMode READ medusaMode WRITE setMedusaMode NOTIFY medusaModeChanged)
    Q_PROPERTY(double playbackSpeed READ playbackSpeed WRITE setPlaybackSpeed NOTIFY playbackSpeedChanged)
    Q_PROPERTY(int trackCursorMs READ trackCursorMs NOTIFY cursorChanged)
    Q_PROPERTY(QString trackCursorText READ trackCursorText NOTIFY cursorChanged)
    Q_PROPERTY(QVariantList keyframes READ keyframes NOTIFY keyframesChanged)
    Q_PROPERTY(bool drawSeconds READ drawSeconds WRITE setDrawSeconds NOTIFY drawOptionsChanged)
    Q_PROPERTY(bool drawKeyframesEnabled READ drawKeyframesEnabled WRITE setDrawKeyframesEnabled NOTIFY drawOptionsChanged)
    Q_PROPERTY(bool drawVideoPosition READ drawVideoPosition WRITE setDrawVideoPosition NOTIFY drawOptionsChanged)
    Q_PROPERTY(bool drawCursorTime READ drawCursorTime WRITE setDrawCursorTime NOTIFY drawOptionsChanged)
    Q_PROPERTY(int inactiveLinesMode READ inactiveLinesMode WRITE setInactiveLinesMode NOTIFY drawOptionsChanged)
    Q_PROPERTY(int lineBoundaryThickness READ lineBoundaryThickness WRITE setLineBoundaryThickness NOTIFY drawOptionsChanged)
    Q_PROPERTY(int leadInMs READ leadInMs WRITE setLeadInMs NOTIFY drawOptionsChanged)
    Q_PROPERTY(int leadOutMs READ leadOutMs WRITE setLeadOutMs NOTIFY drawOptionsChanged)

public:
    explicit AudioController(VideoController *videoController, QObject *parent = nullptr);
    ~AudioController() override;

    bool hasAudio() const { return m_hasAudio; }
    bool isLoadingAudio() const { return m_isLoadingAudio; }
    QString audioPath() const { return m_audioPath; }
    // Explicit timeline durations: audio and video tracks tracked separately; duration yields the active source.
    double duration() const { return m_hasAudio ? m_audioDuration : videoDuration(); }
    double audioDuration() const { return m_audioDuration; }
    double videoDuration() const { return m_videoController ? m_videoController->duration() : 0.0; }
    int durationMs() const { return static_cast<int>(duration() * 1000.0); }
    double msPerPixel() const { return m_msPerPixel; }
    int zoomLevel() const { return m_zoomLevel; }
    int scrollLeft() const { return m_scrollLeft; }
    int selectionStart() const { return m_selectionStart; }
    int selectionEnd() const { return m_selectionEnd; }
    int selectionDuration() const { return std::max(0, m_selectionEnd - m_selectionStart); }
    double currentTime() const { return m_currentTime; }
    bool isPlaying() const { return m_isPlaying; }
    int volume() const { return m_volume; }
    int verticalZoom() const { return m_verticalZoom; }
    double amplitudeScale() const { return m_amplitudeScale; }
    bool linkVolumeAndZoom() const { return m_linkVolumeAndZoom; }
    bool spectrumMode() const { return m_spectrumMode; }
    bool autoScroll() const { return m_autoScroll; }
    bool autoCommit() const { return m_autoCommit; }
    bool autoNext() const { return m_autoNext; }
    bool karaokeMode() const { return m_karaokeMode; }
    bool medusaMode() const { return m_medusaMode; }
    double playbackSpeed() const { return m_playbackSpeed; }
    int trackCursorMs() const { return m_trackCursorMs; }
    QString trackCursorText() const { return m_trackCursorText; }
    QVariantList keyframes() const;

    // Timeline information for audio overlay rendering (aligns with native VideoPositionMarkerProvider and keyframe providers).
    bool hasVideo() const;
    bool isDummyVideo() const;
    // Native Aegisub only renders keyframes when authentic keyframes exist (from video provider or keyframe file),
    // guarded by Audio/Display/Draw/Keyframes in Dialogue Mode.
    bool drawKeyframes() const;
    bool drawKeyframesEnabled() const { return m_drawKeyframesEnabled; }
    int videoFrame() const;
    // Returns start time of frame number; linearly extrapolates when out of bounds (matches native TimeAtFrame).
    double frameTimeMs(int frame) const;

    // Overlay rendering flags matching native Aegisub options.
    // Audio/Display/Draw/{Seconds,Video Position,Cursor Time,Keyframes,Inactive Comments}
    bool drawSeconds() const { return m_drawSeconds; }
    bool drawVideoPosition() const { return m_drawVideoPosition; }
    bool drawCursorTime() const { return m_drawCursorTime; }
    bool drawInactiveLines() const { return m_inactiveLinesMode != 0; }
    // Audio/Inactive Lines Display Mode: 0 = disabled, 1 = previous line only, 2 = previous and next, 3 = all lines.
    int inactiveLinesMode() const { return m_inactiveLinesMode; }
    // Audio/Line Boundaries Thickness (default 2).
    int lineBoundaryThickness() const { return m_lineBoundaryThickness; }
    // Audio/Lead/IN and Audio/Lead/OUT margins in milliseconds (default 100 / 350).
    int leadInMs() const { return m_leadInMs; }
    int leadOutMs() const { return m_leadOutMs; }
    // Overlay revision counter incremented on line/toggle changes for scene graph cache invalidation.
    int overlayRevision() const { return m_overlayRevision; }

    void setSubtitleModel(SubtitleModel *model);

    // Viewport width in logical pixels reported by AudioDisplayController::setViewportSize,
    // utilized for auto-scrolling and scrollRangeInView.
    void setViewportWidth(int width);
    int viewportWidth() const { return m_viewportWidth; }

    // Inactive dialogue line boundaries (gray markers), filtered following native RegenerateInactiveLines.
    struct LineBoundaryMark { int startMs = 0; int endMs = 0; };
    QVector<LineBoundaryMark> inactiveLineBoundaries() const;

    // Karaoke syllable markers and labels; non-empty only when karaoke mode is active and the dialogue line contains \k tags.
    // Native Aegisub creates one label per syllable and a boundary delimiter for every syllable after the first.
    struct SyllableMark { int startMs = 0; int endMs = 0; bool isFirst = false; QString text; };
    QVector<SyllableMark> syllableMarks() const;
    bool hasSyllables() const { return !syllableMarks().isEmpty(); }

    const AudioPcmProvider& pcmProvider() const { return m_pcmProvider; }
    const AegisubStftCore& stftCore() const { return m_stftCore; }
    AudioPcmProvider& pcmProvider() { return m_pcmProvider; }
    AegisubStftCore& stftCore() { return m_stftCore; }

public Q_SLOTS:
    void setSelection(int startMs, int endMs);
    void setSelectionStart(int startMs);
    void setSelectionEnd(int endMs);
    void shiftSelection(int deltaMs);
    void scrollBy(int deltaPx);
    void scrollTo(int pixelPos);
    void scrollTimeToCenter(int timeMs);
    // Falls back to reported viewport width when viewportWidth <= 0.
    void scrollRangeInView(int startMs, int endMs, int viewportWidth = -1);
    void setZoomLevel(int level);
    void setMsPerPixel(double val);
    void setVolume(int vol);
    void setVerticalZoom(int zoom);
    void setLinkVolumeAndZoom(bool link);
    void setSpectrumMode(bool val);
    void toggleSpectrumMode();
    void setAutoScroll(bool val);
    void setAutoCommit(bool val);
    void setAutoNext(bool val);
    void setKaraokeMode(bool val);
    void setMedusaMode(bool val);
    void toggleMedusaMode();
    void setDrawSeconds(bool val);
    void setDrawKeyframesEnabled(bool val);
    void setDrawVideoPosition(bool val);
    void setDrawCursorTime(bool val);
    void setInactiveLinesMode(int mode);
    void setLineBoundaryThickness(int px);
    void setLeadInMs(int ms);
    void setLeadOutMs(int ms);
    void setPlaybackSpeed(double speed);

    void playSelection();
    void playCurrentLine();
    void play500msBefore();
    void play500msAfter();
    void playFirst500ms();
    void playLast500ms();
    void playToEnd();
    void playRange(int startMs, int endMs);
    void stop();
    void commit();
    void leadIn(int deltaMs = 200);
    void leadOut(int deltaMs = 200);
    void jumpToTime(int timeMs);

    void updateCursor(int relX);
    void clearCursor();

    QString formatAssTime(int ms) const;
    // targetSampleRate = 0 preserves native source rate (matching Aegisub ffms2 provider).
    void loadAudio(const QString &path, int targetSampleRate = 0);
    void openAudio(const QString &path);
    bool openAudioFromVideo(const QString &customVideoPath = QString());
    // Upstream "Open 2h30 Blank/Noise Audio": on-demand synthesized virtual providers.
    bool openBlankAudio();
    bool openNoiseAudio();
    void closeAudio();
    // Maps to Audio/Renderer/Spectrum/FreqCurve: 0=Linear ... 4=Logarithmic.
    void applyFreqCurve(int curve);

    // Viewport waveform peak sampling interface for QML Canvas.
    QVariantList getWaveformPeaks(int startMs, int endMs, int pixelWidth);

Q_SIGNALS:
    void audioInfoChanged();
    void zoomChanged();
    void scrollChanged();
    void selectionChanged();
    void positionChanged();
    void playbackChanged();
    void volumeChanged();
    void verticalZoomChanged();
    void linkChanged();
    void spectrumModeChanged();
    void autoScrollChanged();
    void autoCommitChanged();
    void autoNextChanged();
    void karaokeModeChanged();
    void medusaModeChanged();
    void drawOptionsChanged();
    void playbackSpeedChanged();
    void cursorChanged();
    void keyframesChanged();
    void committed();
    /// Emitted on commit when autoNext is enabled to advance to the next line across both button and hotkey paths.
    void nextLineRequested();
    /// Emitted upon audio load failure for user-facing status bar error reporting.
    void audioError(const QString &message);

private:
    static int getZoomLevelFactor(int level);
    void updateAmplitudeScale();
    // Synchronous virtual (blank/noise) audio loading shared by openBlank/openNoise.
    bool openVirtualAudio(AudioPcmProvider::VirtualKind kind);
    void setupAudioSink(int sampleRate);
    void onPlaybackTimerTick();

    VideoController *m_videoController = nullptr;
    SubtitleModel *m_subtitleModel = nullptr;

    // Overlay drawing flags initialized from native default_config.json.
    bool m_drawSeconds = true;          // Audio/Display/Draw/Seconds
    bool m_drawKeyframesEnabled = true; // Audio/Display/Draw/Keyframes in Dialogue Mode
    bool m_drawVideoPosition = true;    // Audio/Display/Draw/Video Position
    bool m_drawCursorTime = true;       // Audio/Display/Draw/Cursor Time
    int m_inactiveLinesMode = 3;        // Audio/Inactive Lines Display Mode
    int m_lineBoundaryThickness = 2;    // Audio/Line Boundaries Thickness
    int m_leadInMs = 100;               // Audio/Lead/IN
    int m_leadOutMs = 350;              // Audio/Lead/OUT
    int m_overlayRevision = 0;

    bool m_hasAudio = false;
    QString m_audioPath;
    double m_audioDuration = 0.0;   // Loaded audio duration in seconds.
    double m_msPerPixel = 20.0;
    int m_viewportWidth = 800;      // Display viewport width in logical pixels.
    int m_zoomLevel = 0;
    int m_scrollLeft = 0;
    int m_selectionStart = 2250;
    int m_selectionEnd = 7250;
    double m_currentTime = 0.0;
    bool m_isPlaying = false;
    double m_playbackSpeed = 1.0;
    std::vector<int16_t> m_playbackBuffer;

    QAudioSink *m_audioSink = nullptr;
    // Streams a sample slice to QAudioSink: either an AudioSliceDevice over real PCM
    // or a VirtualAudioSliceDevice for blank/noise audio.
    QIODevice *m_sliceDevice = nullptr;
    QTimer m_playbackTimer;
    QElapsedTimer m_fallbackTimer;
    int m_playbackStartMs = 0;
    int m_playbackEndMs = 0;

    int m_volume = 50;
    int m_verticalZoom = 50;
    double m_amplitudeScale = 1.0;
    bool m_linkVolumeAndZoom = true;
    bool m_spectrumMode = true;
    bool m_autoScroll = true;
    bool m_autoCommit = false;
    bool m_autoNext = false;
    bool m_karaokeMode = false;
    bool m_medusaMode = false;

    int m_trackCursorMs = -1;
    QString m_trackCursorText;
    int m_freqCurve = 0;

    std::atomic<uint64_t> m_audioLoadRequestId{0};
    bool m_isLoadingAudio = false;

    QVector<float> m_peaks; // Downsampled peak envelope cache.
    AudioPcmProvider m_pcmProvider;
    AegisubStftCore m_stftCore;
};
