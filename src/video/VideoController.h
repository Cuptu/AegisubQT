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
#include <QTimer>
#include <QSet>
#include <memory>
#include "AstraCoreBridge.h"
#include "VideoProvider.h"

/// Controls video playback, frame navigation, keyframe indexing, and subtitle synchronization.
class VideoController : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY videoInfoChanged)
    Q_PROPERTY(QString videoPath READ videoPath NOTIFY videoInfoChanged)
    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY videoInfoChanged)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY videoInfoChanged)
    Q_PROPERTY(double fps READ fps NOTIFY videoInfoChanged)
    Q_PROPERTY(double duration READ duration NOTIFY videoInfoChanged)
    Q_PROPERTY(int totalFrames READ totalFrames NOTIFY videoInfoChanged)
    Q_PROPERTY(double currentTime READ currentTime NOTIFY positionChanged)
    Q_PROPERTY(int currentFrame READ currentFrame NOTIFY positionChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playbackStateChanged)
    Q_PROPERTY(bool isKeyframe READ isKeyframe NOTIFY positionChanged)
    Q_PROPERTY(QString timeAndFrameString READ timeAndFrameString NOTIFY positionChanged)
    Q_PROPERTY(QString relativeTimeString READ relativeTimeString NOTIFY positionChanged)
    Q_PROPERTY(QString dummyColor READ dummyColor NOTIFY videoInfoChanged)
    Q_PROPERTY(bool isDummy READ isDummy NOTIFY videoInfoChanged)
    Q_PROPERTY(QString zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(QString activeSubText READ activeSubText NOTIFY subtitleSyncChanged)
    Q_PROPERTY(bool autoScroll READ autoScroll WRITE setAutoScroll NOTIFY videoInfoChanged)
    Q_PROPERTY(QString frameImageSource READ frameImageSource NOTIFY frameImageChanged)
    Q_PROPERTY(QVariantList keyframeList READ keyframeList NOTIFY keyframesChanged)

    Q_PROPERTY(bool hasKeyframes READ hasKeyframes NOTIFY keyframesChanged)
    Q_PROPERTY(bool hasCustomKeyframes READ hasCustomKeyframes NOTIFY keyframesChanged)
    Q_PROPERTY(QString keyframesPath READ keyframesPath NOTIFY keyframesChanged)
    Q_PROPERTY(bool hasTimecodes READ hasTimecodes NOTIFY timecodesChanged)
    Q_PROPERTY(QString timecodesPath READ timecodesPath NOTIFY timecodesChanged)

public:
    explicit VideoController(QObject *parent = nullptr);

    bool hasVideo() const { return m_hasVideo; }
    QString videoPath() const { return m_videoPath; }
    QString frameImageSource() const { return m_frameImageSource; }
    /// Raw frame image buffer uploaded directly as a Scene Graph texture by VideoSurfaceItem.
    const QImage& currentFrameImage() const { return m_currentFrameImage; }
    int videoWidth() const { return m_width; }
    int videoHeight() const { return m_height; }
    double fps() const { return m_fps; }
    double duration() const { return m_duration; }
    /// Switches to low-resolution decoding during scrubbing; restores full resolution on release.
    Q_INVOKABLE void setScrubbing(bool on);

    int totalFrames() const { return m_totalFrames; }
    double currentTime() const { return m_currentTime; }
    int currentFrame() const { return m_currentFrame; }
    bool isPlaying() const { return m_isPlaying; }
    bool isKeyframe() const { return m_keyframes.contains(m_currentFrame); }
    QString timeAndFrameString() const;
    QString relativeTimeString() const;
    QString dummyColor() const { return m_dummyColor; }
    bool isDummy() const { return m_isDummy; }
    QString zoom() const { return m_zoom; }
    QString activeSubText() const { return m_activeSubText; }
    bool autoScroll() const { return m_autoScroll; }

    int activeSubStart() const { return m_activeSubStart; }
    int activeSubEnd() const { return m_activeSubEnd; }
    const QSet<int>& keyframes() const { return m_keyframes; }
    QVariantList keyframeList() const;

    bool hasKeyframes() const { return !m_keyframes.isEmpty(); }
    bool hasCustomKeyframes() const { return m_hasCustomKeyframes; }
    QString keyframesPath() const { return m_keyframesPath; }
    bool hasTimecodes() const { return m_hasTimecodes; }
    QString timecodesPath() const { return m_timecodesPath; }
    const QVector<double>& timecodes() const { return m_timecodes; }

    double frameToTime(int frame) const;
    int timeToFrame(double sec) const;

    VideoProvider* provider() const { return m_provider.get(); }

public Q_SLOTS:
    void setZoom(const QString &z);
    void setAutoScroll(bool val);
    void setActiveSubtitle(int startMs, int endMs, const QString &text);
    void parseAndSetActiveSubtitle(const QString &startStr, const QString &endStr, const QString &text);
    void play();
    void pause();
    void togglePlay();
    void playCurrentLine();
    void seekFrame(int frame);
    void seekTime(double sec);
    void stepFrame(int delta);
    void openVideoFile(const QString &path);
    void openVideo(const QString &path);
    void openDummyVideo(double fps, int frameCount, int width, int height, const QString &color);
    void closeVideo();
    void copyFrame(bool raw = false, bool withSubs = false);
    void saveFrame(bool raw = false, bool withSubs = false);
    void copyCoordinates(int x, int y);

    bool openKeyframesFile(const QString &path);
    bool saveKeyframesFile(const QString &path);
    void closeKeyframes();
    bool loadKeyframes(const QString &path) { return openKeyframesFile(path); }

    bool openTimecodesFile(const QString &path);
    bool saveTimecodesFile(const QString &path);
    void closeTimecodesFile();
    bool loadTimecodes(const QString &path) { return openTimecodesFile(path); }
    void closeTimecodes() { closeTimecodesFile(); }

    Q_INVOKABLE bool exportClip(const QString &outputPath, int startMs, int endMs, bool streamCopy = true);

Q_SIGNALS:
    void positionChanged();
    void playbackStateChanged();
    void videoInfoChanged();
    void subtitleSyncChanged();
    void frameImageChanged();
    void keyframesChanged();
    void timecodesChanged();
    void zoomChanged();
    /// Emitted when video open or decode encounters an error, triggering UI status notifications.
    void videoError(const QString &message);

private Q_SLOTS:
    void onPlaybackTick();
    void onProviderFrameReady(int frameNumber, const QImage &image, double actualPts);
    void onProviderKeyframesReady(const QVector<int64_t> &indices, const QVector<double> &timestamps);
    void onProviderTimecodesReady(const QVector<double> &timecodes);
    void onProviderError(const QString &errorMessage);

private:
    QString formatTime(double seconds) const;
    int parseAssTime(const QString &str) const;

    std::unique_ptr<VideoProvider> m_provider;

    bool m_hasVideo = false;
    QString m_videoPath;
    QString m_frameImageSource;
    QImage m_currentFrameImage;
    uint64_t m_frameRevision = 0;
    int m_width = 640;
    int m_height = 480;
    double m_fps = 23.976;
    double m_duration = 0.0;
    int m_totalFrames = 0;
    int m_currentFrame = 0;
    int m_lastRequestedFrame = -1;
    double m_currentTime = 0.0;
    bool m_isPlaying = false;
    bool m_isDummy = false;
    QString m_dummyColor = "#000000";
    QString m_zoom = "25%";
    bool m_autoScroll = true;

    QSet<int> m_keyframes;
    QSet<int> m_defaultKeyframes;
    bool m_hasCustomKeyframes = false;
    QString m_keyframesPath;

    bool m_hasTimecodes = false;
    QString m_timecodesPath;
    QVector<double> m_timecodes;

    int m_activeSubStart = 0;
    int m_activeSubEnd = 5000;
    QString m_activeSubText;

    QTimer m_timer;
    bool m_playLineMode = false;
    double m_playLineEndTime = 0.0;
};
