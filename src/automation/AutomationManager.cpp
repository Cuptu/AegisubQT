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

#include "AutomationManager.h"
#include "LuaScript.h"
#include "SubtitleModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QMetaObject>

namespace Automation {

static AutomationManager *s_instance = nullptr;

AutomationManager *AutomationManager::instance() {
    if (!s_instance) {
        s_instance = new AutomationManager();
    }
    return s_instance;
}

AutomationManager::AutomationManager(QObject *parent)
    : QObject(parent)
{
    s_instance = this;

    // Detect include paths
    QString appDir = QCoreApplication::applicationDirPath();
    QString currentDir = QDir::currentPath();

    QStringList candidateInclude = {
        currentDir + "/automation/include",
        appDir + "/automation/include",
        appDir + "/../automation/include"
    };

    for (const QString &p : candidateInclude) {
        if (QDir(p).exists()) {
            addIncludePath(QDir::cleanPath(p));
        }
    }

    scanAutoloadFolder();
}

AutomationManager::~AutomationManager() {
    if (s_instance == this) s_instance = nullptr;
}

void AutomationManager::addIncludePath(const QString &path) {
    if (!m_includePaths.contains(path)) {
        m_includePaths.append(path);
    }
}

QVariantList AutomationManager::scripts() const {
    return m_cachedScripts;
}

QVariantList AutomationManager::macros() const {
    return m_cachedMacros;
}

void AutomationManager::updateMacroList() {
    m_cachedScripts.clear();
    m_cachedMacros.clear();

    for (size_t i = 0; i < m_entries.size(); ++i) {
        const auto &entry = m_entries[i];
        const auto &s = entry.script;

        QVariantMap sMap;
        sMap["index"] = (int)i;
        sMap["name"] = s->name();
        sMap["filename"] = QFileInfo(s->filepath()).fileName();
        sMap["filepath"] = s->filepath();
        sMap["description"] = s->description();
        sMap["author"] = s->author();
        sMap["version"] = s->version();
        sMap["isGlobal"] = entry.isGlobal;
        sMap["loaded"] = s->isLoaded();
        sMap["error"] = s->errorString();
        m_cachedScripts.append(sMap);

        if (s->isLoaded()) {
            for (const auto &m : s->macros()) {
                QVariantMap mMap;
                mMap["id"] = m.id;
                mMap["name"] = m.name;
                mMap["description"] = m.description;
                mMap["scriptName"] = s->name();
                mMap["scriptPath"] = s->filepath();
                m_cachedMacros.append(mMap);
            }
        }
    }

    emit scriptsChanged();
    emit macrosChanged();
}

void AutomationManager::scanAutoloadFolder() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString currentDir = QDir::currentPath();

    QStringList candidateAutoload = {
        currentDir + "/automation/autoload",
        appDir + "/automation/autoload",
        appDir + "/../automation/autoload"
    };

    QString autoloadDir;
    for (const QString &p : candidateAutoload) {
        if (QDir(p).exists()) {
            autoloadDir = QDir::cleanPath(p);
            break;
        }
    }

    if (autoloadDir.isEmpty()) {
        qWarning() << "[AutomationManager] No autoload folder found";
        return;
    }

    qInfo() << "[AutomationManager] Scanning autoload folder:" << autoloadDir;
    QDir dir(autoloadDir);
    QStringList files = dir.entryList({"*.lua", "*.moon"}, QDir::Files, QDir::Name);

    for (const QString &f : files) {
        QString fullPath = dir.filePath(f);
        // Avoid duplicate
        bool exists = false;
        for (const auto &e : m_entries) {
            if (e.script->filepath() == fullPath) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            auto s = std::make_unique<LuaScript>(fullPath, m_includePaths);
            s->load();
            m_entries.push_back({std::move(s), true});
        }
    }

    updateMacroList();
}

bool AutomationManager::addScript(const QString &filepath, bool isGlobal) {
    if (!QFileInfo::exists(filepath)) return false;

    auto s = std::make_unique<LuaScript>(filepath, m_includePaths);
    bool ok = s->load();
    m_entries.push_back({std::move(s), isGlobal});
    updateMacroList();
    return ok;
}

void AutomationManager::removeScript(int index) {
    if (index >= 0 && index < (int)m_entries.size()) {
        m_entries.erase(m_entries.begin() + index);
        updateMacroList();
    }
}

void AutomationManager::reloadScript(int index) {
    if (index >= 0 && index < (int)m_entries.size()) {
        m_entries[index].script->reload();
        updateMacroList();
    }
}

void AutomationManager::reloadAll() {
    for (auto &e : m_entries) {
        e.script->reload();
    }
    updateMacroList();
}

