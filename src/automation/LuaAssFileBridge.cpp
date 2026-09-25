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

#include "LuaAssFileBridge.h"
#include "LuaTextExtents.h"
#include "AegisubCoreBridge.h"

#include <QRegularExpression>
#include <QDebug>
#include <cmath>
#include <algorithm>

namespace Automation {

struct BridgeHolder {
    LuaAssFileBridge *bridge = nullptr;
    std::shared_ptr<bool> alive;
};

LuaAssFileBridge::LuaAssFileBridge(lua_State *L, std::vector<AssEntryData> initialLines, int resX, int resY)
    : m_L(L)
    , m_lines(std::move(initialLines))
    , m_resX(resX)
    , m_resY(resY)
    , m_alive(std::make_shared<bool>(true))
{
    if (m_L) {
        setLuaState(m_L);
    }
}

LuaAssFileBridge::~LuaAssFileBridge() {
    if (m_alive) {
        *m_alive = false;
    }
    if (m_L) {
        lua_pushnil(m_L);
        lua_setfield(m_L, LUA_REGISTRYINDEX, "active_lua_assfile_bridge");
    }
}

void LuaAssFileBridge::setLuaState(lua_State *L) {
    m_L = L;
    if (!m_L) return;

    // Save bridge pointer in registry for global callbacks (parse_karaoke_data, set_undo_point)
    lua_pushlightuserdata(m_L, this);
    lua_setfield(m_L, LUA_REGISTRYINDEX, "active_lua_assfile_bridge");

    // Register active functions into aegisub table
    lua_getglobal(m_L, "aegisub");
    if (lua_istable(m_L, -1)) {
        lua_pushcfunction(m_L, parseKaraokeData);
        lua_setfield(m_L, -2, "parse_karaoke_data");

        lua_pushcfunction(m_L, setUndoPoint);
        lua_setfield(m_L, -2, "set_undo_point");

        lua_pushcfunction(m_L, textExtents);
        lua_setfield(m_L, -2, "text_extents");
    }
    lua_pop(m_L, 1);
}

LuaAssFileBridge *LuaAssFileBridge::getBridge(lua_State *L, int idx) {
    if (!lua_isuserdata(L, idx)) return nullptr;
    auto holder = static_cast<BridgeHolder *>(lua_touserdata(L, idx));
    if (!holder || !holder->alive || !*(holder->alive)) return nullptr;
    return holder->bridge;
}

// -------------------------------------------------------------
// Lua Metatable Closures
// -------------------------------------------------------------
static int meta_index(lua_State *L) {
    auto b = LuaAssFileBridge::getBridge(L, 1);
    return b ? b->indexRead(L) : 0;
}

static int meta_newindex(lua_State *L) {
    auto b = LuaAssFileBridge::getBridge(L, 1);
    return b ? b->indexWrite(L) : 0;
}

static int meta_len(lua_State *L) {
    auto b = LuaAssFileBridge::getBridge(L, 1);
    return b ? b->getLen(L) : 0;
}

static int meta_ipairs(lua_State *L) {
    auto b = LuaAssFileBridge::getBridge(L, 1);
    return b ? b->ipairs(L) : 0;
}

static int meta_gc(lua_State *L) {
    if (lua_isuserdata(L, 1)) {
        auto holder = static_cast<BridgeHolder *>(lua_touserdata(L, 1));
        if (holder) {
            holder->~BridgeHolder();
        }
    }
    return 0;
}

static int closure_delete(lua_State *L) {
    auto b = LuaAssFileBridge::getBridge(L, lua_upvalueindex(1));
    return b ? b->del(L) : 0;
}

static int closure_deleterange(lua_State *L) {
    auto b = LuaAssFileBridge::getBridge(L, lua_upvalueindex(1));
    return b ? b->deleteRange(L) : 0;
}

static int closure_insert(lua_State *L) {
    auto b = LuaAssFileBridge::getBridge(L, lua_upvalueindex(1));
    return b ? b->insert(L) : 0;
}

static int closure_append(lua_State *L) {
    auto b = LuaAssFileBridge::getBridge(L, lua_upvalueindex(1));
    return b ? b->append(L) : 0;
}

static int closure_script_resolution(lua_State *L) {
    auto b = LuaAssFileBridge::getBridge(L, lua_upvalueindex(1));
    return b ? b->getScriptResolution(L) : 0;
}

static int closure_iternext(lua_State *L) {
    auto b = LuaAssFileBridge::getBridge(L, lua_upvalueindex(1));
    return b ? b->iterNext(L) : 0;
}

void LuaAssFileBridge::pushToStack() {
    auto holder = static_cast<BridgeHolder *>(lua_newuserdata(m_L, sizeof(BridgeHolder)));
    new (holder) BridgeHolder{this, m_alive};

    lua_createtable(m_L, 0, 5);
    lua_pushcfunction(m_L, meta_index);
    lua_setfield(m_L, -2, "__index");

    lua_pushcfunction(m_L, meta_newindex);
    lua_setfield(m_L, -2, "__newindex");

    lua_pushcfunction(m_L, meta_len);
    lua_setfield(m_L, -2, "__len");

    lua_pushcfunction(m_L, meta_ipairs);
    lua_setfield(m_L, -2, "__ipairs");

    lua_pushcfunction(m_L, meta_gc);
    lua_setfield(m_L, -2, "__gc");

    lua_setmetatable(m_L, -2);
}

// -------------------------------------------------------------
// Convert C++ AssEntryData -> Lua Table
// -------------------------------------------------------------
void LuaAssFileBridge::assEntryToLua(lua_State *L, size_t idx) {
    if (idx >= m_lines.size()) {
        lua_pushnil(L);
        return;
    }
    const auto &e = m_lines[idx];
    lua_newtable(L);

    lua_pushstring(L, e.section.toUtf8().constData());
    lua_setfield(L, -2, "section");

    if (e.entryClass == AssEntryClass::Info) {
        lua_pushstring(L, "info");
        lua_setfield(L, -2, "class");

        QString raw = QString("%1: %2").arg(e.key, e.value);
        lua_pushstring(L, raw.toUtf8().constData());
        lua_setfield(L, -2, "raw");

        lua_pushstring(L, e.key.toUtf8().constData());
        lua_setfield(L, -2, "key");

        lua_pushstring(L, e.value.toUtf8().constData());
        lua_setfield(L, -2, "value");
    } else if (e.entryClass == AssEntryClass::Style) {
        lua_pushstring(L, "style");
        lua_setfield(L, -2, "class");

        lua_pushstring(L, e.styleName.toUtf8().constData());
        lua_setfield(L, -2, "name");

        lua_pushstring(L, e.fontName.toUtf8().constData());
        lua_setfield(L, -2, "fontname");

        lua_pushnumber(L, e.fontSize);
        lua_setfield(L, -2, "fontsize");

        lua_pushstring(L, e.color1.toUtf8().constData());
        lua_setfield(L, -2, "color1");

        lua_pushstring(L, e.color2.toUtf8().constData());
        lua_setfield(L, -2, "color2");

        lua_pushstring(L, e.color3.toUtf8().constData());
        lua_setfield(L, -2, "color3");

        lua_pushstring(L, e.color4.toUtf8().constData());
        lua_setfield(L, -2, "color4");

        lua_pushboolean(L, e.bold);
        lua_setfield(L, -2, "bold");

        lua_pushboolean(L, e.italic);
        lua_setfield(L, -2, "italic");

        lua_pushboolean(L, e.underline);
        lua_setfield(L, -2, "underline");

        lua_pushboolean(L, e.strikeout);
        lua_setfield(L, -2, "strikeout");

        lua_pushnumber(L, e.scaleX);
        lua_setfield(L, -2, "scale_x");

        lua_pushnumber(L, e.scaleY);
        lua_setfield(L, -2, "scale_y");

        lua_pushnumber(L, e.spacing);
        lua_setfield(L, -2, "spacing");

        lua_pushnumber(L, e.angle);
        lua_setfield(L, -2, "angle");

        lua_pushinteger(L, e.borderStyle);
        lua_setfield(L, -2, "borderstyle");

        lua_pushnumber(L, e.outline);
        lua_setfield(L, -2, "outline");

        lua_pushnumber(L, e.shadow);
        lua_setfield(L, -2, "shadow");

        lua_pushinteger(L, e.align);
        lua_setfield(L, -2, "align");

        lua_pushinteger(L, e.marginL);
        lua_setfield(L, -2, "margin_l");

        lua_pushinteger(L, e.marginR);
        lua_setfield(L, -2, "margin_r");

        lua_pushinteger(L, e.marginV);
        lua_setfield(L, -2, "margin_t");

        lua_pushinteger(L, e.marginV);
        lua_setfield(L, -2, "margin_b");

        lua_pushinteger(L, e.encoding);
        lua_setfield(L, -2, "encoding");

        lua_pushinteger(L, 2);
        lua_setfield(L, -2, "relative_to");
    } else {
        lua_pushstring(L, "dialogue");
        lua_setfield(L, -2, "class");

        lua_pushboolean(L, e.comment);
        lua_setfield(L, -2, "comment");

        lua_pushinteger(L, e.layer);
        lua_setfield(L, -2, "layer");

        lua_pushinteger(L, e.startTime);
        lua_setfield(L, -2, "start_time");

        lua_pushinteger(L, e.endTime);
        lua_setfield(L, -2, "end_time");

        lua_pushstring(L, e.style.toUtf8().constData());
        lua_setfield(L, -2, "style");

        lua_pushstring(L, e.actor.toUtf8().constData());
        lua_setfield(L, -2, "actor");

        lua_pushinteger(L, e.margin_l);
        lua_setfield(L, -2, "margin_l");

        lua_pushinteger(L, e.margin_r);
        lua_setfield(L, -2, "margin_r");

        lua_pushinteger(L, e.margin_t);
        lua_setfield(L, -2, "margin_t");

        lua_pushinteger(L, e.margin_b);
        lua_setfield(L, -2, "margin_b");

        lua_pushstring(L, e.effect.toUtf8().constData());
        lua_setfield(L, -2, "effect");

        lua_pushstring(L, e.text.toUtf8().constData());
        lua_setfield(L, -2, "text");

        // extradata table
        lua_newtable(L);
        for (auto it = e.extra.begin(); it != e.extra.end(); ++it) {
            lua_pushstring(L, it.key().toUtf8().constData());
            lua_pushstring(L, it.value().toUtf8().constData());
            lua_settable(L, -3);
        }
        lua_setfield(L, -2, "extra");
    }
}

// -------------------------------------------------------------
// Convert Lua Table -> C++ AssEntryData
// -------------------------------------------------------------
AssEntryData LuaAssFileBridge::luaToAssEntry(lua_State *L, int idx) {
    AssEntryData res;
    if (!lua_istable(L, idx)) return res;

    auto getString = [&](const char *name, const QString &def = "") -> QString {
        lua_getfield(L, idx, name);
        QString val = lua_isstring(L, -1) ? QString::fromUtf8(lua_tostring(L, -1)) : def;
        lua_pop(L, 1);
        return val;
    };
    auto getDouble = [&](const char *name, double def = 0.0) -> double {
        lua_getfield(L, idx, name);
        double val = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : def;
        lua_pop(L, 1);
        return val;
    };
    auto getInt = [&](const char *name, int def = 0) -> int {
        lua_getfield(L, idx, name);
        int val = lua_isnumber(L, -1) ? (int)lua_tointeger(L, -1) : def;
        lua_pop(L, 1);
        return val;
    };
    auto getBool = [&](const char *name, bool def = false) -> bool {
        lua_getfield(L, idx, name);
        bool val = lua_isboolean(L, -1) ? !!lua_toboolean(L, -1) : def;
        lua_pop(L, 1);
        return val;
    };

    QString cls = getString("class", "dialogue").toLower();
    res.section = getString("section", "");

    if (cls == "info") {
        res.entryClass = AssEntryClass::Info;
        if (res.section.isEmpty()) res.section = "[Script Info]";
        res.key = getString("key");
        res.value = getString("value");
    } else if (cls == "style") {
        res.entryClass = AssEntryClass::Style;
        if (res.section.isEmpty()) res.section = "[V4+ Styles]";
        res.styleName = getString("name", "Default");
        res.fontName = getString("fontname", "Arial");
        res.fontSize = getDouble("fontsize", 20.0);
        res.color1 = getString("color1", "&H00FFFFFF&");
        res.color2 = getString("color2", "&H000000FF&");
        res.color3 = getString("color3", "&H00000000&");
        res.color4 = getString("color4", "&H00000000&");
        res.bold = getBool("bold");
        res.italic = getBool("italic");
        res.underline = getBool("underline");
        res.strikeout = getBool("strikeout");
        res.scaleX = getDouble("scale_x", 100.0);
        res.scaleY = getDouble("scale_y", 100.0);
        res.spacing = getDouble("spacing", 0.0);
        res.angle = getDouble("angle", 0.0);
        res.borderStyle = getInt("borderstyle", 1);
        res.outline = getDouble("outline", 2.0);
        res.shadow = getDouble("shadow", 2.0);
        res.align = getInt("align", 2);
        res.marginL = getInt("margin_l", 10);
        res.marginR = getInt("margin_r", 10);
        res.marginV = getInt("margin_t", 10);
        res.encoding = getInt("encoding", 1);
    } else {
        res.entryClass = AssEntryClass::Dialogue;
        if (res.section.isEmpty()) res.section = "[Events]";
        res.comment = getBool("comment");
        res.layer = getInt("layer");
        res.startTime = getInt("start_time");
        res.endTime = getInt("end_time");
        res.style = getString("style", "Default");
        res.actor = getString("actor");
        res.margin_l = getInt("margin_l");
        res.margin_r = getInt("margin_r");
        res.margin_t = getInt("margin_t");
        res.margin_b = getInt("margin_b", res.margin_t);
        res.effect = getString("effect");
        res.text = getString("text");

        lua_getfield(L, idx, "extra");
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                if (lua_isstring(L, -2) && lua_isstring(L, -1)) {
                    QString k = QString::fromUtf8(lua_tostring(L, -2));
                    QString v = QString::fromUtf8(lua_tostring(L, -1));
                    res.extra[k] = v;
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }

    return res;
}

// -------------------------------------------------------------
// Lua Metatable Index / NewIndex
// -------------------------------------------------------------
int LuaAssFileBridge::indexRead(lua_State *L) {
    if (lua_isnumber(L, 2)) {
        int idx = (int)lua_tointeger(L, 2);
        if (idx < 1 || idx > (int)m_lines.size()) {
            return luaL_error(L, "Requested out-of-range line from subtitle file: %d", idx);
        }
        assEntryToLua(L, idx - 1);
        return 1;
    }

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        if (strcmp(key, "n") == 0) {
            lua_pushinteger(L, m_lines.size());
            return 1;
        }

        lua_pushvalue(L, 1); // push userdata as upvalue
        if (strcmp(key, "delete") == 0) {
            lua_pushcclosure(L, closure_delete, 1);
            return 1;
        } else if (strcmp(key, "deleterange") == 0) {
            lua_pushcclosure(L, closure_deleterange, 1);
            return 1;
        } else if (strcmp(key, "insert") == 0) {
            lua_pushcclosure(L, closure_insert, 1);
            return 1;
        } else if (strcmp(key, "append") == 0) {
            lua_pushcclosure(L, closure_append, 1);
            return 1;
        } else if (strcmp(key, "script_resolution") == 0) {
            lua_pushcclosure(L, closure_script_resolution, 1);
            return 1;
        } else {
            lua_pop(L, 1);
            return luaL_error(L, "Invalid indexing in Subtitle File object: '%s'", key);
        }
    }

    return luaL_error(L, "Attempt to index a Subtitle File object with value of type '%s'", lua_typename(L, lua_type(L, 2)));
}

int LuaAssFileBridge::indexWrite(lua_State *L) {
    m_modified = true;
    int n = (int)lua_tointeger(L, 2);

    if (n < 0) {
        // insert line at -n
        if (n == std::numeric_limits<int>::min()) n = -std::numeric_limits<int>::max();
        int insertIdx = -n - 1;
        if (insertIdx < 0) insertIdx = 0;
        if (insertIdx > (int)m_lines.size()) insertIdx = (int)m_lines.size();
        m_lines.insert(m_lines.begin() + insertIdx, luaToAssEntry(L, 3));
    } else if (n == 0) {
        // append
        m_lines.push_back(luaToAssEntry(L, 3));
    } else {
        if (!lua_isnil(L, 3)) {
            // replace
            if (n < 1 || n > (int)m_lines.size()) {
                return luaL_error(L, "Out-of-range line index in subtitles replacement: %d", n);
            }
            m_lines[n - 1] = luaToAssEntry(L, 3);
        } else {
            // delete
            if (n >= 1 && n <= (int)m_lines.size()) {
                m_lines.erase(m_lines.begin() + (n - 1));
            }
        }
    }
    return 0;
}

int LuaAssFileBridge::getLen(lua_State *L) {
    lua_pushinteger(L, m_lines.size());
    return 1;
}

int LuaAssFileBridge::ipairs(lua_State *L) {
    lua_pushvalue(L, 1); // push userdata as upvalue
    lua_pushcclosure(L, closure_iternext, 1);
    lua_pushnil(L);
    lua_pushinteger(L, 0);
    return 3;
}

int LuaAssFileBridge::iterNext(lua_State *L) {
    size_t i = (size_t)lua_tointeger(L, 2);
    if (i >= m_lines.size()) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, i + 1);
    assEntryToLua(L, i);
    return 2;
}

