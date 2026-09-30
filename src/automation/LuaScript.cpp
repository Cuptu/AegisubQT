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
#include <cmath>
#include <limits>

namespace Automation {

// Forward-declare clipboard_init defined in LuaModules.cpp
int clipboard_init(lua_State *L);

LuaScript::LuaScript(const QString &filepath, const QStringList &includePaths)
    : m_filepath(QFileInfo(filepath).absoluteFilePath())
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
        if (m.toggleRef != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, m.toggleRef);
    }
    m_macros.clear();
    for (const auto &filter : m_filters) {
        luaL_unref(m_L, LUA_REGISTRYINDEX, filter.runRef);
        luaL_unref(m_L, LUA_REGISTRYINDEX, filter.configRef);
    }
    m_filters.clear();

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

    if (lua_isfunction(L, 5)) {
        lua_pushvalue(L, 5);
        m.toggleRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    script->m_macros.push_back(m);
    return 0;
}

int LuaScript::luaRegisterFilter(lua_State *L) {
    // LuaJIT errors longjmp: finish all argument checks before constructing Qt
    // values, so malformed registrations do not skip C++ destructors.
    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checktype(L, 3, LUA_TNUMBER);
    const double priority = lua_tonumber(L, 3);
    if (!std::isfinite(priority) || std::floor(priority) != priority ||
        priority < std::numeric_limits<int>::min() || priority > std::numeric_limits<int>::max())
        return luaL_error(L, "Filter priority must be a finite integer");
    luaL_checktype(L, 4, LUA_TFUNCTION);
    if (!lua_isnoneornil(L, 5)) luaL_checktype(L, 5, LUA_TFUNCTION);
    lua_getfield(L, LUA_REGISTRYINDEX, "current_lua_script");
    auto *script = static_cast<LuaScript *>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!script) return luaL_error(L, "No active automation script");
    size_t nameLength, descriptionLength;
    const char *name = lua_tolstring(L, 1, &nameLength);
    const char *description = lua_tolstring(L, 2, &descriptionLength);
    LuaExportFilter filter;
    filter.id = s_nextFilterId++;
    filter.name = QString::fromUtf8(name, static_cast<qsizetype>(nameLength));
    filter.description = QString::fromUtf8(description, static_cast<qsizetype>(descriptionLength));
    filter.priority = static_cast<int>(priority);
    lua_pushvalue(L, 4);
    filter.runRef = luaL_ref(L, LUA_REGISTRYINDEX);
    if (lua_isfunction(L, 5)) {
        lua_pushvalue(L, 5);
        filter.configRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    script->m_filters.push_back(std::move(filter));
    return 0;
}

namespace {
struct LuaStackRestore {
    lua_State *L;
    int top;
    explicit LuaStackRestore(lua_State *state) : L(state), top(lua_gettop(state)) {}
    ~LuaStackRestore() { lua_settop(L, top); }
};

// Automation dialog readback values are scalar strings, booleans and numbers.
// Reject unsupported QVariant values instead of silently coercing settings.
bool pushFilterSettings(lua_State *L, const QVariantMap &settings, QString &error) {
    lua_createtable(L, 0, static_cast<int>(settings.size()));
    for (auto it = settings.cbegin(); it != settings.cend(); ++it) {
        const auto key = it.key().toUtf8();
        lua_pushlstring(L, key.constData(), key.size());
        const auto &value = it.value();
        switch (value.typeId()) {
        case QMetaType::Bool: lua_pushboolean(L, value.toBool()); break;
        case QMetaType::QString: {
            const auto bytes = value.toString().toUtf8();
            lua_pushlstring(L, bytes.constData(), bytes.size());
            break;
        }
        case QMetaType::Int: case QMetaType::UInt: case QMetaType::LongLong:
        case QMetaType::ULongLong: case QMetaType::Double: case QMetaType::Float:
            if (!std::isfinite(value.toDouble())) {
                error = QStringLiteral("Non-finite filter setting: %1").arg(it.key());
                return false;
            }
            lua_pushnumber(L, value.toDouble());
            break;
        default:
            error = QStringLiteral("Unsupported filter setting: %1").arg(it.key());
            return false;
        }
        lua_rawset(L, -3);
    }
    return true;
}

// Dialog descriptors contain maps of scalar properties and arrays (dropdown
// items). Walk raw tables with a bounded budget; cycles, sparse arrays and
// functions must report a configuration error without invoking metamethods.
bool readFilterValue(lua_State *L, int index, QVariant &out, int depth, int &budget) {
    if (depth > 16 || --budget < 0) return false;
    if (index < 0) index = lua_gettop(L) + index + 1;
    switch (lua_type(L, index)) {
    case LUA_TBOOLEAN: out = bool(lua_toboolean(L, index)); return true;
    case LUA_TNUMBER: {
        const double number = lua_tonumber(L, index);
        if (!std::isfinite(number)) return false;
        out = number; return true;
    }
    case LUA_TSTRING: {
        size_t length;
        const char *bytes = lua_tolstring(L, index, &length);
        out = QString::fromUtf8(bytes, static_cast<qsizetype>(length)); return true;
    }
    case LUA_TTABLE: break;
    default: return false;
    }
    QVariantMap map;
    QMap<int, QVariant> array;
    lua_pushnil(L);
    while (lua_next(L, index)) {
        QVariant value;
        if (!readFilterValue(L, -1, value, depth + 1, budget)) {
            lua_pop(L, 2); return false;
        }
        if (lua_type(L, -2) == LUA_TSTRING) {
            size_t length;
            const char *bytes = lua_tolstring(L, -2, &length);
            map.insert(QString::fromUtf8(bytes, static_cast<qsizetype>(length)), value);
        } else if (lua_type(L, -2) == LUA_TNUMBER) {
            const double key = lua_tonumber(L, -2);
            if (!std::isfinite(key) || key < 1 || key > 10000 || std::floor(key) != key) {
                lua_pop(L, 2); return false;
            }
            array.insert(static_cast<int>(key), value);
        } else { lua_pop(L, 2); return false; }
        lua_pop(L, 1);
    }
    if (!map.isEmpty()) {
        if (!array.isEmpty()) return false;
        out = map; return true;
    }
    QVariantList list;
    for (auto it = array.cbegin(); it != array.cend(); ++it) {
        if (it.key() != list.size() + 1) return false;
        list.append(it.value());
    }
    out = list;
    return true;
}

QString filterError(lua_State *L) {
    const char *message = lua_tostring(L, -1);
    return message ? QString::fromUtf8(message) : QStringLiteral("Export filter callback failed");
}
}