static int assTimeToMs(const QString &timeStr) {
    QStringList parts = timeStr.split(':');
    if (parts.size() < 3) return 0;
    int h = parts[0].toInt();
    int m = parts[1].toInt();
    QStringList secParts = parts[2].split('.');
    int s = secParts[0].toInt();
    int cs = (secParts.size() > 1) ? secParts[1].toInt() : 0;
    if (secParts.size() > 1 && secParts[1].length() == 1) cs *= 10;
    return h * 3600000 + m * 60000 + s * 1000 + cs * 10;
}

static QString msToAssTime(int ms) {
    if (ms < 0) ms = 0;
    int cs = (ms % 1000) / 10;
    int totalSec = ms / 1000;
    int s = totalSec % 60;
    int totalMin = totalSec / 60;
    int m = totalMin % 60;
    int h = totalMin / 60;
    return QString::asprintf("%d:%02d:%02d.%02d", h, m, s, cs);
}

bool AutomationManager::runMacro(int macroId, QObject *subtitleProject, int activeIndex, const QVariantList &selectedIndices) {
    // Locate the script containing this macro
    LuaScript *targetScript = nullptr;
    QString macroName;
    for (const auto &entry : m_entries) {
        for (const auto &m : entry.script->macros()) {
            if (m.id == macroId) {
                targetScript = entry.script.get();
                macroName = m.name;
                break;
            }
        }
        if (targetScript) break;
    }

    if (!targetScript) {
        qWarning() << "[AutomationManager] Macro id" << macroId << "not found";
        return false;
    }

    qInfo() << "[AutomationManager] Running macro:" << macroName << "(id:" << macroId << ")";

    // Resolve native SubtitleModel: direct pointer, QML wrapper property, or registered instance
    SubtitleModel *model = qobject_cast<SubtitleModel *>(subtitleProject);
    if (!model && subtitleProject) {
        QVariant subProp = subtitleProject->property("subtitleModel");
        if (subProp.isValid()) {
            QObject *subObj = subProp.value<QObject *>();
            if (subObj) model = qobject_cast<SubtitleModel *>(subObj);
        }
    }
    if (!model) {
        model = m_subtitleModel;
    }

    // Prepare ASS lines
    std::vector<AssEntryData> lines;
    int resX = 1920;
    int resY = 1080;

    if (model) {
        // 1. Script Info from native model
        QVariantMap info = model->scriptInfo();
        if (info.isEmpty()) info = SubtitleModel::defaultScriptInfo();
        if (!info.contains(QStringLiteral("ScriptType"))) {
            info[QStringLiteral("ScriptType")] = QStringLiteral("v4.00+");
        }
        resX = info.value(QStringLiteral("PlayResX"), 1920).toInt();
        resY = info.value(QStringLiteral("PlayResY"), 1080).toInt();
        if (resX <= 0) resX = 1920;
        if (resY <= 0) resY = 1080;

        for (auto it = info.cbegin(); it != info.cend(); ++it) {
            lines.push_back({AssEntryClass::Info, QStringLiteral("[Script Info]"), it.key(), it.value().toString()});
        }

        // 2. Styles from native model
        QVariantList stylesList = model->styles();
        if (stylesList.isEmpty()) stylesList = SubtitleModel::defaultStyles();

        for (const auto &v : stylesList) {
            QVariantMap st = v.toMap();
            AssEntryData sd;
            sd.entryClass = AssEntryClass::Style;
            sd.section = QStringLiteral("[V4+ Styles]");
            sd.styleName = st.value(QStringLiteral("name"), QStringLiteral("Default")).toString();
            sd.fontName = st.value(QStringLiteral("font"), QStringLiteral("Arial")).toString();
            sd.fontSize = st.value(QStringLiteral("size"), 20.0).toDouble();
            sd.color1 = st.value(QStringLiteral("primary"), QStringLiteral("&H00FFFFFF")).toString();
            sd.color2 = st.value(QStringLiteral("secondary"), QStringLiteral("&H000000FF")).toString();
            sd.color3 = st.value(QStringLiteral("outline"), QStringLiteral("&H00000000")).toString();
            sd.color4 = st.value(QStringLiteral("shadow"), QStringLiteral("&H00000000")).toString();
            sd.bold = st.value(QStringLiteral("bold"), false).toBool();
            sd.italic = st.value(QStringLiteral("italic"), false).toBool();
            sd.underline = st.value(QStringLiteral("underline"), false).toBool();
            sd.strikeout = st.value(QStringLiteral("strikeout"), false).toBool();
            sd.scaleX = st.value(QStringLiteral("scaleX"), 100.0).toDouble();
            sd.scaleY = st.value(QStringLiteral("scaleY"), 100.0).toDouble();
            sd.spacing = st.value(QStringLiteral("spacing"), 0.0).toDouble();
            sd.angle = st.value(QStringLiteral("angle"), 0.0).toDouble();
            sd.borderStyle = st.value(QStringLiteral("borderStyle"), 1).toInt();
            sd.outline = st.value(QStringLiteral("outlineWidth"), 2.0).toDouble();
            sd.shadow = st.value(QStringLiteral("shadowDepth"), 2.0).toDouble();
            sd.align = st.value(QStringLiteral("alignment"), 2).toInt();
            sd.marginL = st.value(QStringLiteral("marginL"), 10).toInt();
            sd.marginR = st.value(QStringLiteral("marginR"), 10).toInt();
            sd.marginV = st.value(QStringLiteral("marginV"), 10).toInt();
            sd.encoding = st.value(QStringLiteral("encoding"), 1).toInt();
            lines.push_back(std::move(sd));
        }

        // 3. Dialogue lines directly from native C++ memory
        for (const auto &item : model->rawLines()) {
            AssEntryData dia;
            dia.entryClass = AssEntryClass::Dialogue;
            dia.section = QStringLiteral("[Events]");
            dia.layer = item.layer;
            dia.startTime = item.startMs;
            dia.endTime = item.endMs;
            dia.style = item.style;
            dia.actor = item.actor;
            dia.effect = item.effect;
            dia.margin_l = item.marginLeft;
            dia.margin_r = item.marginRight;
            dia.margin_t = item.marginVert;
            dia.margin_b = item.marginVert;
            dia.text = item.text;
            dia.comment = item.isComment;
            lines.push_back(std::move(dia));
        }
    } else {
        // Fallback for legacy standalone or detached subtitleProject
        lines.push_back({AssEntryClass::Info, "[Script Info]", "ScriptType", "v4.00+"});
        lines.push_back({AssEntryClass::Info, "[Script Info]", "PlayResX", "1920"});
        lines.push_back({AssEntryClass::Info, "[Script Info]", "PlayResY", "1080"});
        lines.push_back({AssEntryClass::Info, "[Script Info]", "WrapStyle", "0"});
        lines.push_back({AssEntryClass::Info, "[Script Info]", "ScaledBorderAndShadow", "yes"});

        AssEntryData defStyle;
        defStyle.entryClass = AssEntryClass::Style;
        defStyle.section = "[V4+ Styles]";
        defStyle.styleName = "Default";
        defStyle.fontName = "Arial";
        defStyle.fontSize = 48.0;
        lines.push_back(defStyle);

        if (subtitleProject) {
            QVariant linesVar;
            if (QMetaObject::invokeMethod(subtitleProject, "getAllSubtitleLines", Q_RETURN_ARG(QVariant, linesVar))) {
                QVariantList list = linesVar.toList();
                for (const QVariant &item : list) {
                    QVariantMap map = item.toMap();
                    AssEntryData dia;
                    dia.entryClass = AssEntryClass::Dialogue;
                    dia.section = "[Events]";
                    dia.layer = map.value("layer", 0).toInt();
                    dia.startTime = assTimeToMs(map.value("start", "0:00:00.00").toString());
                    dia.endTime = assTimeToMs(map.value("end", "0:00:05.00").toString());
                    dia.style = map.value("style", "Default").toString();
                    dia.actor = map.value("actor", "").toString();
                    dia.effect = map.value("effect", "").toString();
                    dia.margin_l = map.value("marginLeft", 0).toInt();
                    dia.margin_r = map.value("marginRight", 0).toInt();
                    dia.margin_t = map.value("marginVert", 0).toInt();
                    dia.margin_b = dia.margin_t;
                    dia.text = map.value("text", "").toString();
                    dia.comment = map.value("comment", false).toBool();
                    lines.push_back(std::move(dia));
                }
            }
        }
    }

    int headerOffset = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].entryClass != AssEntryClass::Dialogue) {
            headerOffset = static_cast<int>(i + 1);
        }
    }

    // Map selected indices to 1-based Lua indices
    std::vector<int> luaSelected;
    if (selectedIndices.isEmpty()) {
        luaSelected.push_back(activeIndex + headerOffset + 1);
    } else {
        for (const QVariant &v : selectedIndices) {
            luaSelected.push_back(v.toInt() + headerOffset + 1);
        }
    }
    int luaActive = activeIndex + headerOffset + 1;

    // Create bridge & run macro
    LuaAssFileBridge bridge(nullptr, std::move(lines), resX, resY);
    QString err;
    bool ok = targetScript->runMacro(macroId, bridge, luaSelected, luaActive, err);

    if (!ok) {
        qWarning() << "[AutomationManager] Macro execution error:" << err;
        emit macroExecuted(macroName, false, err);
        emit statusMessage(QString("Macro execution failed: %1").arg(err));
        return false;
    }

    // Apply back modified lines to model or project
    if (bridge.isModified()) {
        std::vector<AssEntryData> updated = bridge.getLines();
        QString undoMsg = bridge.undoDescription();
        if (undoMsg.isEmpty()) {
            undoMsg = QString("Macro: %1").arg(macroName);
        }

        if (model) {
            // Push atomic undo point on native model
            model->pushUndo(undoMsg, activeIndex, selectedIndices);

            // 1. Dialogue lines
            std::vector<SubtitleLine> updatedLines;
            updatedLines.reserve(updated.size());
            int lineNum = 1;
            for (const auto &item : updated) {
                if (item.entryClass != AssEntryClass::Dialogue) continue;
                SubtitleLine dl;
                dl.lineNumber = lineNum++;
                dl.layer = item.layer;
                dl.setStartMs(item.startTime);
                dl.setEndMs(item.endTime);
                dl.style = item.style;
                dl.actor = item.actor;
                dl.effect = item.effect;
                dl.marginLeft = item.margin_l;
                dl.marginRight = item.margin_r;
                dl.marginVert = item.margin_t;
                dl.text = item.text;
                dl.isComment = item.comment;
                dl.updateCps();
                updatedLines.push_back(std::move(dl));
            }
            model->setRawLines(std::move(updatedLines));

            // 2. Styles (sync if modified or added by macro)
            QVariantList updatedStyles;
            for (const auto &item : updated) {
                if (item.entryClass != AssEntryClass::Style) continue;
                QVariantMap st;
                st[QStringLiteral("name")] = item.styleName;
                st[QStringLiteral("font")] = item.fontName;
                st[QStringLiteral("size")] = item.fontSize;
                st[QStringLiteral("primary")] = item.color1;
                st[QStringLiteral("secondary")] = item.color2;
                st[QStringLiteral("outline")] = item.color3;
                st[QStringLiteral("shadow")] = item.color4;
                st[QStringLiteral("bold")] = item.bold;
                st[QStringLiteral("italic")] = item.italic;
                st[QStringLiteral("underline")] = item.underline;
                st[QStringLiteral("strikeout")] = item.strikeout;
                st[QStringLiteral("scaleX")] = item.scaleX;
                st[QStringLiteral("scaleY")] = item.scaleY;
                st[QStringLiteral("spacing")] = item.spacing;
                st[QStringLiteral("angle")] = item.angle;
                st[QStringLiteral("borderStyle")] = item.borderStyle;
                st[QStringLiteral("outlineWidth")] = item.outline;
                st[QStringLiteral("shadowDepth")] = item.shadow;
                st[QStringLiteral("alignment")] = item.align;
                st[QStringLiteral("marginL")] = item.marginL;
                st[QStringLiteral("marginR")] = item.marginR;
                st[QStringLiteral("marginV")] = item.marginV;
                st[QStringLiteral("encoding")] = item.encoding;
                updatedStyles.append(st);
            }
            if (!updatedStyles.isEmpty() && updatedStyles != model->styles()) {
                model->setStyles(updatedStyles);
            }

            // 3. Script Info (sync if modified or added by macro)
            QVariantMap updatedInfo = model->scriptInfo();
            bool infoModified = false;
            for (const auto &item : updated) {
                if (item.entryClass != AssEntryClass::Info) continue;
                if (updatedInfo.value(item.key).toString() != item.value) {
                    updatedInfo[item.key] = item.value;
                    infoModified = true;
                }
            }
            if (infoModified) {
                model->setScriptInfo(updatedInfo);
            }
        } else if (subtitleProject) {
            // Legacy path
            QVariantList outList;
            for (const auto &item : updated) {
                if (item.entryClass != AssEntryClass::Dialogue) continue;

                QVariantMap m;
                m["layer"] = item.layer;
                m["start"] = msToAssTime(item.startTime);
                m["end"] = msToAssTime(item.endTime);
                m["cps"] = "0";
                m["style"] = item.style;
                m["actor"] = item.actor;
                m["effect"] = item.effect;
                m["marginLeft"] = item.margin_l;
                m["marginRight"] = item.margin_r;
                m["marginVert"] = item.margin_t;
                m["text"] = item.text;
                m["comment"] = item.comment;
                outList.append(m);
            }
            QMetaObject::invokeMethod(subtitleProject, "setAllSubtitleLines", Q_ARG(QVariant, outList));
        }

        emit macroExecuted(macroName, true, undoMsg);
        emit statusMessage(undoMsg);
    } else {
        QString noModMsg = QString("Executed macro: %1 (no changes)").arg(macroName);
        emit macroExecuted(macroName, true, noModMsg);
        emit statusMessage(noModMsg);
    }

    return true;
}

} // namespace Automation
