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

#include "LuaScript.h"
#include "LuaModules.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>

namespace Automation {

// Forward-declare clipboard_init defined in LuaModules.cpp
int clipboard_init(lua_State *L);

LuaScript::LuaScript(const QString &filepath, const QStringList &includePaths)
    : m_filepath(filepath)
    , m_includePaths(includePaths)
{
    m_name = QFileInfo(filepath).baseName();
}

LuaScript::~LuaScript() {
    cleanupLuaState();
}

void LuaScript::cleanupLuaState() {
    if (!m_L) return;

    for (const auto &m : m_macros) {
        if (m.runRef != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, m.runRef);
        if (m.valRef != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, m.valRef);
    }
    m_macros.clear();

    lua_close(m_L);
    m_L = nullptr;
    m_loaded = false;
}

static int lua_dummy(lua_State *L) {
    return 0;
}

static int lua_gettext(lua_State *L) {
    if (lua_gettop(L) >= 1) {
        lua_pushvalue(L, 1);
        return 1;
    }
    lua_pushstring(L, "");
    return 1;
}

static int lua_progress_is_cancelled(lua_State *L) {
    lua_pushboolean(L, 0);
    return 1;
}

static int lua_debug_out(lua_State *L) {
    int top = lua_gettop(L);
    QString msg;
    for (int i = 1; i <= top; ++i) {
        if (lua_isstring(L, i)) {
            msg += QString::fromUtf8(lua_tostring(L, i)) + " ";
        }
    }
    qInfo() << "[LuaScript Log]" << msg;
    return 0;
}

int LuaScript::luaRegisterMacro(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "current_lua_script");
    auto script = static_cast<LuaScript *>(lua_touserdata(L, -1));
    lua_pop(L, 1);

    if (!script) return 0;

    QString name = lua_isstring(L, 1) ? QString::fromUtf8(lua_tostring(L, 1)) : "Unnamed Macro";
    QString desc = lua_isstring(L, 2) ? QString::fromUtf8(lua_tostring(L, 2)) : "";

    LuaMacro m;
    m.id = s_nextMacroId++;
    m.name = name;
    m.description = desc;