bool LuaScript::filterConfig(int filterId, const LuaAssFileBridge &source, const QVariantMap &settings,
                             QVariantList &controlsOut, QString &errOut) {
    errOut.clear();
    if (!m_loaded || !m_L) { errOut = "Script is not loaded"; return false; }
    const LuaExportFilter *target = nullptr;
    for (const auto &filter : m_filters) if (filter.id == filterId) { target = &filter; break; }
    if (!target) { errOut = "Export filter not found"; return false; }
    if (target->configRef == LUA_NOREF) { controlsOut.clear(); return true; }
    const int configRef = target->configRef;
    LuaStackRestore stack(m_L);
    LuaAssFileBridge readOnly(m_L, source.getLines(), source.resolutionX(), source.resolutionY());
    readOnly.setReadOnly(true);
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, configRef);
    readOnly.pushToStack();
    if (!pushFilterSettings(m_L, settings, errOut)) return false;
    if (lua_pcall(m_L, 2, 1, 0)) { errOut = filterError(m_L); return false; }
    QVariant descriptor;
    int budget = 10000;
    if (!readFilterValue(m_L, -1, descriptor, 0, budget) || descriptor.typeId() != QMetaType::QVariantList) {
        errOut = "Export filter configuration must return an array of control tables";
        return false;
    }
    const auto controls = descriptor.toList();
    for (const auto &control : controls) {
        if (control.typeId() != QMetaType::QVariantMap) {
            errOut = "Export filter configuration contains an invalid control";
            return false;
        }
    }
    controlsOut = controls;
    return true;
}

bool LuaScript::runFilter(int filterId, std::vector<AssEntryData> &lines, int resX, int resY,
                          const QVariantMap &settings, QString &errOut) {
    errOut.clear();
    if (!m_loaded || !m_L) { errOut = "Script is not loaded"; return false; }
    int runRef = LUA_NOREF;
    for (const auto &filter : m_filters) if (filter.id == filterId) { runRef = filter.runRef; break; }
    if (runRef == LUA_NOREF) { errOut = "Export filter not found"; return false; }
    LuaStackRestore stack(m_L);
    LuaAssFileBridge copy(m_L, lines, resX, resY);
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, runRef);
    copy.pushToStack();
    if (!pushFilterSettings(m_L, settings, errOut)) return false;
    if (lua_pcall(m_L, 2, 0, 0)) { errOut = filterError(m_L); return false; }
    lines = copy.getLines();
    return true;
}

