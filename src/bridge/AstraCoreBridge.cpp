// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Aegisub Project http://www.aegisub.org/

#include "AstraCoreBridge.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <limits>
#include <cmath>
#include <string>
#include "../../third_party/astracore/include/astracore.h"
#if defined(Q_OS_WIN)
#define NOMINMAX
#include <windows.h>
#endif

// Use the same ABI declarations as the bundled native implementation.
using NativeMediaInfo = ac_media_info;
namespace {
QString nativeCanonicalPath(const QString &path) {
#if defined(Q_OS_WIN)
    const QString native = QDir::toNativeSeparators(path);
    HANDLE file = CreateFileW(reinterpret_cast<LPCWSTR>(native.utf16()), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    const DWORD capacity = GetFinalPathNameByHandleW(file, nullptr, 0, FILE_NAME_NORMALIZED);
    std::wstring resolved(capacity, L'\0');
    const DWORD length = capacity ? GetFinalPathNameByHandleW(file, resolved.data(), capacity, FILE_NAME_NORMALIZED) : 0;
    CloseHandle(file);
    if (!length || length >= capacity) return {};
    resolved.resize(length);
    if (resolved.rfind(L"\\\\?\\UNC\\", 0) == 0) resolved = L"\\\\" + resolved.substr(8);
    else if (resolved.rfind(L"\\\\?\\", 0) == 0) resolved.erase(0, 4);
    return QDir::fromNativeSeparators(QString::fromStdWString(resolved));
#else
    return QFileInfo(path).canonicalFilePath();
#endif
}
bool validFrame(int width, int height, size_t capacity, double timestamp) {
    return width > 0 && height > 0 && width <= 16384 && height <= 16384
        && std::isfinite(timestamp)
        && uint64_t(width) * uint64_t(height) * 4 <= capacity;
}
bool validCount(int count, size_t elementSize) {
    return count > 0 && uint64_t(count) * elementSize <= uint64_t(std::numeric_limits<int>::max());
}
#if defined(Q_OS_WIN)
class NativeLoadErrorMode {
    DWORD previous = 0;
    bool changed;
public:
    NativeLoadErrorMode() : changed(SetThreadErrorMode(GetThreadErrorMode()
        | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &previous) != 0) {}
    ~NativeLoadErrorMode() { if (changed) SetThreadErrorMode(previous, nullptr); }
};
#endif
}

AstraCoreBridge::AstraCoreBridge(QObject *parent)
    : QObject(parent)
{
    loadLibrary();
}

AstraCoreBridge::~AstraCoreBridge()
{
#if defined(Q_OS_WIN)
    if (m_nativeHandle) FreeLibrary(static_cast<HMODULE>(m_nativeHandle));
#else
    if (m_lib.isLoaded()) {
        m_lib.unload();
    }
#endif
}

QFunctionPointer AstraCoreBridge::resolveSymbol(const char *name)
{
#if defined(Q_OS_WIN)
    return reinterpret_cast<QFunctionPointer>(GetProcAddress(static_cast<HMODULE>(m_nativeHandle), name));
#else
    return m_lib.resolve(name);
#endif
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

    // Only application/bundle/install-prefix locations are eligible. Never
    // interpret a relative filename through the current working directory.
    const QDir app(QCoreApplication::applicationDirPath());
    struct Candidate { QString path, root; };
    QList<Candidate> candidates = {
        {app.filePath(libName), app.absolutePath()},
        {app.filePath("assets/bin/" + libName), app.absolutePath()}
    };
#if defined(Q_OS_MACOS)
    if (app.dirName() == "MacOS")
        candidates.append({app.filePath("../Frameworks/" + libName), app.filePath("..")});
#elif !defined(Q_OS_WIN)
    if (app.dirName() == "bin")
        candidates.append({app.filePath("../lib/" + libName), app.filePath("..")});
#endif

    QString foundPath;
    for (const Candidate &candidate : candidates) {
        const QFileInfo file(candidate.path);
        const QString path = nativeCanonicalPath(candidate.path);
        const QString root = nativeCanonicalPath(candidate.root);
#if defined(Q_OS_WIN)
        constexpr auto pathCase = Qt::CaseInsensitive;
#else
        constexpr auto pathCase = Qt::CaseSensitive;
#endif
        if (!file.isFile() || path.isEmpty() || root.isEmpty()
            || !path.startsWith(root + '/', pathCase)) continue;
#if defined(Q_OS_WIN)
        // Restrict dependency resolution to this DLL's directory, the app and
        // System32. No SetDllDirectory/AddDllDirectory or PATH/CWD fallback.
        const QString nativePath = QDir::toNativeSeparators(path);
        NativeLoadErrorMode errorMode;
        m_nativeHandle = LoadLibraryExW(reinterpret_cast<LPCWSTR>(nativePath.utf16()), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!m_nativeHandle) {
            qWarning() << "[AstraCoreBridge] Cannot load" << path << "Windows error:" << GetLastError();
            continue;
        }
#else
        m_lib.setFileName(path);
        if (!m_lib.load()) {
            qWarning() << "[AstraCoreBridge] Cannot load" << path << m_lib.errorString();
            continue;
        }
#endif
        foundPath = path;
        break;
    }

    if (foundPath.isEmpty()) {
        qWarning() << "[AstraCoreBridge] Warning:" << libName << "not found in candidate paths.";
        return;
    }

    m_fn_abi_version = reinterpret_cast<ac_abi_version_fn>(resolveSymbol("ac_abi_version"));
    if (!m_fn_abi_version || m_fn_abi_version() != AC_ABI_VERSION) {
        qWarning() << "[AstraCoreBridge] Unsupported native ABI; expected" << AC_ABI_VERSION;
        return;
    }
    m_fn_check_encoder = reinterpret_cast<ac_check_encoder_fn>(resolveSymbol("ac_check_encoder"));
    m_fn_probe_utf8 = reinterpret_cast<ac_probe_utf8_fn>(resolveSymbol("ac_probe_utf8"));
    m_fn_extract_waveform_peaks_utf8 = reinterpret_cast<ac_extract_waveform_peaks_utf8_fn>(resolveSymbol("ac_extract_waveform_peaks_utf8"));
    m_fn_extract_audio_wav_utf8 = reinterpret_cast<ac_extract_audio_wav_utf8_fn>(resolveSymbol("ac_extract_audio_wav_utf8"));
    m_fn_extract_keyframes_utf8 = reinterpret_cast<ac_extract_keyframes_utf8_fn>(resolveSymbol("ac_extract_keyframes_utf8"));
    m_fn_extract_spectrogram_utf8 = reinterpret_cast<ac_extract_spectrogram_utf8_fn>(resolveSymbol("ac_extract_spectrogram_utf8"));
    m_fn_extract_timecodes_utf8 = reinterpret_cast<ac_extract_timecodes_utf8_fn>(resolveSymbol("ac_extract_timecodes_utf8"));
    m_fn_grab_frame_image_utf8 = reinterpret_cast<ac_grab_frame_image_utf8_fn>(resolveSymbol("ac_grab_frame_image_utf8"));
    m_fn_probe_hdr_utf8 = reinterpret_cast<ac_probe_hdr_utf8_fn>(resolveSymbol("ac_probe_hdr_utf8"));
    m_fn_video_open_session_utf8 = reinterpret_cast<ac_video_open_session_utf8_fn>(resolveSymbol("ac_video_open_session_utf8"));
    m_fn_video_session_grab_frame = reinterpret_cast<ac_video_session_grab_frame_fn>(resolveSymbol("ac_video_session_grab_frame"));
    m_fn_video_close_session = reinterpret_cast<ac_video_close_session_fn>(resolveSymbol("ac_video_close_session"));
    m_fn_change_audio_speed_utf8 = reinterpret_cast<ac_change_audio_speed_utf8_fn>(resolveSymbol("ac_change_audio_speed_utf8"));
    m_fn_trim_media_utf8 = reinterpret_cast<ac_trim_media_utf8_fn>(resolveSymbol("ac_trim_media_utf8"));

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
    if (!m_available || !m_fn_probe_utf8 || !QFileInfo::exists(filePath)) return false;

    NativeMediaInfo nativeInfo = {};
    nativeInfo.struct_size = sizeof(NativeMediaInfo);
    char errBuf[1024] = {0};

    int ret = m_fn_probe_utf8(filePath.toUtf8().constData(), &nativeInfo, errBuf, sizeof(errBuf));
    if (ret == 0 && nativeInfo.struct_size == sizeof(nativeInfo)
        && std::isfinite(nativeInfo.duration_seconds) && nativeInfo.duration_seconds >= 0
        && std::isfinite(nativeInfo.frame_rate) && nativeInfo.frame_rate >= 0
        && nativeInfo.width >= 0 && nativeInfo.height >= 0
        && (!nativeInfo.has_video || (nativeInfo.width > 0 && nativeInfo.height > 0))) {
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
    if (!m_available || !m_fn_extract_waveform_peaks_utf8 || !QFileInfo::exists(filePath)
        || targetSampleRate <= 0 || samplesPerPeak <= 0 || !validCount(maxPeaks, sizeof(float))) return peaks;

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

    if (ret > 0 && ret <= maxPeaks && std::isfinite(dur) && dur >= 0) {
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
    if (!m_available || !m_fn_extract_audio_wav_utf8 || !QFileInfo::exists(inputPath)
        || targetSampleRate <= 0 || channels <= 0) return false;

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
    if (!m_available || !m_fn_extract_keyframes_utf8 || !QFileInfo::exists(filePath)
        || !validCount(maxKeyframes, sizeof(int64_t))) return timestamps;

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
    if (!m_available || !m_fn_extract_spectrogram_utf8 || !QFileInfo::exists(filePath)
        || targetSampleRate <= 0 || hopSize <= 0 || nFft < 16
        || (nFft & (nFft - 1)) != 0 || maxFrames <= 0) return spectrogram;

    int numBins = nFft / 2;
    const uint64_t capacity = uint64_t(maxFrames) * uint64_t(numBins);
    if (capacity > uint64_t(std::numeric_limits<int>::max()) / sizeof(float)) return spectrogram;
    spectrogram.resize(qsizetype(capacity));
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

    if (ret >= 0 && ret <= maxFrames && binsResult == numBins) {
        spectrogram.resize(qsizetype(ret) * numBins);
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
    if (!m_available || !m_fn_extract_timecodes_utf8 || !QFileInfo::exists(inputPath)
        || !validCount(maxFrames, sizeof(double))) return ptsList;

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
    if (!m_available || !m_fn_grab_frame_image_utf8 || !QFileInfo::exists(filePath)
        || !std::isfinite(targetSeconds) || targetSeconds < 0 || targetWidth < 0 || targetHeight < 0) return QImage();

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

    if (ret == 0 && validFrame(outW, outH, bufSize, actualTs)) {
        if (outActualSeconds) *outActualSeconds = actualTs;
        QImage img(reinterpret_cast<const uchar*>(buffer.constData()), outW, outH, outW * 4, QImage::Format_RGBA8888);
        return img.copy(); // Deep copy so buffer can be safely released
    } else {
        qWarning() << "[AstraCoreBridge] grabFrameImage error:" << errBuf;
        return QImage();
    }
}

using NativeHdrMetadata = ac_hdr_metadata;

bool AstraCoreBridge::probeHdr(const QString &filePath, bool &isHdr, int &bitDepth, int &colorPrimaries,
                               int &colorTransfer, int *colorSpace, int *colorRange)
{
    isHdr = false;
    bitDepth = 8;
    colorPrimaries = 0;
    colorTransfer = 0;
    if (colorSpace) *colorSpace = -1;
    if (colorRange) *colorRange = 0;

    if (!m_available || !m_fn_probe_hdr_utf8 || !QFileInfo::exists(filePath)) return false;

    NativeHdrMetadata hdrMeta = {};
    hdrMeta.struct_size = sizeof(NativeHdrMetadata);
    char errBuf[1024] = {0};

    int ret = m_fn_probe_hdr_utf8(filePath.toUtf8().constData(), &hdrMeta, errBuf, sizeof(errBuf));
    if (ret == 0 && hdrMeta.struct_size == sizeof(hdrMeta)
        && hdrMeta.bit_depth > 0 && hdrMeta.bit_depth <= 64) {
        isHdr = hdrMeta.is_hdr != 0;
        bitDepth = hdrMeta.bit_depth;
        colorPrimaries = hdrMeta.color_primaries;
        colorTransfer = hdrMeta.color_transfer;
        if (colorSpace) *colorSpace = hdrMeta.color_space;
        if (colorRange) *colorRange = hdrMeta.color_range;
        return true;
    } else {
        qWarning() << "[AstraCoreBridge] probeHdr error:" << errBuf;
        return false;
    }
}

void* AstraCoreBridge::openVideoSession(const QString &filePath)
{
    if (!m_available || !m_fn_video_open_session_utf8 || !m_fn_video_session_grab_frame
        || !m_fn_video_close_session || !QFileInfo::exists(filePath)) return nullptr;
    char errBuf[1024] = {0};
    void *session = m_fn_video_open_session_utf8(filePath.toUtf8().constData(), errBuf, sizeof(errBuf));
    if (!session) {
        qWarning() << "[AstraCoreBridge] openVideoSession error:" << errBuf;
    }
    return session;
}

QImage AstraCoreBridge::grabSessionFrame(void *session, double targetSeconds, int targetWidth, int targetHeight, double *outActualSeconds)
{
    if (!m_available || !m_fn_video_session_grab_frame || !session
        || !std::isfinite(targetSeconds) || targetSeconds < 0 || targetWidth < 0 || targetHeight < 0) return QImage();

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

    if (ret == 0 && validFrame(outW, outH, bufSize, actualTs)) {
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
    if (!m_available || !m_fn_change_audio_speed_utf8 || !QFileInfo::exists(inputPath)
        || !std::isfinite(speedFactor) || speedFactor < 0.25 || speedFactor > 4.0) return false;

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
    if (!m_available || !m_fn_trim_media_utf8 || !QFileInfo::exists(inputPath)
        || !std::isfinite(startSeconds) || startSeconds < 0
        || !std::isfinite(durationSeconds) || durationSeconds <= 0) return false;

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



