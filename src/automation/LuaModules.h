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

#pragma once

#include <lua.hpp>
#include <QStringList>
#include <initializer_list>

namespace Automation {

void do_register_lib_table(lua_State *L, std::initializer_list<const char *> types);
void do_register_lib_function(lua_State *L, const char *name, const char *type_name, void *func);

/// Preload built-in C/FFI module shims into package.preload
/// (lpeg, aegisub.__unicode_impl, aegisub.__re_impl, aegisub.__lfs_impl).
void preload_modules(lua_State *L);

/// Configure search paths, install custom module loader for package.loaders[2],
/// expose include(), and initialize the MoonScript compiler.
bool install_script_loaders(lua_State *L, const QStringList &include_paths);

/// Load a .lua or .moon script file into the lua_State, stripping UTF-8 BOM if present.
bool load_script_file(lua_State *L, const QString &filepath, QString &errorMsg);

/// Duplicate a buffer or null-terminated string via malloc() so it can be managed
/// across the Lua FFI boundary and released by ffi.C.free().
char *strndup_malloc(const char *data, size_t len);
char *strdup_malloc(const char *data);

} // namespace Automation