// -------------------------------------------------------------
// Subtitle Object Operations
// -------------------------------------------------------------
int LuaAssFileBridge::append(lua_State *L) {
    m_modified = true;
    int count = lua_gettop(L);
    for (int i = 1; i <= count; ++i) {
        auto e = luaToAssEntry(L, i);
        // Find suitable insertion place matching section or append to end
        auto grp = e.entryClass;
        size_t place = m_lines.size();
        for (size_t k = m_lines.size(); k > 0; --k) {
            if (m_lines[k - 1].entryClass == grp) {
                place = k;
                break;
            }
        }
        m_lines.insert(m_lines.begin() + place, std::move(e));
    }
    return 0;
}

int LuaAssFileBridge::insert(lua_State *L) {
    m_modified = true;
    int before = (int)lua_tointeger(L, 1);
    if (before < 1 || before > (int)m_lines.size() + 1) {
        return luaL_error(L, "Out of range line index in subtitles.insert: %d", before);
    }
    int count = lua_gettop(L);
    size_t pos = before - 1;
    for (int i = 2; i <= count; ++i) {
        m_lines.insert(m_lines.begin() + pos, luaToAssEntry(L, i));
        pos++;
    }
    return 0;
}

int LuaAssFileBridge::del(lua_State *L) {
    m_modified = true;
    int top = lua_gettop(L);
    if (top == 0) return 0;

    std::vector<int> to_del;
    if (top == 1 && lua_istable(L, 1)) {
        int n = (int)lua_objlen(L, 1);
        for (int i = 1; i <= n; ++i) {
            lua_rawgeti(L, 1, i);
            to_del.push_back((int)lua_tointeger(L, -1) - 1);
            lua_pop(L, 1);
        }
    } else {
        for (int i = 1; i <= top; ++i) {
            to_del.push_back((int)lua_tointeger(L, i) - 1);
        }
    }

    std::sort(to_del.begin(), to_del.end(), std::greater<int>());
    to_del.erase(std::unique(to_del.begin(), to_del.end()), to_del.end());

    for (int idx : to_del) {
        if (idx >= 0 && idx < (int)m_lines.size()) {
            m_lines.erase(m_lines.begin() + idx);
        }
    }
    return 0;
}

