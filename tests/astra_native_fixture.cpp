// Test-only C ABI library. Built beside the isolated provider test executable;
// it must never be deployed as the application's media backend.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#define EXPORT extern "C" __declspec(dllexport)
#else
#define EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace {
struct MediaInfo {
    uint32_t size;
    double duration;
    int64_t bitRate;
    int32_t width, height;
    double fps;
    int32_t video, audio;
};
struct Session { std::atomic<bool> active{false}; };
std::mutex gateMutex;
std::condition_variable gateCv;
bool blockNext = false, blockNextClose = false, entered = false, released = false;
std::atomic<int> calls{0}, closeWhileActive{0}, liveSessions{0};

void awaitGate(bool closing = false) {
    std::unique_lock<std::mutex> lock(gateMutex);
    auto &block = closing ? blockNextClose : blockNext;
    if (!block) return;
    block = false;
    entered = true;
    gateCv.notify_all();
    gateCv.wait(lock, [] { return released; });
}

int grab(Session *session, double seconds, int width, int height, uint8_t *pixels,
         size_t capacity, int *outWidth, int *outHeight, double *actual) {
    if (session) session->active.store(true);
    ++calls;
    awaitGate();
    const size_t bytes = size_t(width) * size_t(height) * 4;
    if (width <= 0 || height <= 0 || bytes > capacity) {
        if (session) session->active.store(false);
        return -1;
    }
    std::memset(pixels, static_cast<int>(seconds) % 255, bytes);
    *outWidth = width;
    *outHeight = height;
    *actual = seconds;
    if (session) session->active.store(false);
    return 0;
}
}

EXPORT uint32_t ac_abi_version() { return 5; }
EXPORT int ac_probe_utf8(const char *, void *output, char *, size_t) {
    auto *info = static_cast<MediaInfo *>(output);
    if (info->size != sizeof(MediaInfo)) return -1;
    info->duration = 100;
    info->bitRate = 1000;
    info->width = 960;
    info->height = 540;
    info->fps = 24;
    info->video = 1;
    info->audio = 0;
    return 0;
}
EXPORT void *ac_video_open_session_utf8(const char *, char *, size_t) {
    ++liveSessions;
    return new Session;
}
EXPORT void ac_video_close_session(void *value) {
    auto *session = static_cast<Session *>(value);
    if (session->active.load()) ++closeWhileActive;
    awaitGate(true);
    delete session;
    --liveSessions;
}
EXPORT int ac_video_session_grab_frame(void *session, double seconds, int width, int height,
    int, uint8_t *pixels, size_t capacity, int *outWidth, int *outHeight, double *actual, char *, size_t) {
    return grab(static_cast<Session *>(session), seconds, width, height, pixels, capacity, outWidth, outHeight, actual);
}
EXPORT int ac_grab_frame_image_utf8(const char *, double seconds, int width, int height,
    int, uint8_t *pixels, size_t capacity, int *outWidth, int *outHeight, double *actual, char *, size_t) {
    return grab(nullptr, seconds, width, height, pixels, capacity, outWidth, outHeight, actual);
}
EXPORT int ac_extract_keyframes_utf8(const char *, double *timestamps, int64_t *indices, int count, char *, size_t) {
    awaitGate();
    if (count < 2) return 0;
    timestamps[0] = 0; timestamps[1] = 1;
    indices[0] = 0; indices[1] = 24;
    return 2;
}
EXPORT int ac_extract_timecodes_utf8(const char *, const char *, double *timestamps, int count, char *, size_t) {
    awaitGate();
    if (count < 2) return 0;
    timestamps[0] = 0; timestamps[1] = 1.0 / 24;
    return 2;
}

EXPORT void fixture_arm() {
    std::lock_guard<std::mutex> lock(gateMutex);
    blockNext = true;
    blockNextClose = false;
    entered = released = false;
}
EXPORT void fixture_arm_close() {
    std::lock_guard<std::mutex> lock(gateMutex);
    blockNextClose = true;
    blockNext = false;
    entered = released = false;
}
EXPORT int fixture_wait_entered(int milliseconds) {
    std::unique_lock<std::mutex> lock(gateMutex);
    return gateCv.wait_for(lock, std::chrono::milliseconds(milliseconds), [] { return entered; });
}
EXPORT void fixture_release() {
    std::lock_guard<std::mutex> lock(gateMutex);
    released = true;
    blockNext = false;
    blockNextClose = false;
    gateCv.notify_all();
}
EXPORT int fixture_calls() { return calls.load(); }
EXPORT int fixture_unsafe_closes() { return closeWhileActive.load(); }
EXPORT int fixture_live_sessions() { return liveSessions.load(); }
EXPORT int fixture_is_blocked() {
    std::lock_guard<std::mutex> lock(gateMutex);
    return entered && !released;
}
