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
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <vector>
#include "LuaAssFileBridge.h"

namespace Automation {

/// Represents an Automation 4 macro registered by a Lua script.
/// Holds registry references for execution and validation callbacks.
struct LuaMacro {
    int id = 0;
    QString name;
    QString description;
    int runRef = LUA_NOREF;
    int valRef = LUA_NOREF;
    int toggleRef = LUA_NOREF;
};
struct LuaExportFilter {
    int id = 0;
    QString name;
    QString description;
    int priority = 0;
    int runRef = LUA_NOREF;
    int configRef = LUA_NOREF;
};
struct LuaMacroState {
    bool enabled = false;
    bool checkable = false;
    bool checked = false;
    QString description;
    QString error;
};
struct LuaMacroSelection {
    bool hasSelection = false;
    bool hasActive = false;
    std::vector<int> selectedLines;
    int activeLine = 0;
};
struct LuaVideoContext {
    bool available = false;
    int width = 0;
    int height = 0;
    double aspectRatio = 0;
    int aspectRatioType = 0;
};

/// Encapsulates an isolated Lua execution context for an Automation 4 script.
/// Manages script lifetime, global environment bindings, and macro execution.
class LuaScript {
public:
    explicit LuaScript(const QString &filepath, const QStringList &includePaths);
    ~LuaScript();

    bool load();
    void reload();

    QString filepath() const { return m_filepath; }
    QString name() const { return m_name; }
    QString description() const { return m_description; }
    QString author() const { return m_author; }
    QString version() const { return m_version; }
    bool isLoaded() const { return m_loaded; }
    QString errorString() const { return m_errorMsg; }

    const std::vector<LuaMacro> &macros() const { return m_macros; }
    const std::vector<LuaExportFilter> &filters() const { return m_filters; }
    bool filterConfig(int filterId, const LuaAssFileBridge &source, const QVariantMap &settings,
                      QVariantList &controlsOut, QString &errOut);
    // Commit output only after a successful call; retained Lua userdata expires
    // with the temporary export copy and cannot mutate the editor document.
    bool runFilter(int filterId, std::vector<AssEntryData> &lines, int resX, int resY,
                   const QVariantMap &settings, QString &errOut);
    void setVideoContext(const LuaVideoContext &context) { m_videoContext = context; }
    LuaMacroState macroState(int macroId, LuaAssFileBridge &bridge,
                            const std::vector<int> &selectedLines, int activeLine);

    /// Execute a registered macro against the provided subtitle bridge.
    /// Invariant: Passes (subtitles, selected_lines_table, active_line_index) to the Lua function.
    bool runMacro(int macroId, LuaAssFileBridge &bridge, const std::vector<int> &selectedLines, int activeLine, QString &errOut,
                  LuaMacroSelection *selectionOut = nullptr);

    /// Lua C-function callback for aegisub.register_macro().
    static int luaRegisterMacro(lua_State *L);
    static int luaRegisterFilter(lua_State *L);
    static int luaVideoSize(lua_State *L);

private:
    void initLuaState();
    void cleanupLuaState();

    QString m_filepath;
    QStringList m_includePaths;
    LuaVideoContext m_videoContext;
    lua_State *m_L = nullptr;

    QString m_name;
    QString m_description;
    QString m_author;
    QString m_version;
    bool m_loaded = false;
    QString m_errorMsg;

    std::vector<LuaMacro> m_macros;
    std::vector<LuaExportFilter> m_filters;
    static inline int s_nextMacroId = 1;
    static inline int s_nextFilterId = 1;
};

} // namespace Automation

