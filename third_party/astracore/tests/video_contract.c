#include "astracore.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 3) return 1;
    char error[512] = {0};
    double times[32] = {0}, keys[32] = {0};
    int64_t indices[32] = {0};
    int count = ac_extract_timecodes_utf8(argv[1], NULL, times, 32, error, sizeof(error));
    if (count != 10) {
        fprintf(stderr, "expected 10 frame timestamps, got %d: %s\n", count, error);
        return 2;
    }
    for (int i = 0; i < count; ++i) {
        if (fabs(times[i] - i * 0.1) > 0.001) return 3;
    }
    count = ac_extract_keyframes_utf8(argv[1], keys, indices, 32, error, sizeof(error));
    if (count < 1 || indices[0] != 0 || fabs(keys[0]) > 0.001) return 4;
    unsigned char image[96 * 64 * 4];
    memset(image, 0, sizeof(image));
    int width = 0, height = 0;
    double actual = -1;
    int result = ac_grab_frame_image_utf8(argv[1], 0.5, 96, 64, 1,
        image, sizeof(image), &width, &height, &actual, error, sizeof(error));
    if (result || width != 96 || height != 64 || fabs(actual - 0.5) > 0.001) {
        fprintf(stderr, "stateless frame failed: result=%d size=%dx%d pts=%f %s\n",
            result, width, height, actual, error);
        return 5;
    }
    int colored = 0;
    for (size_t i = 0; i < sizeof(image); i += 4) {
        if (image[i + 3] != 255) return 6;
        if (image[i] || image[i + 1] || image[i + 2]) ++colored;
    }
    if (!colored) return 7;
    AcVideoSession *session = ac_video_open_session_utf8(argv[1], error, sizeof(error));
    if (!session) return 8;
    const double requests[] = {0.3, 0.3, 0.6, 0.1, 0.9, 0.9};
    int failed = 0;
    for (size_t i = 0; i < sizeof(requests) / sizeof(requests[0]); ++i) {
        result = ac_video_session_grab_frame(session, requests[i], 48, 32, 2,
            image, sizeof(image), &width, &height, &actual, error, sizeof(error));
        if (result || width != 48 || height != 32 || fabs(actual - requests[i]) > 0.001) {
            fprintf(stderr, "session request %zu target=%f got=%f result=%d %s\n",
                i, requests[i], actual, result, error);
            failed = 9;
            break;
        }
    }
    ac_video_close_session(session);
    if (failed) return failed;
    ac_hdr_metadata metadata = {0};
    metadata.struct_size = sizeof(metadata);
    if (ac_probe_hdr_utf8(argv[1], &metadata, error, sizeof(error)) < 0) return 10;
    if (metadata.color_range < 0 || metadata.color_range > 2) return 11;
    ac_hdr_metadata legacy = {0};
    legacy.struct_size = offsetof(ac_hdr_metadata, color_range);
    legacy.color_range = 0x12345678;
    if (ac_probe_hdr_utf8(argv[1], &legacy, error, sizeof(error)) < 0) return 12;
    if (legacy.color_range != 0x12345678) return 13;
    memset(&metadata, 0, sizeof(metadata));
    metadata.struct_size = sizeof(metadata);
    if (ac_probe_hdr_utf8(argv[2], &metadata, error, sizeof(error)) < 0) return 14;
    if (metadata.color_space != 1 || metadata.color_range != 1) {
        fprintf(stderr, "expected BT.709/Limited metadata, got space=%d range=%d\n",
            metadata.color_space, metadata.color_range);
        return 15;
    }
    puts("Real decode, repeated seek, scaling, keyframes and timestamps passed.");
    return 0;
}
