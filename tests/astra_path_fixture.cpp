#include "../third_party/astracore/include/astracore.h"
#ifdef _WIN32
extern "C" __declspec(dllimport) int astra_path_test_dimension();
#else
extern "C" int astra_path_test_dimension();
#endif
extern "C" uint32_t ac_abi_version() { return 5; }
extern "C" int ac_probe_utf8(const char *, ac_media_info *info, char *, size_t) {
    info->width = astra_path_test_dimension();
    info->height = 8; info->duration_seconds = 1; info->frame_rate = 24;
    info->has_video = 1;
    return 0;
}
