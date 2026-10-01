#include "astracore.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    char err[256];
    memset(err, 0, sizeof(err));

    // 1. Verify ABI and semantic versions
    if (ac_abi_version() != AC_ABI_VERSION) {
        fprintf(stderr, "ABI version mismatch: expected %u, got %u\n", AC_ABI_VERSION, ac_abi_version());
        return 1;
    }
    if (ac_version() != AC_BUILD_VERSION) {
        fprintf(stderr, "Semantic version mismatch: expected 0x%08X, got 0x%08X\n", AC_BUILD_VERSION, ac_version());
        return 1;
    }
    if (strcmp(ac_version_string(), "5.1.0") != 0) {
        fprintf(stderr, "Version string mismatch: %s\n", ac_version_string());
        return 1;
    }
    if (ac_has_feature("keyframes") != 1 || ac_has_feature("video_session") != 1 || ac_has_feature("__invalid_tag__") != 0) {
        fprintf(stderr, "Feature query check failed\n");
        return 1;
    }

    // 2. Validate trim media error handling on missing input
    if (ac_trim_media_utf8("__non_existent_file__", "out.mp4", 0.0, 5.0, 1, err, sizeof(err)) >= 0) {
        fprintf(stderr, "ac_trim_media unexpectedly succeeded on invalid file\n");
        return 2;
    }

    // 3. Validate audio speed factor bounds
    if (ac_change_audio_speed_utf8("any.wav", "out.wav", 0.1, err, sizeof(err)) >= 0) {
        fprintf(stderr, "ac_change_audio_speed allowed speed factor below 0.25\n");
        return 3;
    }
    if (ac_change_audio_speed_utf8("any.wav", "out.wav", 5.0, err, sizeof(err)) >= 0) {
        fprintf(stderr, "ac_change_audio_speed allowed speed factor above 4.0\n");
        return 4;
    }

    // 4. Validate audio stream extraction error handling
    if (ac_extract_audio_stream_utf8("__missing__", "out.m4a", 0, err, sizeof(err)) >= 0) {
        fprintf(stderr, "ac_extract_audio_stream unexpectedly succeeded on invalid file\n");
        return 5;
    }

    printf("AstraCore ABI v%u editing and speed tests passed.\n", AC_ABI_VERSION);
    return 0;
}
