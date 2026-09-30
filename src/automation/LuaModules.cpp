// Copyright (c) 2005 - 2026, Aegisub Project & Contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

#include "LuaModules.h"
#include "AutomationFileSystem.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QGuiApplication>
#include <QClipboard>
#include <QDebug>

#include <filesystem>
#include <memory>
#include <cstring>

extern "C" int luaopen_lpeg(lua_State *L);

namespace Automation {

char *strndup_malloc(const char *data, size_t len) {
    if (!data) return nullptr;
    char *ret = static_cast<char *>(malloc(len + 1));
    if (!ret) return nullptr;
    memcpy(ret, data, len);
    ret[len] = '\0';
    return ret;
}

char *strdup_malloc(const char *data) {
    if (!data) return nullptr;
    return strndup_malloc(data, strlen(data));
}

void do_register_lib_table(lua_State *L, std::initializer_list<const char *> types) {
    lua_getglobal(L, "require");
    lua_pushstring(L, "ffi");
    lua_call(L, 1, 1);

    for (auto type : types) {
        lua_getfield(L, -1, "cdef");
        lua_pushfstring(L, "typedef struct %s %s;", type, type);
        lua_call(L, 1, 0);
    }

    lua_getfield(L, -1, "cast");
    lua_remove(L, -2); // Remove ffi module table, leaving ffi.cast on stack
}

void do_register_lib_function(lua_State *L, const char *name, const char *type_name, void *func) {
    lua_pushvalue(L, -2); // Push ffi.cast
    lua_pushstring(L, type_name);
    lua_pushlightuserdata(L, func);
    lua_call(L, 2, 1);
    lua_setfield(L, -2, name);
}

// -------------------------------------------------------------
// unicode_impl
// -------------------------------------------------------------
static char *unicode_to_upper_case(const char *str, char **err) {
    if (!str) return nullptr;
    QString qstr = QString::fromUtf8(str).toUpper();
    QByteArray utf8 = qstr.toUtf8();
    return strndup_malloc(utf8.constData(), utf8.size());
}

static char *unicode_to_lower_case(const char *str, char **err) {
    if (!str) return nullptr;
    QString qstr = QString::fromUtf8(str).toLower();
    QByteArray utf8 = qstr.toUtf8();
    return strndup_malloc(utf8.constData(), utf8.size());
}

static char *unicode_to_fold_case(const char *str, char **err) {
    if (!str) return nullptr;
    QString qstr = QString::fromUtf8(str).toCaseFolded();
    QByteArray utf8 = qstr.toUtf8();
    return strndup_malloc(utf8.constData(), utf8.size());
}

static int luaopen_unicode_impl(lua_State *L) {
    do_register_lib_table(L, {});
    lua_createtable(L, 0, 3);
    do_register_lib_function(L, "to_upper_case", "char * (*)(const char *, char **)", (void*)unicode_to_upper_case);
    do_register_lib_function(L, "to_lower_case", "char * (*)(const char *, char **)", (void*)unicode_to_lower_case);
    do_register_lib_function(L, "to_fold_case",  "char * (*)(const char *, char **)", (void*)unicode_to_fold_case);
    lua_remove(L, -2); // remove ffi.cast
    return 1;
}

int luaopen_re_impl(lua_State *L);

// -------------------------------------------------------------
// lfs_impl
// -------------------------------------------------------------
struct DirectoryIterator {
    std::filesystem::directory_iterator it;
    std::filesystem::directory_iterator end;
};

// Every filesystem operation reports failures through the FFI error argument.
// No native exception may escape into a LuaJIT frame.
template<class Function>
auto filesystem_call(char **err, Function function) -> decltype(function()) {
    try { return function(); }
    catch (const std::exception &exception) { *err = strdup_malloc(exception.what()); }
    catch (...) { *err = strdup_malloc("Unknown filesystem error"); }
    return {};
}

static DirectoryIterator *lfs_dir_new(const char *path, char **err) {
    return filesystem_call(err, [&]() -> DirectoryIterator * {
        auto result = std::make_unique<DirectoryIterator>();
        std::error_code error;
        result->it = std::filesystem::directory_iterator(FileSystem::path(path), error);
        if (error) throw std::runtime_error("cannot open " + std::string(path) + ": " + error.message());
        return result.release();
    });
}

static char *lfs_dir_next(DirectoryIterator *it, char **err) {
    if (!it || it->it == it->end) return nullptr;
    return filesystem_call(err, [&]() -> char * {
        const auto filename = FileSystem::utf8(it->it->path().filename());
        ++it->it;
        return strndup_malloc(filename.data(), filename.size());
    });
}

static void lfs_dir_close(DirectoryIterator *it, char **) {
    if (it) it->it = std::filesystem::directory_iterator();
}

static void lfs_dir_free(DirectoryIterator *it, char **) { delete it; }

static const char *lfs_get_mode(const char *path, char **err) {
    return filesystem_call(err, [&] { return FileSystem::mode(FileSystem::path(path)); });
}

static int64_t lfs_get_mtime(const char *path, char **err) {
    return filesystem_call(err, [&] { return FileSystem::modification(FileSystem::path(path)); });
}

static uint64_t lfs_get_size(const char *path, char **err) {
    return filesystem_call(err, [&] { return FileSystem::size(FileSystem::path(path)); });
}

static bool lfs_chdir(const char *dir, char **err) {
    return filesystem_call(err, [&] { std::filesystem::current_path(FileSystem::path(dir)); return true; });
}

static char *lfs_currentdir(char **err) {
    return filesystem_call(err, [&] {
        const auto result = FileSystem::utf8(std::filesystem::current_path());
        return strndup_malloc(result.data(), result.size());
    });
}

static bool lfs_mkdir(const char *dir, char **err) {
    return filesystem_call(err, [&] { FileSystem::mkdir(FileSystem::path(dir)); return true; });
}

static bool lfs_rmdir(const char *dir, char **err) {
    return filesystem_call(err, [&] { FileSystem::remove(FileSystem::path(dir)); return true; });
}

static bool lfs_touch(const char *path, char **err) {
    return filesystem_call(err, [&] { FileSystem::touch(FileSystem::path(path)); return true; });
}

static int luaopen_lfs_impl(lua_State *L) {
    do_register_lib_table(L, {"DirectoryIterator"});
    lua_createtable(L, 0, 12);
    do_register_lib_function(L, "chdir",      "bool (*)(const char *, char **)", (void*)lfs_chdir);
    do_register_lib_function(L, "currentdir", "char * (*)(char **)", (void*)lfs_currentdir);
    do_register_lib_function(L, "mkdir",      "bool (*)(const char *, char **)", (void*)lfs_mkdir);
    do_register_lib_function(L, "rmdir",      "bool (*)(const char *, char **)", (void*)lfs_rmdir);
    do_register_lib_function(L, "touch",      "bool (*)(const char *, char **)", (void*)lfs_touch);
    do_register_lib_function(L, "get_mtime",  "int64_t (*)(const char *, char **)", (void*)lfs_get_mtime);
    do_register_lib_function(L, "get_mode",   "const char * (*)(const char *, char **)", (void*)lfs_get_mode);
    do_register_lib_function(L, "get_size",   "uint64_t (*)(const char *, char **)", (void*)lfs_get_size);
    do_register_lib_function(L, "dir_new",    "DirectoryIterator * (*)(const char *, char **)", (void*)lfs_dir_new);
    do_register_lib_function(L, "dir_free",   "void (*)(DirectoryIterator *, char **)", (void*)lfs_dir_free);
    do_register_lib_function(L, "dir_next",   "char * (*)(DirectoryIterator *, char **)", (void*)lfs_dir_next);
    do_register_lib_function(L, "dir_close",  "void (*)(DirectoryIterator *, char **)", (void*)lfs_dir_close);
    lua_remove(L, -2); // remove ffi.cast
    return 1;
}

// -------------------------------------------------------------
// clipboard
// -------------------------------------------------------------
static char *clipboard_get() {
    QClipboard *cb = QGuiApplication::clipboard();
    if (!cb) return nullptr;
    QString text = cb->text();
    if (text.isEmpty()) return nullptr;
    QByteArray utf8 = text.toUtf8();
    return strndup_malloc(utf8.constData(), utf8.size());
}

static bool clipboard_set(const char *str) {
    QClipboard *cb = QGuiApplication::clipboard();
    if (!cb || !str) return false;
    cb->setText(QString::fromUtf8(str));
    return true;
}

int clipboard_init(lua_State *L) {
    do_register_lib_table(L, {});
    lua_createtable(L, 0, 2);
    do_register_lib_function(L, "get", "char * (*)()", (void*)clipboard_get);
    do_register_lib_function(L, "set", "bool (*)(const char *)", (void*)clipboard_set);
    lua_remove(L, -2); // remove ffi.cast
    return 1;
}

// -------------------------------------------------------------
// Preload modules
// -------------------------------------------------------------
void preload_modules(lua_State *L) {
    luaL_openlibs(L);

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");

    lua_pushcfunction(L, luaopen_re_impl);
    lua_setfield(L, -2, "aegisub.__re_impl");

    lua_pushcfunction(L, luaopen_unicode_impl);
    lua_setfield(L, -2, "aegisub.__unicode_impl");

    lua_pushcfunction(L, luaopen_lfs_impl);
    lua_setfield(L, -2, "aegisub.__lfs_impl");

    lua_pushcfunction(L, luaopen_lpeg);
    lua_setfield(L, -2, "lpeg");

    lua_pop(L, 2); // pop preload, package
}

// -------------------------------------------------------------
// Script Loading & Module Resolution
// -------------------------------------------------------------
bool load_script_file(lua_State *L, const QString &filepath, QString &errorMsg) {
    QFile f(filepath);
    if (!f.open(QIODevice::ReadOnly)) {
        errorMsg = QString("Could not open file: %1").arg(filepath);
        return false;
    }
    QByteArray data = f.readAll();
    f.close();

    const char *buff = data.constData();
    size_t size = data.size();

    // Discard UTF-8 BOM if present
    if (size >= 3 && (unsigned char)buff[0] == 0xEF && (unsigned char)buff[1] == 0xBB && (unsigned char)buff[2] == 0xBF) {
        buff += 3;
        size -= 3;
    }

    if (!filepath.endsWith(".moon", Qt::CaseInsensitive)) {
        if (luaL_loadbuffer(L, buff, size, filepath.toUtf8().constData()) != 0) {
            errorMsg = QString::fromUtf8(lua_tostring(L, -1));
            lua_pop(L, 1);
            return false;
        }
        return true;
    }

    // MoonScript compilation via registered moonscript compiler
    lua_getfield(L, LUA_REGISTRYINDEX, "moonscript");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        errorMsg = "MoonScript engine is not available";
        return false;
    }
    lua_pushlstring(L, buff, size);
    lua_pushstring(L, filepath.toUtf8().constData());
    if (lua_pcall(L, 2, 2, 0) != 0) {
        errorMsg = QString::fromUtf8(lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    if (lua_isnil(L, -2)) {
        errorMsg = QString::fromUtf8(lua_tostring(L, -1));
        lua_pop(L, 2);
        return false;
    }
    lua_pop(L, 1); // remove extra nil/unused return value
    return true;
}

// A negative result leaves an error string on the Lua stack. The thin Lua
// callback raises it only after all path/file temporaries have been destroyed.
static int prepare_module_load(lua_State *L) {
    const char *modname = lua_tostring(L, 1);
    if (!modname) return 1;

    QString module = QString::fromUtf8(modname);
    module.replace('.', '/');

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    QString packagePaths = QString::fromUtf8(lua_tostring(L, -1));
    lua_pop(L, 2);

    QStringList paths = packagePaths.split(';', Qt::SkipEmptyParts);
    for (const QString &tmpl : paths) {
        QString filename = tmpl;
        filename.replace('?', module);

        // If .lua is specified and a .moon counterpart exists, prefer .moon
        if (filename.endsWith(".lua", Qt::CaseInsensitive)) {
            QString moonname = filename.left(filename.length() - 4) + ".moon";
            if (QFileInfo::exists(moonname)) {
                filename = moonname;
            }
        }

        if (QFileInfo::exists(filename)) {
            QString err;
            if (load_script_file(L, filename, err)) {
                return 1;
            } else {
                lua_pushfstring(L, "Error loading module '%s':\n%s", filename.toUtf8().constData(), err.toUtf8().constData());
                return -1;
            }
        }
    }

    lua_pushfstring(L, "\n\tno file matching module in custom automation loader: %s", modname);
    return 1;
}

static int custom_module_loader(lua_State *L) {
    const int result = prepare_module_load(L);
    return result < 0 ? lua_error(L) : result;
}

// Finish Qt object lifetimes before raising a Lua error or executing Lua code.
// LuaJIT's error unwinder must not cross live C++ objects owned by the loader.
static bool prepare_lua_include(lua_State *L) {
    const char *filename_c = lua_tostring(L, 1);
    if (!filename_c) {
        lua_pushliteral(L, "include() requires a string filename");
        return false;
    }
    QString filename = QString::fromUtf8(filename_c);

    lua_getfield(L, LUA_REGISTRYINDEX, "include_paths");
    QStringList includePaths;
    if (lua_istable(L, -1)) {
        int n = (int)lua_objlen(L, -1);
        for (int i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            includePaths.append(QString::fromUtf8(lua_tostring(L, -1)));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, LUA_REGISTRYINDEX, "script_dir");
    if (lua_isstring(L, -1)) {
        includePaths.prepend(QString::fromUtf8(lua_tostring(L, -1)));
    }
    lua_pop(L, 1);

    QString targetPath;
    if (QFileInfo(filename).isAbsolute() && QFileInfo::exists(filename)) {
        targetPath = filename;
    } else {
        for (const QString &dir : includePaths) {
            QString candidate = QDir(dir).filePath(filename);
            if (QFileInfo::exists(candidate)) {
                targetPath = candidate;
                break;
            }
            if (filename.endsWith(".lua", Qt::CaseInsensitive)) {
                QString moonCandidate = QDir(dir).filePath(filename.left(filename.length() - 4) + ".moon");
                if (QFileInfo::exists(moonCandidate)) {
                    targetPath = moonCandidate;
                    break;
                }
            }
        }
    }

    if (targetPath.isEmpty()) {
        lua_pushfstring(L, "Lua include not found: %s", filename_c);
        return false;
    }

    QString err;
    if (!load_script_file(L, targetPath, err)) {
        lua_pushfstring(L, "Error loading Lua include '%s':\n%s", targetPath.toUtf8().constData(), err.toUtf8().constData());
        return false;
    }
    return true;
}

static int custom_lua_include(lua_State *L) {
    if (!prepare_lua_include(L)) return lua_error(L);
    int pretop = lua_gettop(L) - 1;
    lua_call(L, 0, LUA_MULTRET);
    return lua_gettop(L) - pretop;
}

bool install_script_loaders(lua_State *L, const QStringList &include_paths) {
    // 1. Persist include_paths in the Lua registry for include() resolution.
    lua_createtable(L, include_paths.size(), 0);
    for (int i = 0; i < include_paths.size(); ++i) {
        lua_pushstring(L, include_paths[i].toUtf8().constData());
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, LUA_REGISTRYINDEX, "include_paths");

    // 2. Populate package.path with user and system search directories.
    lua_getglobal(L, "package");
    QString pPath;
    for (const QString &p : include_paths) {
        pPath += QString("%1/?.lua;%1/?/init.lua;").arg(p);
    }
    // Do not inherit Lua's ./?.lua paths or LUA_PATH/LUA_CPATH. Native and
    // Lua modules resolve only in the configured include/script directories.
    QString cPath;
    for (const auto &path : include_paths) {
#ifdef Q_OS_WIN
        cPath += path + "/?.dll;";
#else
        cPath += path + "/?.so;";
#endif
    }
    lua_pushstring(L, cPath.toUtf8().constData());
    lua_setfield(L, -2, "cpath");

    lua_pushstring(L, pPath.toUtf8().constData());
    lua_setfield(L, -2, "path");

    // 3. Replace the standard path loader (package.loaders[2]) with our custom loader
    //    that supports transparent .moon compilation and Aegisub include search paths.
    lua_getfield(L, -1, "loaders");
    if (lua_istable(L, -1)) {
        lua_pushcfunction(L, custom_module_loader);
        lua_rawseti(L, -2, 2);
    }
    lua_pop(L, 2); // pop loaders, package

    // 4. Sandbox file execution by replacing dofile/loadfile with Aegisub's include().
    lua_pushnil(L);
    lua_setglobal(L, "dofile");
    lua_pushnil(L);
    lua_setglobal(L, "loadfile");
    lua_pushcfunction(L, custom_lua_include);
    lua_setglobal(L, "include");

    // 5. Cache MoonScript compiler in the Lua registry.
    if (luaL_dostring(L, "return require('moonscript').loadstring") == 0) {
        lua_setfield(L, LUA_REGISTRYINDEX, "moonscript");
    } else {
        qWarning() << "[LuaModules] Could not preload moonscript:" << lua_tostring(L, -1);
        lua_pop(L, 1);
    }

    return true;
}

} // namespace Automation
