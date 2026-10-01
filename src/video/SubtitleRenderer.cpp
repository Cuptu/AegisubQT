// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT
#include "SubtitleRenderer.h"
#include "../../third_party/libass_headers/ass.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QPainter>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

struct SubtitleRenderer::Impl {
    QLibrary module;
#ifdef Q_OS_WIN
    HMODULE handle = nullptr;
#endif
    ASS_Library *library = nullptr;
    ASS_Renderer *renderer = nullptr;
    ASS_Track *track = nullptr;
    QString failure;
#define ASS_FUNCTION(name) decltype(&name) name = nullptr;
    ASS_FUNCTION(ass_library_init)
    ASS_FUNCTION(ass_library_done)
    ASS_FUNCTION(ass_renderer_init)
    ASS_FUNCTION(ass_renderer_done)
    ASS_FUNCTION(ass_set_fonts)
    ASS_FUNCTION(ass_set_extract_fonts)
    ASS_FUNCTION(ass_set_frame_size)
    ASS_FUNCTION(ass_set_storage_size)
    ASS_FUNCTION(ass_read_memory)
    ASS_FUNCTION(ass_free_track)
    ASS_FUNCTION(ass_render_frame)
#undef ASS_FUNCTION
    QFunctionPointer resolve(const char *name) {
#ifdef Q_OS_WIN
        return reinterpret_cast<QFunctionPointer>(GetProcAddress(handle, name));
#else
        return module.resolve(name);
#endif
    }
    ~Impl() {
        if (track) ass_free_track(track);
        if (renderer) ass_renderer_done(renderer);
        if (library) ass_library_done(library);
#ifdef Q_OS_WIN
        if (handle) FreeLibrary(handle);
#endif
    }
};

SubtitleRenderer::SubtitleRenderer(const QString &libraryPath) : d(std::make_unique<Impl>()) {
    QStringList candidates;
    if (!libraryPath.isEmpty()) candidates << QFileInfo(libraryPath).absoluteFilePath();
    else {
        const QDir app(QCoreApplication::applicationDirPath());
        for (const auto &root : {app.absolutePath(), app.filePath("assets/bin"),
                                app.filePath("../Resources/assets/bin"), app.filePath("../Resources/astracore"),
                                app.filePath("../Frameworks"),
                                app.filePath("../lib")}) {
            for (const auto &file : QDir(root).entryList({"libass*.dll", "libass*.dylib", "libass.so*"}, QDir::Files))
                candidates << QDir(root).absoluteFilePath(file);
        }
#ifdef Q_OS_LINUX
        candidates << "/usr/lib/x86_64-linux-gnu/libass.so.9" << "/usr/lib/aarch64-linux-gnu/libass.so.9";
#endif
    }
    bool loaded = false;
    for (const auto &path : candidates) {
#ifdef Q_OS_WIN
        const auto nativePath = QDir::toNativeSeparators(path);
        d->handle = LoadLibraryExW(reinterpret_cast<LPCWSTR>(nativePath.utf16()), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        loaded = d->handle != nullptr;
#else
        d->module.setFileName(path);
        loaded = d->module.load();
#endif
        if (loaded) break;
    }
    if (!loaded) { d->failure = "The packaged libass subtitle renderer could not be loaded"; return; }
#define RESOLVE(name) d->name = reinterpret_cast<decltype(d->name)>(d->resolve(#name)); \
    if (!d->name) { d->failure = "Missing libass API: " #name; return; }
    RESOLVE(ass_library_init)
    RESOLVE(ass_library_done)
    RESOLVE(ass_renderer_init)
    RESOLVE(ass_renderer_done)
    RESOLVE(ass_set_fonts)
    RESOLVE(ass_set_extract_fonts)
    RESOLVE(ass_set_frame_size)
    RESOLVE(ass_set_storage_size)
    RESOLVE(ass_read_memory)
    RESOLVE(ass_free_track)
    RESOLVE(ass_render_frame)
#undef RESOLVE
    d->library = d->ass_library_init();
    if (!d->library) { d->failure = "libass initialization failed"; return; }
    d->ass_set_extract_fonts(d->library, 1);
    d->renderer = d->ass_renderer_init(d->library);
    if (!d->renderer) { d->failure = "libass renderer initialization failed"; return; }
    // CoreText on macOS, DirectWrite on Windows, Fontconfig on Linux.
    d->ass_set_fonts(d->renderer, nullptr, "Arial", ASS_FONTPROVIDER_AUTODETECT, nullptr, 1);
}
SubtitleRenderer::~SubtitleRenderer() = default;
bool SubtitleRenderer::available() const { return d->renderer != nullptr; }
QString SubtitleRenderer::error() const { return d->failure; }
bool SubtitleRenderer::setDocument(const QByteArray &ass) {
    if (!available()) return false;
    if (d->track) d->ass_free_track(d->track);
    auto data = ass;
    d->track = d->ass_read_memory(d->library, data.data(), data.size(), "UTF-8");
    return d->track != nullptr;
}
QImage SubtitleRenderer::render(int width, int height, qint64 milliseconds) {
    if (!available() || !d->track || width <= 0 || height <= 0 || width > 16384 || height > 16384) return {};
    d->ass_set_storage_size(d->renderer, width, height);
    d->ass_set_frame_size(d->renderer, width, height);
    int changed = 0;
    auto *images = d->ass_render_frame(d->renderer, d->track, milliseconds, &changed);
    QImage result(width, height, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    for (auto *image = images; image; image = image->next) {
        if (image->w <= 0 || image->h <= 0) continue;
        QImage bitmap(image->w, image->h, QImage::Format_ARGB32_Premultiplied);
        const int opacity = 255 - (image->color & 255);
        const int r = image->color >> 24, g = (image->color >> 16) & 255, b = (image->color >> 8) & 255;
        for (int y = 0; y < image->h; ++y) {
            auto *pixels = reinterpret_cast<QRgb *>(bitmap.scanLine(y));
            for (int x = 0; x < image->w; ++x) {
                const int a = (image->bitmap[y * image->stride + x] * opacity + 127) / 255;
                pixels[x] = qRgba((r*a+127)/255, (g*a+127)/255, (b*a+127)/255, a);
            }
        }
        painter.drawImage(image->dst_x, image->dst_y, bitmap);
    }
    return result;
}
