#ifndef ASTRACORE_H
#define ASTRACORE_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(ASTRACORE_BUILD)
#    define AC_API __declspec(dllexport)
#  else
#    define AC_API __declspec(dllimport)
#  endif
#else
#  define AC_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AstraCore Semantic Versioning and Binary Interface (ABI) Specification.
 *
 * Versioning model:
 *   - MAJOR (high 16 bits): Incremented on breaking ABI changes (e.g. struct layout changes
 *     without backward fallback, removed symbols, altered calling conventions).
 *   - MINOR (middle 8 bits): Incremented on backward-compatible feature additions
 *     (new exported functions, new enum values, extended optional capabilities).
 *   - PATCH (low 8 bits): Incremented on internal bug fixes, optimizations, or codec updates.
 *
 * Legacy Compatibility:
 *   ac_abi_version() continues to return AC_VERSION_MAJOR (currently 5u) to preserve
 *   the original scalar contract.
 *   Callers desiring full semantic versioning can invoke ac_version() or ac_version_string().
 *   Callers checking feature availability should invoke ac_has_feature().
 */
#define AC_VERSION_MAJOR 5u
#define AC_VERSION_MINOR 1u
#define AC_VERSION_PATCH 0u

#define AC_VERSION_INT(major, minor, patch) \
    (((uint32_t)(major) << 16) | ((uint32_t)(minor) << 8) | ((uint32_t)(patch)))

#define AC_BUILD_VERSION AC_VERSION_INT(AC_VERSION_MAJOR, AC_VERSION_MINOR, AC_VERSION_PATCH)

#define AC_GET_MAJOR(v) (((uint32_t)(v) >> 16) & 0xFFFFu)
#define AC_GET_MINOR(v) (((uint32_t)(v) >> 8)  & 0xFFu)
#define AC_GET_PATCH(v) (((uint32_t)(v))       & 0xFFu)

/* Legacy single-scalar ABI definition */
#define AC_ABI_VERSION AC_VERSION_MAJOR

/*
 * Cancellation callback: returns nonzero when cancellation is requested.
 */
typedef int (*ac_cancel_callback)(void *opaque);

typedef struct ac_media_info {
    uint32_t struct_size;
    double duration_seconds;
    int64_t bit_rate;
    int32_t width;
    int32_t height;
    double frame_rate;
    int32_t has_video;
    int32_t has_audio;
} ac_media_info;

/* Returns legacy major ABI revision (e.g., 5u). */
AC_API uint32_t ac_abi_version(void);

/* Returns packed semantic 32-bit version: (major << 16) | (minor << 8) | patch. */
AC_API uint32_t ac_version(void);

/* Returns human-readable version string, e.g. "5.0.0". */
AC_API const char *ac_version_string(void);

/*
 * Queries runtime feature availability by tag string.
 * Returns 1 if supported, 0 if unsupported, or -1 on invalid argument.
 *
 * Supported tags:
 *   - "keyframes"       : ac_extract_keyframes_utf8 / _cancel_utf8
 *   - "frame_grabber"   : ac_grab_frame_image_utf8 / _cancel_utf8
 *   - "timecodes"       : ac_extract_timecodes_utf8 / _cancel_utf8
 *   - "spectrogram"     : ac_extract_spectrogram_utf8 / _cancel_utf8
 *   - "hdr_prober"      : ac_probe_hdr_utf8 / _cancel_utf8
 *   - "waveform"        : ac_extract_waveform_peaks_utf8 / _cancel_utf8
 *   - "audio_tempo"     : ac_change_audio_speed_utf8 / _cancel_utf8
 *   - "lossless_trim"   : ac_trim_media_utf8 / _cancel_utf8
 *   - "video_session"   : ac_video_open_session_utf8 / ac_video_session_grab_frame
 *   - "swscale"         : libswscale pixel conversions
 */
AC_API int ac_has_feature(const char *feature_name);

/*
 * Reads container and stream metadata without decoding frames.
 * Returns 0 on success or a negative libav error code. All paths are UTF-8.
 * error_buffer is optional and always NUL-terminated when its size is nonzero.
 */
AC_API int ac_probe_utf8(
    const char *path,
    ac_media_info *result,
    char *error_buffer,
    size_t error_buffer_size);

AC_API int ac_probe_cancel_utf8(
    const char *path,
    ac_media_info *result,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size);

/*
 * Checks whether the specified encoder (e.g. "h264_nvenc", "hevc_nvenc", "av1_nvenc", "libx264")
 * is compiled into libavcodec and can be successfully initialized by the driver/library.
 * Returns 1 if available and operational, 0 if unsupported or failed to initialize.
 */
AC_API int ac_check_encoder(const char *encoder_name);

