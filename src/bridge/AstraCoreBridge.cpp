// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Aegisub Project http://www.aegisub.org/

#include "AstraCoreBridge.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <limits>
#if defined(Q_OS_WIN)
#define NOMINMAX
#include <windows.h>
#endif

// 8-byte alignment contract matching AstraCore.Native ABI
#pragma pack(push, 8)
struct NativeMediaInfo {
    uint32_t struct_size;
    double duration_seconds;
    int64_t bit_rate;
    int32_t width;
    int32_t height;
    double frame_rate;
    int32_t has_video;
    int32_t has_audio;
};
#pragma pack(pop)

AstraCoreBridge::AstraCoreBridge(QObject *parent)
    : QObject(parent)
{
    loadLibrary();
}

AstraCoreBridge::~AstraCoreBridge()
{
    if (m_lib.isLoaded()) {
        m_lib.unload();
    }
}

AstraCoreBridge* AstraCoreBridge::instance()
{
    static AstraCoreBridge inst;
    return &inst;
}

void AstraCoreBridge::loadLibrary()
{
    // Resolve platform-specific native shared library filenames: Windows .dll / Linux .so / macOS .dylib
#if defined(Q_OS_WIN)
    const QString libName = QStringLiteral("AstraCore.Native.dll");
#elif defined(Q_OS_MACOS)
    const QString libName = QStringLiteral("libAstraCore.Native.dylib");
#else
    const QString libName = QStringLiteral("libAstraCore.Native.so");
#endif

    // Search order: application directory, embedded assets/bin, macOS Frameworks, Linux lib, and system library path
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/" + libName,
        QCoreApplication::applicationDirPath() + "/assets/bin/" + libName,
        QCoreApplication::applicationDirPath() + "/../Frameworks/" + libName,
        QCoreApplication::applicationDirPath() + "/../lib/" + libName,
        libName
    };

    QString foundPath;
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            foundPath = path;
            break;
        }
    }

    if (foundPath.isEmpty()) {
        qWarning() << "[AstraCoreBridge] Warning:" << libName << "not found in candidate paths.";
        return;
    }

#if defined(Q_OS_WIN)
    SetDllDirectoryW(reinterpret_cast<LPCWSTR>(QFileInfo(foundPath).absolutePath().utf16()));
#endif

    m_lib.setFileName(foundPath);
    if (!m_lib.load()) {
        qWarning() << "[AstraCoreBridge] Failed to load" << foundPath << ":" << m_lib.errorString();
        return;
    }

    m_fn_abi_version = reinterpret_cast<ac_abi_version_fn>(m_lib.resolve("ac_abi_version"));
    m_fn_check_encoder = reinterpret_cast<ac_check_encoder_fn>(m_lib.resolve("ac_check_encoder"));
    m_fn_probe_utf8 = reinterpret_cast<ac_probe_utf8_fn>(m_lib.resolve("ac_probe_utf8"));
    m_fn_extract_waveform_peaks_utf8 = reinterpret_cast<ac_extract_waveform_peaks_utf8_fn>(m_lib.resolve("ac_extract_waveform_peaks_utf8"));
    m_fn_extract_audio_wav_utf8 = reinterpret_cast<ac_extract_audio_wav_utf8_fn>(m_lib.resolve("ac_extract_audio_wav_utf8"));
    m_fn_extract_keyframes_utf8 = reinterpret_cast<ac_extract_keyframes_utf8_fn>(m_lib.resolve("ac_extract_keyframes_utf8"));
    m_fn_extract_spectrogram_utf8 = reinterpret_cast<ac_extract_spectrogram_utf8_fn>(m_lib.resolve("ac_extract_spectrogram_utf8"));
    m_fn_extract_timecodes_utf8 = reinterpret_cast<ac_extract_timecodes_utf8_fn>(m_lib.resolve("ac_extract_timecodes_utf8"));
    m_fn_grab_frame_image_utf8 = reinterpret_cast<ac_grab_frame_image_utf8_fn>(m_lib.resolve("ac_grab_frame_image_utf8"));
    m_fn_probe_hdr_utf8 = reinterpret_cast<ac_probe_hdr_utf8_fn>(m_lib.resolve("ac_probe_hdr_utf8"));
    m_fn_video_open_session_utf8 = reinterpret_cast<ac_video_open_session_utf8_fn>(m_lib.resolve("ac_video_open_session_utf8"));
    m_fn_video_session_grab_frame = reinterpret_cast<ac_video_session_grab_frame_fn>(m_lib.resolve("ac_video_session_grab_frame"));
    m_fn_video_close_session = reinterpret_cast<ac_video_close_session_fn>(m_lib.resolve("ac_video_close_session"));
    m_fn_change_audio_speed_utf8 = reinterpret_cast<ac_change_audio_speed_utf8_fn>(m_lib.resolve("ac_change_audio_speed_utf8"));
    m_fn_trim_media_utf8 = reinterpret_cast<ac_trim_media_utf8_fn>(m_lib.resolve("ac_trim_media_utf8"));

    if (m_fn_abi_version && m_fn_probe_utf8) {
        m_available = true;
        qInfo() << "[AstraCoreBridge] Loaded successfully. ABI Version:" << m_fn_abi_version();
    } else {
        qWarning() << "[AstraCoreBridge] Failed to resolve essential symbols in" << foundPath;
    }
}

