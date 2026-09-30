#include "astracore.h"
#include <math.h>
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
typedef HMODULE Library;
static Library open_library(const char *path) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    wchar_t wide[32768];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, 32768)) return NULL;
    return LoadLibraryW(wide);
}
#define RESOLVE(library, name) GetProcAddress(library, name)
#define CLOSE(library) FreeLibrary(library)
#else
#include <dlfcn.h>
typedef void *Library;
static Library open_library(const char *path) { return dlopen(path, RTLD_NOW | RTLD_LOCAL); }
#define RESOLVE(library, name) dlsym(library, name)
#define CLOSE(library) dlclose(library)
#endif

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: dynamic_load NATIVE_LIBRARY MEDIA_FILE\n");
        return 1;
    }
    Library library = open_library(argv[1]);
    if (!library) {
        fprintf(stderr, "Native library or its dependency closure could not load: %s\n", argv[1]);
#if !defined(_WIN32)
        fprintf(stderr, "%s\n", dlerror());
#endif
        return 2;
    }
    typedef uint32_t (*Abi)(void);
    typedef int (*Probe)(const char *, ac_media_info *, char *, size_t);
    typedef AcVideoSession *(*Open)(const char *, char *, size_t);
    typedef int (*Grab)(AcVideoSession *, double, int, int, int, uint8_t *, size_t,
        int *, int *, double *, char *, size_t);
    typedef void (*Close)(AcVideoSession *);
    Abi abi = (Abi)RESOLVE(library, "ac_abi_version");
    Probe probe = (Probe)RESOLVE(library, "ac_probe_utf8");
    Open open = (Open)RESOLVE(library, "ac_video_open_session_utf8");
    Grab grab = (Grab)RESOLVE(library, "ac_video_session_grab_frame");
    Close close = (Close)RESOLVE(library, "ac_video_close_session");
    int result = 3;
    AcVideoSession *session = NULL;
    char error[512] = {0};
    if (!abi || !probe || !open || !grab || !close || abi() != AC_ABI_VERSION) goto done;
    ac_media_info info = {0};
    info.struct_size = sizeof(info);
    if (probe(argv[2], &info, error, sizeof(error)) || info.width != 96 || info.height != 64
        || !info.has_video || !info.has_audio || fabs(info.frame_rate - 10) > 0.001) goto done;
    session = open(argv[2], error, sizeof(error));
    if (!session) goto done;
    uint8_t pixels[48 * 32 * 4] = {0};
    int width = 0, height = 0;
    double actual = -1;
    if (grab(session, 0.5, 48, 32, 1, pixels, sizeof(pixels), &width, &height,
        &actual, error, sizeof(error)) || width != 48 || height != 32
        || fabs(actual - 0.5) > 0.001) goto done;
    for (size_t i = 3; i < sizeof(pixels); i += 4) if (pixels[i] != 255) goto done;
    result = 0;
    puts("Dynamically loaded native library and decoded a real media frame.");
done:
    if (session) close(session);
    CLOSE(library);
    if (result) fprintf(stderr, "Native dynamic-load contract failed: %s\n", error);
    return result;
}
