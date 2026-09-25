// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Aegisub Project http://www.aegisub.org/

#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QLibrary>
#include <QImage>

/// Media stream metadata probed from multimedia containers.
struct MediaInfo {
    double duration = 0.0;
    int64_t bitRate = 0;
    int width = 0;
    int height = 0;
    double fps = 23.976;
    bool hasVideo = false;
    bool hasAudio = false;
};

/// Dynamic FFI bridge to AstraCore.Native.dll providing hardware-accelerated
/// media probing, audio demuxing, and waveform peak extraction.
class AstraCoreBridge : public QObject {
    Q_OBJECT
public:
    explicit AstraCoreBridge(QObject *parent = nullptr);
    ~AstraCoreBridge();

    static AstraCoreBridge* instance();

    bool isAvailable() const { return m_available; }
    uint32_t abiVersion();
    bool probe(const QString &filePath, MediaInfo &info);
    QVector<float> extractWaveformPeaks(const QString &filePath, int targetSampleRate = 4000, int samplesPerPeak = 100, int maxPeaks = 100000, double *outDuration = nullptr);
    bool extractAudioWav(const QString &inputPath, const QString &outputWavPath, int targetSampleRate = 16000, int channels = 1);
    QVector<double> extractKeyframes(const QString &filePath, QVector<int64_t> *outFrameIndices = nullptr, int maxKeyframes = 50000);
    QVector<float> extractSpectrogram(const QString &filePath, int targetSampleRate = 16000, int nFft = 512, int hopSize = 256, int *outNumBins = nullptr, int maxFrames = 10000);
    QVector<double> extractTimecodes(const QString &inputPath, const QString &outputTimecodesPath = QString(), int maxFrames = 500000);
    QImage grabFrameImage(const QString &filePath, double targetSeconds, int targetWidth = 0, int targetHeight = 0, double *outActualSeconds = nullptr);
    bool probeHdr(const QString &filePath, bool &isHdr, int &bitDepth, int &colorPrimaries, int &colorTransfer);

    // Persistent video session methods for low-latency scrubbing
    void* openVideoSession(const QString &filePath);
    QImage grabSessionFrame(void *session, double targetSeconds, int targetWidth = 0, int targetHeight = 0, double *outActualSeconds = nullptr);
    void closeVideoSession(void *session);

    // Media editing and audio tempo adjustments
    bool changeAudioSpeed(const QString &inputPath, const QString &outputPath, double speedFactor);
    bool trimMedia(const QString &inputPath, const QString &outputPath, double startSeconds, double durationSeconds, bool streamCopy = true);

private:
    void loadLibrary();

    bool m_available = false;
    QLibrary m_lib;

    // Native C FFI function signatures
    typedef uint32_t (*ac_abi_version_fn)();
    typedef int (*ac_check_encoder_fn)(const char *);
    typedef int (*ac_probe_utf8_fn)(const char *, void *, char *, size_t);
    typedef int (*ac_extract_waveform_peaks_utf8_fn)(const char *, int, int, float *, int, double *, char *, size_t);
    typedef int (*ac_extract_audio_wav_utf8_fn)(const char *, const char *, int, int, char *, size_t);
    typedef int (*ac_extract_keyframes_utf8_fn)(const char *, double *, int64_t *, int, char *, size_t);
    typedef int (*ac_extract_spectrogram_utf8_fn)(const char *, int, int, int, float *, int, int *, double *, char *, size_t);
    typedef int (*ac_extract_timecodes_utf8_fn)(const char *, const char *, double *, int, char *, size_t);
    typedef int (*ac_grab_frame_image_utf8_fn)(const char *, double, int, int, int, uint8_t *, size_t, int *, int *, double *, char *, size_t);
    typedef int (*ac_probe_hdr_utf8_fn)(const char *, void *, char *, size_t);
    typedef void* (*ac_video_open_session_utf8_fn)(const char *, char *, size_t);
    typedef int (*ac_video_session_grab_frame_fn)(void *, double, int, int, int, uint8_t *, size_t, int *, int *, double *, char *, size_t);
    typedef void (*ac_video_close_session_fn)(void *);
    typedef int (*ac_change_audio_speed_utf8_fn)(const char *, const char *, double, char *, size_t);
    typedef int (*ac_trim_media_utf8_fn)(const char *, const char *, double, double, int, char *, size_t);

    ac_abi_version_fn m_fn_abi_version = nullptr;
    ac_check_encoder_fn m_fn_check_encoder = nullptr;
    ac_probe_utf8_fn m_fn_probe_utf8 = nullptr;
    ac_extract_waveform_peaks_utf8_fn m_fn_extract_waveform_peaks_utf8 = nullptr;
    ac_extract_audio_wav_utf8_fn m_fn_extract_audio_wav_utf8 = nullptr;
    ac_extract_keyframes_utf8_fn m_fn_extract_keyframes_utf8 = nullptr;
    ac_extract_spectrogram_utf8_fn m_fn_extract_spectrogram_utf8 = nullptr;
    ac_extract_timecodes_utf8_fn m_fn_extract_timecodes_utf8 = nullptr;
    ac_grab_frame_image_utf8_fn m_fn_grab_frame_image_utf8 = nullptr;
    ac_probe_hdr_utf8_fn m_fn_probe_hdr_utf8 = nullptr;
    ac_video_open_session_utf8_fn m_fn_video_open_session_utf8 = nullptr;
    ac_video_session_grab_frame_fn m_fn_video_session_grab_frame = nullptr;
    ac_video_close_session_fn m_fn_video_close_session = nullptr;
    ac_change_audio_speed_utf8_fn m_fn_change_audio_speed_utf8 = nullptr;
    ac_trim_media_utf8_fn m_fn_trim_media_utf8 = nullptr;
};