/*
 * Decodes the first audio stream of the given file, resamples to mono at target_sample_rate (e.g. 4000),
 * computes peak absolute amplitudes in buckets of samples_per_peak,
 * and writes up to max_peaks into out_peaks (values normalized in [0.0f, 1.0f]).
 * out_duration receives the audio stream duration in seconds.
 * Returns the number of peaks written on success, or a negative libav error code on failure.
 * error_buffer is optional and always NUL-terminated when error_buffer_size > 0.
 */
AC_API int ac_extract_waveform_peaks_utf8(
    const char *path,
    int target_sample_rate,
    int samples_per_peak,
    float *out_peaks,
    int max_peaks,
    double *out_duration,
    char *error_buffer,
    size_t error_buffer_size);

AC_API int ac_extract_waveform_peaks_cancel_utf8(
    const char *path,
    int target_sample_rate,
    int samples_per_peak,
    float *out_peaks,
    int max_peaks,
    double *out_duration,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size);

/*
 * Decodes the first audio stream of input_path, resamples to target_sample_rate (e.g. 16000)
 * mono (or channels) 16-bit signed integer PCM, and writes a standard RIFF/WAV file to output_wav_path.
 * Returns 0 on success, or a negative libav error code on failure.
 * error_buffer is optional and always NUL-terminated when error_buffer_size > 0.
 */
AC_API int ac_extract_audio_wav_utf8(
    const char *input_path,
    const char *output_wav_path,
    int target_sample_rate,
    int channels,
    char *error_buffer,
    size_t error_buffer_size);

AC_API int ac_extract_audio_wav_cancel_utf8(
    const char *input_path,
    const char *output_wav_path,
    int target_sample_rate,
    int channels,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size);

/*
 * Adjusts audio tempo (speed) using atempo filter without altering pitch.
 * speed_factor must be in range [0.25, 4.0]. Writes a 16-bit PCM WAV file.
 * Returns 0 on success, or a negative libav error code on failure.
 */
AC_API int ac_change_audio_speed_cancel_utf8(
    const char *input_path,
    const char *output_wav_path,
    double speed_factor,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size);

AC_API int ac_change_audio_speed_utf8(
    const char *input_path,
    const char *output_wav_path,
    double speed_factor,
    char *error_buffer,
    size_t error_buffer_size);

/*
 * Trims a media file from start_seconds with duration_seconds.
 * When stream_copy is 1, performs fast lossless packet copy with timestamp rewriting.
 * When stream_copy is 0, decodes and re-encodes frames.
 * Returns 0 on success, or a negative libav error code on failure.
 */
AC_API int ac_trim_media_cancel_utf8(
    const char *input_path,
    const char *output_path,
    double start_seconds,
    double duration_seconds,
    int stream_copy,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size);

AC_API int ac_trim_media_utf8(
    const char *input_path,
    const char *output_path,
    double start_seconds,
    double duration_seconds,
    int stream_copy,
    char *error_buffer,
    size_t error_buffer_size);

/*
 * Demuxes and copies the specified audio stream index (or first audio stream if < 0)
 * into a standalone audio file without re-encoding.
 * Returns 0 on success, or a negative libav error code on failure.
 */
AC_API int ac_extract_audio_stream_utf8(
    const char *input_media,
    const char *output_audio,
    int stream_index,
    char *error_buffer,
    size_t error_buffer_size);

/*
 * 1. Keyframe list extraction:
 * Scans video stream packet headers using demuxer without decoding frames.
 * Writes up to max_keyframes timestamps (in seconds) to out_timestamps (if non-NULL),
 * and frame indices to out_frame_indices (if non-NULL).
 * Returns the total count of keyframes found, or negative libav error code on failure.
 */
AC_API int ac_extract_keyframes_cancel_utf8(
    const char *path,
    double *out_timestamps,
    int64_t *out_frame_indices,
    int max_keyframes,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size);

AC_API int ac_extract_keyframes_utf8(
    const char *path,
    double *out_timestamps,
    int64_t *out_frame_indices,
    int max_keyframes,
    char *error_buffer,
    size_t error_buffer_size);

/*
 * 2. Audio spectrogram extraction (STFT frequency-magnitude matrix):
 * Decodes audio, resamples to target_sample_rate (e.g. 16000), applies Hann windowing,
 * performs FFT of size n_fft (e.g. 512, must be power of 2), and outputs (n_fft / 2) bins
 * per time frame. Values written to out_magnitudes are normalized in [0.0f, 1.0f].
 * out_num_bins receives (n_fft / 2). out_duration receives audio duration in seconds.
 * Returns the total number of time-slices written, or negative libav error code on failure.
 */
AC_API int ac_extract_spectrogram_cancel_utf8(
    const char *path,
    int target_sample_rate,
    int n_fft,
    int hop_size,
    float *out_magnitudes,
    int max_frames,
    int *out_num_bins,
    double *out_duration,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size);

AC_API int ac_extract_spectrogram_utf8(
    const char *path,
    int target_sample_rate,
    int n_fft,
    int hop_size,
    float *out_magnitudes,
    int max_frames,
    int *out_num_bins,
    double *out_duration,
    char *error_buffer,
    size_t error_buffer_size);

