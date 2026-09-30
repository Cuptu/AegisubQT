// Isolated test ABI. Returns malformed metadata without writing past buffers.
#include "../third_party/astracore/include/astracore.h"
#include <cstring>
#include <limits>
#ifdef _WIN32
#define EXPORT extern "C" __declspec(dllexport)
#else
#define EXPORT extern "C" __attribute__((visibility("default")))
#endif
namespace { int mode = 0, calls = 0; }
EXPORT void fixture_mode(int value) { mode = value; calls = 0; }
EXPORT int fixture_calls() { return calls; }
extern "C" uint32_t ac_abi_version() { return mode == 1 ? 6 : 5; }
extern "C" int ac_probe_utf8(const char *, ac_media_info *info, char *, size_t) {
    ++calls;
    info->duration_seconds = mode == 7 ? std::numeric_limits<double>::quiet_NaN() : 1;
    info->width = 8; info->height = 8; info->frame_rate = 24;
    info->has_video = 1;
    if (mode == 8) info->struct_size = 0;
    return 0;
}
extern "C" int ac_probe_hdr_utf8(const char *, ac_hdr_metadata *info, char *, size_t) {
    ++calls;
    info->bit_depth = 10;
    if (mode == 10) info->struct_size = 0;
    return 0;
}
static int frame(double seconds, int width, int height, uint8_t *pixels, size_t capacity,
    int *outWidth, int *outHeight, double *actual) {
    ++calls;
    std::memset(pixels, 255, capacity);
    *outWidth = mode == 2 ? width + 1 : mode == 3 ? std::numeric_limits<int>::max() : width;
    *outHeight = height;
    *actual = mode == 4 ? std::numeric_limits<double>::quiet_NaN() : seconds;
    return 0;
}
extern "C" int ac_grab_frame_image_utf8(const char *, double seconds, int width, int height, int,
    uint8_t *pixels, size_t capacity, int *outWidth, int *outHeight, double *actual, char *, size_t) {
    return frame(seconds, width, height, pixels, capacity, outWidth, outHeight, actual);
}
extern "C" AcVideoSession *ac_video_open_session_utf8(const char *, char *, size_t) {
    ++calls; return reinterpret_cast<AcVideoSession *>(uintptr_t(1));
}
extern "C" void ac_video_close_session(AcVideoSession *) { ++calls; }
extern "C" int ac_video_session_grab_frame(AcVideoSession *, double seconds, int width, int height, int,
    uint8_t *pixels, size_t capacity, int *outWidth, int *outHeight, double *actual, char *, size_t) {
    return frame(seconds, width, height, pixels, capacity, outWidth, outHeight, actual);
}
extern "C" int ac_extract_spectrogram_utf8(const char *, int, int fft, int, float *values,
    int maximum, int *bins, double *duration, char *, size_t) {
    ++calls;
    *bins = mode == 5 ? fft / 2 + 1 : fft / 2;
    *duration = 1;
    const int written = maximum < 2 ? maximum : 2;
    for (int i = 0; i < written * (fft / 2); ++i) values[i] = 0.25f;
    return mode == 6 ? maximum + 1 : written;
}
extern "C" int ac_extract_waveform_peaks_utf8(const char *, int, int, float *values,
    int maximum, double *duration, char *, size_t) {
    ++calls;
    values[0] = 0.25f;
    *duration = mode == 11 ? std::numeric_limits<double>::quiet_NaN() : 1;
    return mode == 9 ? maximum + 1 : 1;
}