uint32_t AstraCoreBridge::abiVersion()
{
    return m_fn_abi_version ? m_fn_abi_version() : 0;
}

bool AstraCoreBridge::probe(const QString &filePath, MediaInfo &info)
{
    if (!m_fn_probe_utf8 || !QFileInfo::exists(filePath)) return false;

    NativeMediaInfo nativeInfo = {};
    nativeInfo.struct_size = sizeof(NativeMediaInfo);
    char errBuf[1024] = {0};

    int ret = m_fn_probe_utf8(filePath.toUtf8().constData(), &nativeInfo, errBuf, sizeof(errBuf));
    if (ret == 0) {
        info.duration = nativeInfo.duration_seconds;
        info.bitRate = nativeInfo.bit_rate;
        info.width = nativeInfo.width;
        info.height = nativeInfo.height;
        info.fps = nativeInfo.frame_rate > 0 ? nativeInfo.frame_rate : 23.976;
        info.hasVideo = nativeInfo.has_video != 0;
        info.hasAudio = nativeInfo.has_audio != 0;
        return true;
    } else {
        qWarning() << "[AstraCoreBridge] probe error:" << errBuf;
        return false;
    }
}

QVector<float> AstraCoreBridge::extractWaveformPeaks(const QString &filePath, int targetSampleRate, int samplesPerPeak, int maxPeaks, double *outDuration)
{
    QVector<float> peaks;
    if (!m_fn_extract_waveform_peaks_utf8 || !QFileInfo::exists(filePath)) return peaks;

    peaks.resize(maxPeaks);
    double dur = 0.0;
    char errBuf[1024] = {0};

    int ret = m_fn_extract_waveform_peaks_utf8(
        filePath.toUtf8().constData(),
        targetSampleRate,
        samplesPerPeak,
        peaks.data(),
        maxPeaks,
        &dur,
        errBuf,
        sizeof(errBuf)
    );

    if (ret > 0) {
        peaks.resize(ret);
        if (outDuration) *outDuration = dur;
        return peaks;
    } else {
        peaks.clear();
        qWarning() << "[AstraCoreBridge] extractWaveformPeaks error:" << errBuf;
        return peaks;
    }
}

bool AstraCoreBridge::extractAudioWav(const QString &inputPath, const QString &outputWavPath, int targetSampleRate, int channels)
{
    if (!m_fn_extract_audio_wav_utf8 || !QFileInfo::exists(inputPath)) return false;

    char errBuf[1024] = {0};
    int ret = m_fn_extract_audio_wav_utf8(
        inputPath.toUtf8().constData(),
        outputWavPath.toUtf8().constData(),
        targetSampleRate,
        channels,
        errBuf,
        sizeof(errBuf)
    );

    if (ret == 0) {
        return true;
    } else {
        qWarning() << "[AstraCoreBridge] extractAudioWav error:" << errBuf;
        return false;
    }
}

QVector<double> AstraCoreBridge::extractKeyframes(const QString &filePath, QVector<int64_t> *outFrameIndices, int maxKeyframes)
{
    QVector<double> timestamps;
    if (!m_fn_extract_keyframes_utf8 || !QFileInfo::exists(filePath) || maxKeyframes <= 0) return timestamps;

    timestamps.resize(maxKeyframes);
    QVector<int64_t> frameIndices;
    if (outFrameIndices) {
        frameIndices.resize(maxKeyframes);
    }

    char errBuf[1024] = {0};
    int ret = m_fn_extract_keyframes_utf8(
        filePath.toUtf8().constData(),
        timestamps.data(),
        outFrameIndices ? frameIndices.data() : nullptr,
        maxKeyframes,
        errBuf,
        sizeof(errBuf)
    );

    if (ret >= 0) {
        int count = qMin(ret, maxKeyframes);
        timestamps.resize(count);
        if (outFrameIndices) {
            frameIndices.resize(count);
            *outFrameIndices = frameIndices;
        }
        return timestamps;
    } else {
        timestamps.clear();
        qWarning() << "[AstraCoreBridge] extractKeyframes error:" << errBuf;
        return timestamps;
    }
}