/*
 * 3. Variable Frame Rate (VFR) Timecodes extraction:
 * Scans video packet timestamps in sequence.
 * If out_pts_seconds is non-NULL, writes up to max_frames presentation timestamps (in seconds).
 * If output_timecodes_path is non-NULL, writes a standard Matroska/Aegisub Timecodes v2 text file.
 * Returns total video frames detected, or negative libav error code on failure.
 */
AC_API int ac_extract_timecodes_cancel_utf8(
    const char *input_path,
    const char *output_timecodes_path,
    double *out_pts_seconds,
    int max_frames,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size);

AC_API int ac_extract_timecodes_utf8(
    const char *input_path,
    const char *output_timecodes_path,
    double *out_pts_seconds,
    int max_frames,
    char *error_buffer,
    size_t error_buffer_size);

/*
 * 4. In-memory precise frame grabber (for Subtitle OCR & timeline thumbnails):
 * Seeks to target_seconds, decodes the target video frame, and scales to caller's buffer.
 * pix_fmt: 0 = RGB24 (3 bytes/px), 1 = RGBA32 (4 bytes/px), 2 = BGRA32 (4 bytes/px).
 * If target_width and target_height are > 0, rescales; otherwise uses native frame resolution.
 * out_actual_seconds receives the decoded frame's presentation timestamp.
 * out_width and out_height receive actual dimensions of the output image.
 * Returns 0 on success, or a negative libav error code on failure.
 */
AC_API int ac_grab_frame_image_cancel_utf8(
    const char *path,
    double target_seconds,
    int target_width,
    int target_height,
    int pix_fmt,
    uint8_t *out_image_buffer,
    size_t buffer_size,
    int *out_width,
    int *out_height,
    double *out_actual_seconds,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size);

AC_API int ac_grab_frame_image_utf8(
    const char *path,
    double target_seconds,
    int target_width,
    int target_height,
    int pix_fmt,
    uint8_t *out_image_buffer,
    size_t buffer_size,
    int *out_width,
    int *out_height,
    double *out_actual_seconds,
    char *error_buffer,
    size_t error_buffer_size);

/*
 * 5. HDR and 10-bit color metadata:
 */
typedef struct ac_hdr_metadata {
    uint32_t struct_size;
    int32_t is_hdr;             /* 1 if HDR (PQ / HLG / BT.2020), 0 if SDR */
    int32_t bit_depth;          /* 8, 10, or 12 bits */
    int32_t color_primaries;    /* AVColorPrimaries (e.g. 9 = BT.2020) */
    int32_t color_transfer;     /* AVColorTransferCharacteristic (e.g. 16 = SMPTE ST 2084 / PQ, 18 = HLG) */
    int32_t color_space;        /* AVColorSpace (e.g. 9 = BT.2020 non-constant luminance) */
    double max_cll;             /* Maximum Content Light Level in nits (0.0 if not specified) */
    double max_fall;            /* Maximum Frame-Average Light Level in nits (0.0 if not specified) */
    int32_t color_range;        /* AVColorRange; optional tail field in the extended struct */
} ac_hdr_metadata;

/*
 * Probes HDR10 / HLG / 10-bit metadata from container and video stream side data.
 * Returns 0 on success, or negative libav error code.
 */
AC_API int ac_probe_hdr_cancel_utf8(
    const char *path,
    ac_hdr_metadata *result,
    ac_cancel_callback cancel_cb,
    void *cancel_opaque,
    char *error_buffer,
    size_t error_buffer_size);

AC_API int ac_probe_hdr_utf8(
    const char *path,
    ac_hdr_metadata *result,
    char *error_buffer,
    size_t error_buffer_size);

/*
 * 6. Persistent Video Decoding Session:
 * Maintains open container and codec contexts across multiple sequential or random-access
 * frame grab requests, eliminating repetitive container probing and codec init overhead.
 * Calls on one session must be serialized; separate sessions are independent.
 * Grab requires a finite, nonnegative time, nonnegative dimensions (0 = source
 * size), pix_fmt 0/1/2 as above, and a sufficiently sized caller buffer.
 * Delayed frames are drained at EOF. Closing NULL is allowed.
 */
typedef struct AcVideoSession AcVideoSession;

AC_API AcVideoSession *ac_video_open_session_utf8(
    const char *path,
    char *error_buffer,
    size_t error_buffer_size);

AC_API int ac_video_session_grab_frame(
    AcVideoSession *session,
    double target_seconds,
    int target_width,
    int target_height,
    int pix_fmt,
    uint8_t *out_image_buffer,
    size_t buffer_size,
    int *out_width,
    int *out_height,
    double *out_actual_seconds,
    char *error_buffer,
    size_t error_buffer_size);

AC_API void ac_video_close_session(AcVideoSession *session);

#ifdef __cplusplus
}
#endif

#endif