int LuaAssFileBridge::deleteRange(lua_State *L) {
    m_modified = true;
    int a = std::max<int>(1, (int)lua_tointeger(L, 1)) - 1;
    int b = std::min<int>((int)m_lines.size(), (int)lua_tointeger(L, 2));
    if (a >= b) return 0;

    m_lines.erase(m_lines.begin() + a, m_lines.begin() + b);
    return 0;
}

int LuaAssFileBridge::getScriptResolution(lua_State *L) {
    lua_pushinteger(L, m_resX);
    lua_pushinteger(L, m_resY);
    return 2;
}

// -------------------------------------------------------------
// Global aegisub Table Callbacks
// -------------------------------------------------------------
int LuaAssFileBridge::setUndoPoint(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "active_lua_assfile_bridge");
    auto bridge = static_cast<LuaAssFileBridge *>(lua_touserdata(L, -1));
    lua_pop(L, 1);

    if (bridge && lua_isstring(L, 1)) {
        bridge->m_undoDescription = QString::fromUtf8(lua_tostring(L, 1));
        bridge->m_modified = true;
    }
    return 0;
}

int LuaAssFileBridge::textExtents(lua_State *L) {
    if (!lua_istable(L, 1) || !lua_isstring(L, 2)) {
        return luaL_error(L, "Invalid arguments to aegisub.text_extents");
    }

    auto getString = [&](const char *name, const QString &def = "") -> QString {
        lua_getfield(L, 1, name);
        QString val = lua_isstring(L, -1) ? QString::fromUtf8(lua_tostring(L, -1)) : def;
        lua_pop(L, 1);
        return val;
    };
    auto getDouble = [&](const char *name, double def = 0.0) -> double {
        lua_getfield(L, 1, name);
        double val = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : def;
        lua_pop(L, 1);
        return val;
    };
    auto getBool = [&](const char *name, bool def = false) -> bool {
        lua_getfield(L, 1, name);
        bool val = lua_isboolean(L, -1) ? !!lua_toboolean(L, -1) : def;
        lua_pop(L, 1);
        return val;
    };
    auto getInt = [&](const char *name, int def = 0) -> int {
        lua_getfield(L, 1, name);
        int val = lua_isnumber(L, -1) ? (int)lua_tointeger(L, -1) : def;
        lua_pop(L, 1);
        return val;
    };

    AssStyleExtents style;
    style.font = getString("fontname", "Arial");
    style.fontsize = getDouble("fontsize", 20.0);
    style.bold = getBool("bold");
    style.italic = getBool("italic");
    style.underline = getBool("underline");
    style.strikeout = getBool("strikeout");
    style.scalex = getDouble("scale_x", 100.0);
    style.scaley = getDouble("scale_y", 100.0);
    style.spacing = getDouble("spacing", 0.0);
    style.encoding = getInt("encoding", 1);

    QString text = QString::fromUtf8(lua_tostring(L, 2));

    double width = 0, height = 0, descent = 0, extlead = 0;
    if (!CalculateTextExtents(style, text, width, height, descent, extlead)) {
        return luaL_error(L, "Failed to calculate text extents");
    }

    lua_pushnumber(L, width);
    lua_pushnumber(L, height);
    lua_pushnumber(L, descent);
    lua_pushnumber(L, extlead);
    return 4;
}