QVector<float> AstraCoreBridge::extractSpectrogram(const QString &filePath, int targetSampleRate, int nFft, int hopSize, int *outNumBins, int maxFrames)
{
    QVector<float> spectrogram;
    if (!m_fn_extract_spectrogram_utf8 || !QFileInfo::exists(filePath) || maxFrames <= 0) return spectrogram;

    int numBins = nFft / 2;
    spectrogram.resize(maxFrames * numBins);
    char errBuf[1024] = {0};
    int binsResult = 0;
    double duration = 0.0;

    int ret = m_fn_extract_spectrogram_utf8(
        filePath.toUtf8().constData(),
        targetSampleRate,
        nFft,
        hopSize,
        spectrogram.data(),
        maxFrames,
        &binsResult,
        &duration,
        errBuf,
        sizeof(errBuf)
    );

    if (ret >= 0) {
        spectrogram.resize(ret * binsResult);
        if (outNumBins) *outNumBins = binsResult;
        return spectrogram;
    } else {
        spectrogram.clear();
        qWarning() << "[AstraCoreBridge] extractSpectrogram error:" << errBuf;
        return spectrogram;
    }
}

QVector<double> AstraCoreBridge::extractTimecodes(const QString &inputPath, const QString &outputTimecodesPath, int maxFrames)
{
    QVector<double> ptsList;
    if (!m_fn_extract_timecodes_utf8 || !QFileInfo::exists(inputPath) || maxFrames <= 0) return ptsList;

    ptsList.resize(maxFrames);
    char errBuf[1024] = {0};

    int ret = m_fn_extract_timecodes_utf8(
        inputPath.toUtf8().constData(),
        outputTimecodesPath.isEmpty() ? nullptr : outputTimecodesPath.toUtf8().constData(),
        ptsList.data(),
        maxFrames,
        errBuf,
        sizeof(errBuf)
    );

    if (ret >= 0) {
        ptsList.resize(qMin(ret, maxFrames));
        return ptsList;
    } else {
        ptsList.clear();
        qWarning() << "[AstraCoreBridge] extractTimecodes error:" << errBuf;
        return ptsList;
    }
}

QImage AstraCoreBridge::grabFrameImage(const QString &filePath, double targetSeconds, int targetWidth, int targetHeight, double *outActualSeconds)
{
    if (!m_fn_grab_frame_image_utf8 || !QFileInfo::exists(filePath)) return QImage();

    // Default probe for dimensions if not given
    int reqWidth = targetWidth > 0 ? targetWidth : 1920;
    int reqHeight = targetHeight > 0 ? targetHeight : 1080;
    if (reqWidth > 16384 || reqHeight > 16384 || reqWidth <= 0 || reqHeight <= 0) return QImage();

    // RGBA format = 4 bytes per pixel with integer overflow protection
    uint64_t rawBufSize = static_cast<uint64_t>(reqWidth) * static_cast<uint64_t>(reqHeight) * 4ULL;
    if (rawBufSize > static_cast<uint64_t>(std::numeric_limits<int>::max())) return QImage();

    size_t bufSize = static_cast<size_t>(rawBufSize);
    QByteArray buffer(static_cast<int>(bufSize), 0);

    int outW = 0, outH = 0;
    double actualTs = 0.0;
    char errBuf[1024] = {0};

    int ret = m_fn_grab_frame_image_utf8(
        filePath.toUtf8().constData(),
        targetSeconds,
        targetWidth,
        targetHeight,
        1, // RGBA
        reinterpret_cast<uint8_t*>(buffer.data()),
        bufSize,
        &outW,
        &outH,
        &actualTs,
        errBuf,
        sizeof(errBuf)
    );

    if (ret == 0 && outW > 0 && outH > 0) {
        if (outActualSeconds) *outActualSeconds = actualTs;
        QImage img(reinterpret_cast<const uchar*>(buffer.constData()), outW, outH, outW * 4, QImage::Format_RGBA8888);
        return img.copy(); // Deep copy so buffer can be safely released
    } else {
        qWarning() << "[AstraCoreBridge] grabFrameImage error:" << errBuf;
        return QImage();
    }
}

#pragma pack(push, 8)
struct NativeHdrMetadata {
    uint32_t struct_size;
    int32_t is_hdr;
    int32_t bit_depth;
    int32_t color_primaries;
    int32_t color_transfer;
    int32_t color_space;
    double max_cll;
    double max_fall;
};
#pragma pack(pop)

