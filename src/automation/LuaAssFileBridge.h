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
#include <QList>
#include <QMap>
#include <vector>
#include <memory>

namespace Automation {

enum class AssEntryClass {
    Info,
    Style,
    Dialogue
};

struct AssEntryData {
    AssEntryClass entryClass = AssEntryClass::Dialogue;
    QString section; // e.g. "[Script Info]", "[V4+ Styles]", "[Events]"

    // Info fields
    QString key;
    QString value;

    // Style fields
    QString styleName = "Default";
    QString fontName = "Arial";
    double fontSize = 20.0;
    QString color1 = "&H00FFFFFF&";
    QString color2 = "&H000000FF&";
    QString color3 = "&H00000000&";
    QString color4 = "&H00000000&";
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikeout = false;
    double scaleX = 100.0;
    double scaleY = 100.0;
    double spacing = 0.0;
    double angle = 0.0;
    int borderStyle = 1;
    double outline = 2.0;
    double shadow = 2.0;
    int align = 2;
    int marginL = 10;
    int marginR = 10;
    int marginV = 10;
    int encoding = 1;

    // Dialogue fields
    bool comment = false;
    int layer = 0;
    int startTime = 0; // ms
    int endTime = 0;   // ms
    QString style = "Default";
    QString actor;
    int margin_l = 0;
    int margin_r = 0;
    int margin_t = 0;
    int margin_b = 0;
    QString effect;
    QString text;
    QMap<QString, QString> extra;
};

class LuaAssFileBridge {
public:
    LuaAssFileBridge(lua_State *L, std::vector<AssEntryData> initialLines, int resX = 1920, int resY = 1080);
    ~LuaAssFileBridge();

    void setLuaState(lua_State *L);

    // Push the userdata 'subtitles' object onto Lua stack
    void pushToStack();

    std::vector<AssEntryData> getLines() const { return m_lines; }
    QString undoDescription() const { return m_undoDescription; }
    bool isModified() const { return m_modified; }

    static LuaAssFileBridge *getBridge(lua_State *L, int idx);

    // Callbacks for Lua metatable
    int indexRead(lua_State *L);
    int indexWrite(lua_State *L);
    int getLen(lua_State *L);
    int ipairs(lua_State *L);
    int iterNext(lua_State *L);

    // Subtitle object methods
    int append(lua_State *L);
    int insert(lua_State *L);
    int del(lua_State *L);
    int deleteRange(lua_State *L);
    int getScriptResolution(lua_State *L);

    // Global aegisub table callbacks registered while active
    static int parseKaraokeData(lua_State *L);
    static int setUndoPoint(lua_State *L);
    static int textExtents(lua_State *L);

private:
    void assEntryToLua(lua_State *L, size_t idx);
    static AssEntryData luaToAssEntry(lua_State *L, int idx);

    lua_State *m_L;
    std::vector<AssEntryData> m_lines;
    int m_resX = 1920;
    int m_resY = 1080;
    QString m_undoDescription;
    bool m_modified = false;
    std::shared_ptr<bool> m_alive;
};

} // namespace Automation