// -------------------------------------------------------------
// parse_karaoke_data
// -------------------------------------------------------------
int LuaAssFileBridge::parseKaraokeData(lua_State *L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "Subtitle line must be a dialogue line table");
    }

    auto e = luaToAssEntry(L, 1);
    if (e.entryClass != AssEntryClass::Dialogue) {
        return luaL_error(L, "Subtitle line must be a dialogue line");
    }

    auto sylList = AegisubCoreBridge::parseKaraokeLine(e.text, e.startTime, e.endTime, false);

    // Build Lua result table: index 0 (empty compatibility entry) + 1..N
    lua_newtable(L);

    // [0]
    lua_createtable(L, 0, 6);
    lua_pushinteger(L, 0); lua_setfield(L, -2, "duration");
    lua_pushinteger(L, 0); lua_setfield(L, -2, "start_time");
    lua_pushinteger(L, 0); lua_setfield(L, -2, "end_time");
    lua_pushstring(L, "");  lua_setfield(L, -2, "tag");
    lua_pushstring(L, "");  lua_setfield(L, -2, "text");
    lua_pushstring(L, "");  lua_setfield(L, -2, "text_stripped");
    lua_rawseti(L, -2, 0);

    // [1..N]
    for (int i = 0; i < sylList.size(); ++i) {
        const auto &s = sylList[i];
        lua_createtable(L, 0, 6);
        lua_pushinteger(L, s["duration"].toInt());
        lua_setfield(L, -2, "duration");

        lua_pushinteger(L, s["startTime"].toInt() - e.startTime);
        lua_setfield(L, -2, "start_time");

        lua_pushinteger(L, s["endTime"].toInt() - e.startTime);
        lua_setfield(L, -2, "end_time");

        lua_pushstring(L, s["tagType"].toString().toUtf8().constData());
        lua_setfield(L, -2, "tag");

        lua_pushstring(L, s["textWithTags"].toString().toUtf8().constData());
        lua_setfield(L, -2, "text");

        lua_pushstring(L, s["text"].toString().toUtf8().constData());
        lua_setfield(L, -2, "text_stripped");

        lua_rawseti(L, -2, i + 1);
    }

    return 1;
}

} // namespace Automation