bool AstraCoreBridge::probeHdr(const QString &filePath, bool &isHdr, int &bitDepth, int &colorPrimaries, int &colorTransfer)
{
    isHdr = false;
    bitDepth = 8;
    colorPrimaries = 0;
    colorTransfer = 0;

    if (!m_fn_probe_hdr_utf8 || !QFileInfo::exists(filePath)) return false;

    NativeHdrMetadata hdrMeta = {};
    hdrMeta.struct_size = sizeof(NativeHdrMetadata);
    char errBuf[1024] = {0};

    int ret = m_fn_probe_hdr_utf8(filePath.toUtf8().constData(), &hdrMeta, errBuf, sizeof(errBuf));
    if (ret == 0) {
        isHdr = hdrMeta.is_hdr != 0;
        bitDepth = hdrMeta.bit_depth;
        colorPrimaries = hdrMeta.color_primaries;
        colorTransfer = hdrMeta.color_transfer;
        return true;
    } else {
        qWarning() << "[AstraCoreBridge] probeHdr error:" << errBuf;
        return false;
    }
}

void* AstraCoreBridge::openVideoSession(const QString &filePath)
{
    if (!m_fn_video_open_session_utf8 || !QFileInfo::exists(filePath)) return nullptr;
    char errBuf[1024] = {0};
    void *session = m_fn_video_open_session_utf8(filePath.toUtf8().constData(), errBuf, sizeof(errBuf));
    if (!session) {
        qWarning() << "[AstraCoreBridge] openVideoSession error:" << errBuf;
    }
    return session;
}

QImage AstraCoreBridge::grabSessionFrame(void *session, double targetSeconds, int targetWidth, int targetHeight, double *outActualSeconds)
{
    if (!m_fn_video_session_grab_frame || !session) return QImage();

    int reqWidth = targetWidth > 0 ? targetWidth : 1920;
    int reqHeight = targetHeight > 0 ? targetHeight : 1080;
    if (reqWidth > 16384 || reqHeight > 16384 || reqWidth <= 0 || reqHeight <= 0) return QImage();

    uint64_t rawBufSize = static_cast<uint64_t>(reqWidth) * static_cast<uint64_t>(reqHeight) * 4ULL;
    if (rawBufSize > static_cast<uint64_t>(std::numeric_limits<int>::max())) return QImage();

    size_t bufSize = static_cast<size_t>(rawBufSize);
    QByteArray buffer(static_cast<int>(bufSize), 0);

    int outW = 0, outH = 0;
    double actualTs = 0.0;
    char errBuf[1024] = {0};

    int ret = m_fn_video_session_grab_frame(
        session,
        targetSeconds,
        targetWidth,
        targetHeight,
        1, // RGBA
        reinterpret_cast<uint8_t*>(buffer.data()),
        bufSize,
        &outW,
        &outH,
        &actualTs,
        errBuf,
        sizeof(errBuf)
    );

    if (ret == 0 && outW > 0 && outH > 0) {
        if (outActualSeconds) *outActualSeconds = actualTs;
        QImage img(reinterpret_cast<const uchar*>(buffer.constData()), outW, outH, outW * 4, QImage::Format_RGBA8888);
        return img.copy();
    } else {
        qWarning() << "[AstraCoreBridge] grabSessionFrame error:" << errBuf;
        return QImage();
    }
}

void AstraCoreBridge::closeVideoSession(void *session)
{
    if (m_fn_video_close_session && session) {
        m_fn_video_close_session(session);
    }
}

bool AstraCoreBridge::changeAudioSpeed(const QString &inputPath, const QString &outputPath, double speedFactor)
{
    if (!m_fn_change_audio_speed_utf8 || !QFileInfo::exists(inputPath)) return false;

    char errBuf[1024] = {0};
    int ret = m_fn_change_audio_speed_utf8(
        inputPath.toUtf8().constData(),
        outputPath.toUtf8().constData(),
        speedFactor,
        errBuf,
        sizeof(errBuf)
    );

    if (ret == 0) {
        return true;
    } else {
        qWarning() << "[AstraCoreBridge] changeAudioSpeed error:" << errBuf;
        return false;
    }
}

bool AstraCoreBridge::trimMedia(const QString &inputPath, const QString &outputPath, double startSeconds, double durationSeconds, bool streamCopy)
{
    if (!m_fn_trim_media_utf8 || !QFileInfo::exists(inputPath)) return false;

    char errBuf[1024] = {0};
    int ret = m_fn_trim_media_utf8(
        inputPath.toUtf8().constData(),
        outputPath.toUtf8().constData(),
        startSeconds,
        durationSeconds,
        streamCopy ? 1 : 0,
        errBuf,
        sizeof(errBuf)
    );

    if (ret == 0) {
        return true;
    } else {
        qWarning() << "[AstraCoreBridge] trimMedia error:" << errBuf;
        return false;
    }
}