    if (lua_isfunction(L, 3)) {
        lua_pushvalue(L, 3);
        m.runRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    if (lua_gettop(L) >= 4 && lua_isfunction(L, 4)) {
        lua_pushvalue(L, 4);
        m.valRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    script->m_macros.push_back(m);
    return 0;
}

void LuaScript::initLuaState() {
    cleanupLuaState();

    m_L = luaL_newstate();
    if (!m_L) {
        m_errorMsg = "Failed to allocate Lua state";
        return;
    }

    preload_modules(m_L);
    install_script_loaders(m_L, m_includePaths);

    // Save script pointer in registry for C-callback dispatch
    lua_pushlightuserdata(m_L, this);
    lua_setfield(m_L, LUA_REGISTRYINDEX, "current_lua_script");

    QString dir = QFileInfo(m_filepath).dir().absolutePath();
    lua_pushstring(m_L, dir.toUtf8().constData());
    lua_setfield(m_L, LUA_REGISTRYINDEX, "script_dir");

    // Construct global "aegisub" automation table
    lua_createtable(m_L, 0, 16);

    lua_pushinteger(m_L, 4);
    lua_setfield(m_L, -2, "lua_automation_version");

    lua_pushcfunction(m_L, luaRegisterMacro);
    lua_setfield(m_L, -2, "register_macro");

    lua_pushcfunction(m_L, lua_dummy);
    lua_setfield(m_L, -2, "register_filter");

    lua_pushcfunction(m_L, LuaAssFileBridge::textExtents);
    lua_setfield(m_L, -2, "text_extents");

    lua_pushcfunction(m_L, lua_gettext);
    lua_setfield(m_L, -2, "gettext");

    lua_pushcfunction(m_L, lua_gettext);
    lua_setfield(m_L, -2, "decode_path");

    lua_pushcfunction(m_L, clipboard_init);
    lua_setfield(m_L, -2, "__init_clipboard");

    lua_pushcfunction(m_L, lua_dummy);
    lua_setfield(m_L, -2, "__raise_warning");

    // aegisub.progress table
    lua_createtable(m_L, 0, 4);
    lua_pushcfunction(m_L, lua_dummy); lua_setfield(m_L, -2, "set");
    lua_pushcfunction(m_L, lua_dummy); lua_setfield(m_L, -2, "task");
    lua_pushcfunction(m_L, lua_dummy); lua_setfield(m_L, -2, "title");
    lua_pushcfunction(m_L, lua_progress_is_cancelled); lua_setfield(m_L, -2, "is_cancelled");
    lua_setfield(m_L, -2, "progress");

    // aegisub.debug table
    lua_createtable(m_L, 0, 2);
    lua_pushcfunction(m_L, lua_debug_out); lua_setfield(m_L, -2, "out");
    lua_setfield(m_L, -2, "debug");

    // aegisub.log shorthand
    lua_pushcfunction(m_L, lua_debug_out);
    lua_setfield(m_L, -2, "log");

    lua_setglobal(m_L, "aegisub");
}

bool LuaScript::load() {
    initLuaState();
    if (!m_L) return false;

    QString err;
    if (!load_script_file(m_L, m_filepath, err)) {
        m_errorMsg = QString("Error compiling '%1':\n%2").arg(m_filepath, err);
        qWarning() << "[LuaScript]" << m_errorMsg;
        cleanupLuaState();
        return false;
    }

    if (lua_pcall(m_L, 0, 0, 0) != 0) {
        m_errorMsg = QString("Runtime error initializing '%1':\n%2").arg(m_filepath, QString::fromUtf8(lua_tostring(m_L, -1)));
        qWarning() << "[LuaScript]" << m_errorMsg;
        lua_pop(m_L, 1);
        cleanupLuaState();
        return false;
    }

    // Extract metadata globals exported by the script
    lua_getglobal(m_L, "script_name");
    if (lua_isstring(m_L, -1)) m_name = QString::fromUtf8(lua_tostring(m_L, -1));
    lua_pop(m_L, 1);

    lua_getglobal(m_L, "script_description");
    if (lua_isstring(m_L, -1)) m_description = QString::fromUtf8(lua_tostring(m_L, -1));
    lua_pop(m_L, 1);

    lua_getglobal(m_L, "script_author");
    if (lua_isstring(m_L, -1)) m_author = QString::fromUtf8(lua_tostring(m_L, -1));
    lua_pop(m_L, 1);

    lua_getglobal(m_L, "script_version");
    if (lua_isstring(m_L, -1)) m_version = QString::fromUtf8(lua_tostring(m_L, -1));
    lua_pop(m_L, 1);

    m_loaded = true;
    m_errorMsg.clear();
    return true;
}

void LuaScript::reload() {
    load();
}

bool LuaScript::runMacro(int macroId, LuaAssFileBridge &bridge, const std::vector<int> &selectedLines, int activeLine, QString &errOut) {
    if (!m_loaded || !m_L) {
        errOut = "Script is not loaded";
        return false;
    }

    const LuaMacro *target = nullptr;
    for (const auto &m : m_macros) {
        if (m.id == macroId) {
            target = &m;
            break;
        }
    }

    if (!target || target->runRef == LUA_NOREF) {
        errOut = "Macro function not found";
        return false;
    }

    // Bind bridge to this script's Lua state
    bridge.setLuaState(m_L);

    // Push macro function
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, target->runRef);

    // Automation 4 ABI:
    // Arg 1: Subtitles bridge userdata
    bridge.pushToStack();

    // Arg 2: selected_lines table (1-based line indices)
    lua_createtable(m_L, (int)selectedLines.size(), 0);
    for (size_t i = 0; i < selectedLines.size(); ++i) {
        lua_pushinteger(m_L, selectedLines[i]);
        lua_rawseti(m_L, -2, (int)(i + 1));
    }

    // Arg 3: active_line index
    lua_pushinteger(m_L, activeLine);

    // Execute macro callback: returns (new_selected_lines, new_active_line)
    if (lua_pcall(m_L, 3, 2, 0) != 0) {
        errOut = QString::fromUtf8(lua_tostring(m_L, -1));
        lua_pop(m_L, 1);
        return false;
    }

    // Discard the 2 return values
    lua_pop(m_L, 2);
    return true;
}

} // namespace Automation