void LuaScript::initLuaState() {
    cleanupLuaState();

    m_L = luaL_newstate();
    if (!m_L) {
        m_errorMsg = "Failed to allocate Lua state";
        return;
    }

    preload_modules(m_L);
    QStringList modulePaths = m_includePaths;
    // Relative imports belong to the explicitly loaded script, independent of CWD.
    modulePaths.append(QFileInfo(m_filepath).dir().absolutePath());
    install_script_loaders(m_L, modulePaths);

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
    lua_pushcfunction(m_L, luaVideoSize);
    lua_setfield(m_L, -2, "video_size");

    lua_pushcfunction(m_L, luaRegisterFilter);
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

int LuaScript::luaVideoSize(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "current_lua_script");
    const auto *script = static_cast<LuaScript *>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!script || !script->m_videoContext.available) {
        lua_pushnil(L);
        return 1;
    }
    const auto &video = script->m_videoContext;
    lua_pushinteger(L, video.width);
    lua_pushinteger(L, video.height);
    lua_pushnumber(L, video.aspectRatio);
    lua_pushinteger(L, video.aspectRatioType);
    return 4;
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

LuaMacroState LuaScript::macroState(int macroId, LuaAssFileBridge &source,
                                   const std::vector<int> &selectedLines, int activeLine) {
    LuaMacroState state;
    if (!m_loaded || !m_L) { state.error = "Script is not loaded"; return state; }
    const LuaMacro *target = nullptr;
    for (const auto &macro : m_macros) if (macro.id == macroId) { target = &macro; break; }
    if (!target || target->runRef == LUA_NOREF) { state.error = "Macro function not found"; return state; }
    const auto macro = *target;
    state.description = macro.description;
    state.enabled = true;
    state.checkable = macro.toggleRef != LUA_NOREF;
    // Separate, short-lived userdata prevents a validator retaining a writable
    // handle to the execution bridge or sharing mutations with the toggle callback.
    auto call = [&](int ref, int results) {
        LuaAssFileBridge readOnly(m_L, source.getLines(), source.resolutionX(), source.resolutionY());
        readOnly.setReadOnly(true);
        lua_rawgeti(m_L, LUA_REGISTRYINDEX, ref);
        readOnly.pushToStack();
        lua_createtable(m_L, static_cast<int>(selectedLines.size()), 0);
        for (size_t i = 0; i < selectedLines.size(); ++i) {
            lua_pushinteger(m_L, selectedLines[i]);
            lua_rawseti(m_L, -2, static_cast<int>(i + 1));
        }
        if (activeLine > 0) lua_pushinteger(m_L, activeLine); else lua_pushnil(m_L);
        if (lua_pcall(m_L, 3, results, 0) == 0) return true;
        const char *error = lua_tostring(m_L, -1);
        state.error = error ? QString::fromUtf8(error) : QStringLiteral("Lua state callback failed");
        qWarning() << "[LuaScript] Macro state callback:" << state.error;
        lua_pop(m_L, 1);
        return false;
    };
    if (macro.valRef != LUA_NOREF) {
        if (call(macro.valRef, 2)) {
            state.enabled = lua_toboolean(m_L, -2);
            if (lua_isstring(m_L, -1)) {
                const auto help = QString::fromUtf8(lua_tostring(m_L, -1));
                if (!help.isEmpty()) state.description = help;
            }
            lua_pop(m_L, 2);
        } else state.enabled = false;
    }
    if (state.checkable && call(macro.toggleRef, 1)) {
        state.checked = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);
    }
    return state;
}

bool LuaScript::runMacro(int macroId, LuaAssFileBridge &bridge, const std::vector<int> &selectedLines, int activeLine, QString &errOut,
                         LuaMacroSelection *selectionOut) {
    if (selectionOut) *selectionOut = {};
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

    const int runRef = target->runRef;
    const auto state = macroState(macroId, bridge, selectedLines, activeLine);
    if (!state.enabled) {
        errOut = state.error.isEmpty() ? QStringLiteral("Macro is unavailable for the current selection") : state.error;
        return false;
    }
    // Bind bridge to this script's Lua state
    bridge.setLuaState(m_L);

    // Push macro function
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, runRef);

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

    if (selectionOut) {
        auto indexAt = [&](int index) -> int {
            if (lua_type(m_L, index) == LUA_TNUMBER) {
                const double value = lua_tonumber(m_L, index);
                if (std::isfinite(value) && std::floor(value) == value && value >= 1 &&
                        value <= std::numeric_limits<int>::max()) return static_cast<int>(value);
            }
            qWarning() << "[LuaScript] Ignoring invalid macro selection index";
            return 0;
        };
        if (lua_istable(m_L, -2)) {
            selectionOut->hasSelection = true;
            lua_pushnil(m_L);
            while (lua_next(m_L, -3)) {
                const int index = indexAt(-1);
                if (index) selectionOut->selectedLines.push_back(index);
                lua_pop(m_L, 1);
            }
        }
        if (!lua_isnil(m_L, -1)) {
            selectionOut->activeLine = indexAt(-1);
            selectionOut->hasActive = selectionOut->activeLine != 0;
        }
    }
    // Return values have been copied out; keep the Lua stack balanced.
    lua_pop(m_L, 2);
    return true;
}

} // namespace Automation

