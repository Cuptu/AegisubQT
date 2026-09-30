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
#include "FramerateExport.h"
#include <libaegisub/color.h>
#include "SubtitleModel.h"
#include "../bridge/AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QMetaObject>
#include <QUrl>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>
#include <limits>
#include <QSet>
#include <optional>

namespace Automation {

// Built-in IDs occupy a separate namespace from registered Lua filters.
static constexpr int FixStylesFilterId = -1;
static constexpr int FramerateFilterId = -2;

static double defaultInputFps(const QVariantMap &context) {
    const double fps = context.value("inputFps").toDouble();
    return std::isfinite(fps) && fps > 0 ? QString::number(fps, 'f', 3).toDouble() : 23.976;
}

static std::pair<agi::vfr::Framerate, agi::vfr::Framerate> exportRates(const QVariantMap &settings,
                                                                 const QVariantMap &context) {
    auto fps = [&](const char *name) {
        const auto value = settings.value(name, defaultInputFps(context));
        if (value.typeId() == QMetaType::Bool) throw std::invalid_argument("Invalid framerate value");
        bool ok = false;
        const double number = value.toDouble(&ok);
        if (!ok || !std::isfinite(number) || number <= 0 || number > 1000)
            throw std::invalid_argument("Framerate must be greater than zero and at most 1000");
        return agi::vfr::Framerate(number);
    };
    auto input = fps("inputFps");
    agi::vfr::Framerate output;
    const auto mode = settings.value("outputMode", context.value("isVfr").toBool() ? "Variable" : "Constant").toString();
    if (mode == "Constant") output = fps("outputFps");
    else if (mode == "Variable") {
        if (!context.value("isVfr").toBool()) throw std::invalid_argument("No variable-framerate timecodes are loaded");
        const auto times = context.value("timecodes").toList();
        if (times.size() < 2 || times.size() > 10'000'000) throw std::invalid_argument("Invalid variable-framerate timecodes");
        std::vector<int> milliseconds;
        milliseconds.reserve(times.size());
        for (const auto &time : times) {
            bool ok = false;
            const double number = time.toDouble(&ok);
            if (!ok || !std::isfinite(number) || number < 0 || number > std::numeric_limits<int>::max() || std::floor(number) != number)
                throw std::invalid_argument("Invalid variable-framerate timestamp");
            milliseconds.push_back(static_cast<int>(number));
        }
        output = agi::vfr::Framerate(std::move(milliseconds));
    } else throw std::invalid_argument("Invalid framerate output mode");
    const auto reverse = settings.value("reverse", false);
    if (reverse.typeId() != QMetaType::Bool) throw std::invalid_argument("Invalid reverse-transformation value");
    if (reverse.toBool()) std::swap(input, output);
    return {std::move(input), std::move(output)};
}

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

    QStringList candidateInclude = {
        appDir + "/automation/include",
        appDir + "/../Resources/automation/include",
        appDir + "/../share/AegisubQT/automation/include",
        AppPaths::dataDirectory() + "/automation/include"
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
    const auto canonical = QFileInfo(path).canonicalFilePath();
    if (!canonical.isEmpty() && QDir(canonical).exists() && !m_includePaths.contains(canonical)) {
        m_includePaths.append(canonical);
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
    m_cachedFilters.clear();
    m_cachedFilters.append(QVariantMap{
        {"id", FixStylesFilterId}, {"name", tr("Fix Styles")},
        {"description", tr("Replace styles that are not defined in the file with Default.")},
        {"priority", -5000}, {"hasConfig", false}, {"isBuiltin", true},
        {"scriptName", tr("Built-in")}, {"scriptPath", QString()}});
    m_cachedFilters.append(QVariantMap{
        {"id", FramerateFilterId}, {"name", tr("Transform Framerate")},
        {"description", tr("Transform subtitle times, including animations, fades and karaoke, between constant or variable framerates.")},
        {"priority", 1000}, {"hasConfig", true}, {"isBuiltin", true},
        {"scriptName", tr("Built-in")}, {"scriptPath", QString()}});

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
            for (const auto &filter : s->filters()) {
                m_cachedFilters.append(QVariantMap{
                    {"id", filter.id}, {"name", filter.name}, {"description", filter.description},
                    {"priority", filter.priority}, {"hasConfig", filter.configRef != LUA_NOREF},
                    {"scriptName", s->name()}, {"scriptPath", s->filepath()}});
            }
            for (const auto &m : s->macros()) {
                QVariantMap mMap;
                mMap["id"] = m.id;
                mMap["name"] = m.name;
                mMap["description"] = m.description;
                mMap["enabled"] = false;
                mMap["checkable"] = m.toggleRef != LUA_NOREF;
                mMap["checked"] = false;
                mMap["scriptName"] = s->name();
                mMap["scriptPath"] = s->filepath();
                m_cachedMacros.append(mMap);
            }
        }
    }

    std::stable_sort(m_cachedFilters.begin(), m_cachedFilters.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("priority").toInt() > b.toMap().value("priority").toInt();
    });
    emit scriptsChanged();
    emit macrosChanged();
    emit filtersChanged();
}

LuaVideoContext AutomationManager::videoContext() const {
    LuaVideoContext context;
    if (!m_video || !m_video->property("hasVideo").toBool()) return context;
    context.width = m_video->property("videoWidth").toInt();
    context.height = m_video->property("videoHeight").toInt();
    context.available = context.width > 0 && context.height > 0;
    if (!context.available) return context;
    const double override = m_videoDisplay ? m_videoDisplay->property("arOverride").toDouble() : 0;
    context.aspectRatio = override > 0 ? override : static_cast<double>(context.width) / context.height;
    context.aspectRatioType = m_videoDisplay ? m_videoDisplay->property("arOverrideType").toInt() : 0;
    return context;
}

void AutomationManager::scanAutoloadFolder() {
    QString appDir = QCoreApplication::applicationDirPath();

    QStringList candidateAutoload = {
        appDir + "/automation/autoload",
        appDir + "/../Resources/automation/autoload",
        appDir + "/../share/AegisubQT/automation/autoload",
        AppPaths::dataDirectory() + "/automation/autoload"
    };

    QStringList autoloadDirectories;
    for (const auto &path : candidateAutoload) {
        const auto canonical = QFileInfo(path).canonicalFilePath();
        if (!canonical.isEmpty() && QDir(canonical).exists() && !autoloadDirectories.contains(canonical))
            autoloadDirectories.append(canonical);
    }
    if (autoloadDirectories.isEmpty()) {
        qWarning() << "[AutomationManager] No autoload folder found";
        updateMacroList();
        return;
    }

    for (const QString &autoloadDir : autoloadDirectories) {
        qInfo() << "[AutomationManager] Scanning autoload folder:" << autoloadDir;
        QDir dir(autoloadDir);
        QStringList files = dir.entryList({"*.lua", "*.moon"}, QDir::Files, QDir::Name);

        for (const QString &f : files) {
            QString fullPath = QFileInfo(dir.filePath(f)).canonicalFilePath();
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
                s->setVideoContext(videoContext());
                s->load();
                m_entries.push_back({std::move(s), true});
            }
        }
    }
    updateMacroList();
}

bool AutomationManager::addScript(const QString &filepath, bool isGlobal) {
    QString cleanPath = filepath;
    if (cleanPath.startsWith(QStringLiteral("file:"))) {
        const QUrl url(cleanPath);
        if (url.isLocalFile()) cleanPath = url.toLocalFile();
    }
    if (!QFileInfo::exists(cleanPath)) return false;

    auto s = std::make_unique<LuaScript>(cleanPath, m_includePaths);
    s->setVideoContext(videoContext());
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
        m_entries[index].script->setVideoContext(videoContext());
        m_entries[index].script->reload();
        updateMacroList();
    }
}

void AutomationManager::reloadAll() {
    for (auto &e : m_entries) {
        e.script->setVideoContext(videoContext());
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

struct MacroInput {
    SubtitleModel *model;
    std::vector<AssEntryData> lines;
    int resX, resY;
    std::vector<int> luaSelected;
    int luaActive;
};
static MacroInput macroInput(QObject *subtitleProject, SubtitleModel *fallbackModel,
                             int activeIndex, const QVariantList &selectedIndices) {
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
        model = fallbackModel;
    }

    // Prepare ASS lines
    std::vector<AssEntryData> lines;
    int resX = 1920;
    int resY = 1080;

    if (model) {
        // 1. Script Info from native model
        QVariantMap info = model->scriptInfo();
        resX = info.value(QStringLiteral("PlayResX"), 1920).toInt();
        resY = info.value(QStringLiteral("PlayResY"), 1080).toInt();
        if (resX <= 0) resX = 1920;
        if (resY <= 0) resY = 1080;

        for (auto it = info.cbegin(); it != info.cend(); ++it) {
            lines.push_back({AssEntryClass::Info, QStringLiteral("[Script Info]"), it.key(), it.value().toString()});
        }

        // 2. Styles from native model
        QVariantList stylesList = model->styles();

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
            dia.extra = item.extra;
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
                    dia.extra = assExtraFromVariant(map.value("extra"));
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

    std::vector<int> luaSelected;
    const int count = static_cast<int>(lines.size()) - headerOffset;
    for (const auto &value : selectedIndices) {
        const int row = value.toInt();
        if (row >= 0 && row < count) luaSelected.push_back(row + headerOffset + 1);
    }
    const int luaActive = activeIndex >= 0 && activeIndex < count ? activeIndex + headerOffset + 1 : 0;
    if (luaSelected.empty() && luaActive) luaSelected.push_back(luaActive);
    return {model, std::move(lines), resX, resY, std::move(luaSelected), luaActive};
}

static void applyAssEntries(SubtitleModel &model, const std::vector<AssEntryData> &updated) {
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
        dl.extra = item.extra;
        dl.updateCps();
        updatedLines.push_back(std::move(dl));
    }
    model.setRawLines(std::move(updatedLines));

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
    if (updatedStyles != model.styles()) {
        model.setStyles(updatedStyles);
    }

    // 3. Script Info (sync if modified or added by macro)
    const QVariantMap originalInfo = model.scriptInfo();
    QVariantMap updatedInfo;
    for (const auto &item : updated) {
        if (item.entryClass != AssEntryClass::Info) continue;
        // Rebuild the final set, so deleted keys stay deleted. Preserve
        // native QVariant types for values whose text is unchanged.
        updatedInfo[item.key] = originalInfo.contains(item.key) &&
                originalInfo.value(item.key).toString() == item.value
            ? originalInfo.value(item.key) : QVariant(item.value);
    }
    if (updatedInfo != originalInfo) {
        model.setScriptInfo(updatedInfo);
    }
}

QVariantMap AutomationManager::normalizeConfigColor(const QString &text, bool withAlpha) const {
    const auto bytes = text.toUtf8();
    agi::Color color;
    if (!agi::Color::TryParse(color, std::string_view(bytes.constData(), bytes.size())))
        return {{"success", false}, {"message", tr("Invalid color: %1").arg(text)}};
    return {{"success", true}, {"value", QString::fromStdString(color.GetHexFormatted(withAlpha))}};
}

QVariantMap AutomationManager::exportFilterConfig(int filterId, QObject *subtitleProject,
                                                   const QVariantMap &settings) {
    auto input = macroInput(subtitleProject, m_subtitleModel, -1, {});
    if (!input.model) return {{"success", false}, {"message", tr("No subtitle document is open")}};
    if (filterId == FixStylesFilterId) return {{"success", true}, {"controls", QVariantList{}}, {"message", QString()}};
    if (filterId == FramerateFilterId) {
        const auto context = m_video ? m_video->property("exportFramerateContext").toMap() : QVariantMap{};
        const auto defaultFps = defaultInputFps(context);
        const auto mode = context.value("isVfr").toBool() ? "Variable" : "Constant";
        QVariantList controls{
            QVariantMap{{"class", "label"}, {"label", tr("Input framerate:")}, {"x", 0}, {"y", 0}},
            QVariantMap{{"class", "floatedit"}, {"name", "inputFps"}, {"value", settings.value("inputFps", defaultFps)}, {"min", 1e-9}, {"max", 1000}, {"x", 1}, {"y", 0}},
            QVariantMap{{"class", "button"}, {"label", tr("From video")}, {"target", "inputFps"}, {"value", context.value("outputFps")}, {"enabled", context.value("available").toBool()}, {"x", 2}, {"y", 0}},
            QVariantMap{{"class", "label"}, {"label", tr("Output:")}, {"x", 0}, {"y", 1}},
            QVariantMap{{"class", "dropdown"}, {"name", "outputMode"}, {"items", context.value("isVfr").toBool() ? QVariantList{"Constant", "Variable"} : QVariantList{"Constant"}}, {"value", settings.value("outputMode", mode)}, {"x", 1}, {"y", 1}},
            QVariantMap{{"class", "floatedit"}, {"name", "outputFps"}, {"value", settings.value("outputFps", defaultFps)}, {"min", 1e-9}, {"max", 1000}, {"nativeEnabledWhen", QVariantMap{{"field", "outputMode"}, {"value", "Constant"}}}, {"x", 2}, {"y", 1}},
            QVariantMap{{"class", "checkbox"}, {"name", "reverse"}, {"label", tr("Reverse transformation")}, {"value", settings.value("reverse", false)}, {"x", 0}, {"y", 2}, {"width", 3}}
        };
        return {{"success", true}, {"controls", controls}, {"message", QString()}};
    }
    LuaAssFileBridge source(nullptr, std::move(input.lines), input.resX, input.resY);
    for (auto &entry : m_entries) {
        for (const auto &filter : entry.script->filters()) {
            if (filter.id != filterId) continue;
            entry.script->setVideoContext(videoContext());
            QVariantList controls;
            QString error;
            const bool success = entry.script->filterConfig(filterId, source, settings, controls, error);
            return {{"success", success}, {"controls", controls}, {"message", error}};
        }
    }
    return {{"success", false}, {"message", tr("Export filter is no longer available")}};
}

QVariantMap AutomationManager::exportSubtitles(const QString &filePath, const QString &charset,
                                               const QVariantList &pipeline, QObject *subtitleProject) {
    auto fail = [](const QString &error) -> QVariantMap {
        return {{"success", false}, {"message", error}};
    };
    auto input = macroInput(subtitleProject, m_subtitleModel, -1, {});
    if (!input.model) return fail(tr("No subtitle document is open"));
    struct Step { LuaScript *script; int id; QString name; QVariantMap settings; std::optional<std::pair<agi::vfr::Framerate, agi::vfr::Framerate>> rates; };
    std::vector<Step> steps;
    for (const auto &value : pipeline) {
        if (value.typeId() != QMetaType::QVariantMap) return fail(tr("Invalid export filter selection"));
        const auto step = value.toMap();
        const auto idValue = step.value("id");
        const int type = idValue.typeId();
        const double number = idValue.toDouble();
        if ((type != QMetaType::Int && type != QMetaType::Double && type != QMetaType::LongLong) ||
            !std::isfinite(number) || std::floor(number) != number || number == 0 || number < std::numeric_limits<int>::min() ||
            number > std::numeric_limits<int>::max()) return fail(tr("Invalid export filter ID"));
        if (step.contains("settings") && step.value("settings").typeId() != QMetaType::QVariantMap)
            return fail(tr("Invalid export filter settings"));
        const int id = static_cast<int>(number);
        if (id == FixStylesFilterId) {
            steps.push_back({nullptr, id, tr("Fix Styles"), {}});
            continue;
        }
        if (id == FramerateFilterId) {
            try {
                const auto context = m_video ? m_video->property("exportFramerateContext").toMap() : QVariantMap{};
                steps.push_back({nullptr, id, tr("Transform Framerate"), {}, exportRates(step.value("settings").toMap(), context)});
            } catch (const std::exception &error) { return fail(QString::fromUtf8(error.what())); }
            continue;
        }
        bool found = false;
        for (auto &entry : m_entries) {
            for (const auto &filter : entry.script->filters()) {
                if (filter.id != id) continue;
                steps.push_back({entry.script.get(), id, filter.name, step.value("settings").toMap()});
                found = true;
                break;
            }
            if (found) break;
        }
        if (!found) return fail(tr("Export filter is no longer available"));
    }
    auto lines = std::move(input.lines);
    for (auto &step : steps) {
        if (step.id == FramerateFilterId) {
            QString error;
            if (!FramerateExport::transform(lines, step.rates->first, step.rates->second, error))
                return fail(tr("Export filter '%1' failed: %2").arg(step.name, error));
            continue;
        }
        if (step.id == FixStylesFilterId) {
            QSet<QString> styles;
            for (const auto &line : lines)
                if (line.entryClass == AssEntryClass::Style) styles.insert(line.styleName.toLower());
            for (auto &line : lines)
                if (line.entryClass == AssEntryClass::Dialogue && !styles.contains(line.style.toLower()))
                    line.style = QStringLiteral("Default");
            continue;
        }
        step.script->setVideoContext(videoContext());
        QString error;
        if (!step.script->runFilter(step.id, lines, input.resX, input.resY, step.settings, error))
            return fail(tr("Export filter '%1' failed: %2").arg(step.name, error));
    }
    SubtitleModel transformed;
    applyAssEntries(transformed, lines);
    if (!input.model->exportToFile(filePath, charset, transformed))
        return fail(tr("Could not export subtitles. Check the filename (.ass or .srt), encoding and write permissions."));
    return {{"success", true}, {"message", tr("Subtitles exported to %1").arg(filePath)}};
}

void AutomationManager::refreshMacroStates(QObject *subtitleProject, int activeIndex, const QVariantList &selectedIndices) {
    auto input = macroInput(subtitleProject, m_subtitleModel, activeIndex, selectedIndices);
    LuaAssFileBridge source(nullptr, std::move(input.lines), input.resX, input.resY);
    bool changed = false;
    for (auto &entry : m_entries) {
        entry.script->setVideoContext(videoContext());
        for (const auto &macro : entry.script->macros()) {
            const auto state = entry.script->macroState(macro.id, source, input.luaSelected, input.luaActive);
            for (auto &value : m_cachedMacros) {
                auto map = value.toMap();
                if (map.value("id").toInt() != macro.id) continue;
                map["enabled"] = state.enabled;
                map["checkable"] = state.checkable;
                map["checked"] = state.checked;
                map["description"] = state.description;
                if (map != value.toMap()) { value = map; changed = true; }
                break;
            }
        }
    }
    if (changed) emit macrosChanged();
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

    auto input = macroInput(subtitleProject, m_subtitleModel, activeIndex, selectedIndices);
    auto *model = input.model;
    auto &lines = input.lines;
    const int resX = input.resX, resY = input.resY;
    const auto &luaSelected = input.luaSelected;
    const int luaActive = input.luaActive;

    // Create bridge & run macro
    LuaAssFileBridge bridge(nullptr, std::move(lines), resX, resY);
    QString err;
    LuaMacroSelection selection;
    targetScript->setVideoContext(videoContext());
    bool ok = targetScript->runMacro(macroId, bridge, luaSelected, luaActive, err, &selection);

    if (!ok) {
        qWarning() << "[AutomationManager] Macro execution error:" << err;
        emit macroExecuted(macroName, false, err);
        emit statusMessage(QString("Macro execution failed: %1").arg(err));
        return false;
    }

    const std::vector<AssEntryData> updated = bridge.getLines();
    // Apply back modified lines to model or project
    if (bridge.isModified()) {
        QString undoMsg;
        auto applyCommit = [&](const std::vector<AssEntryData> &updated, const QString &description) {
            undoMsg = description;
            if (model) {
                // Preserve the before-state for this Lua checkpoint
                model->pushUndo(undoMsg, activeIndex, selectedIndices);

                applyAssEntries(*model, updated);
            } else if (subtitleProject) {
                // Legacy wrappers can expose the same undo API as SubtitleProject.
                QMetaObject::invokeMethod(subtitleProject, "pushUndo", Q_ARG(QVariant, QVariant(description)));
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
                    m["extra"] = assExtraToVariant(item.extra);
                    outList.append(m);
                }
                QMetaObject::invokeMethod(subtitleProject, "setAllSubtitleLines", Q_ARG(QVariant, outList));
            }

        };
        // Lua only publishes checkpoints after the whole callback succeeds.
        // Each checkpoint gets its own native before-state and undo label.
        for (const auto &commit : bridge.pendingCommits()) applyCommit(commit.lines, commit.description);
        if (bridge.hasUncommittedChanges()) applyCommit(updated, QString("Macro: %1").arg(macroName));

        emit macroExecuted(macroName, true, undoMsg);
        emit statusMessage(undoMsg);
    } else {
        QString noModMsg = QString("Executed macro: %1 (no changes)").arg(macroName);
        emit macroExecuted(macroName, true, noModMsg);
        emit statusMessage(noModMsg);
    }

    if (selection.hasSelection || selection.hasActive || bridge.isModified()) {
        // Lua indices address the whole final ASS file. Build a mapping rather
        // than subtracting the old header offset after rows have been deleted.
        std::vector<int> rows(updated.size(), -1);
        int count = 0;
        for (size_t i = 0; i < updated.size(); ++i)
            if (updated[i].entryClass == AssEntryClass::Dialogue) rows[i] = count++;
        auto mapIndex = [&](int index) -> int {
            if (index >= 1 && index <= static_cast<int>(rows.size()) && rows[index - 1] >= 0)
                return rows[index - 1];
            qWarning() << "[AutomationManager] Ignoring macro index outside final dialogue rows:" << index;
            return -1;
        };
        std::vector<int> selected;
        if (selection.hasSelection) {
            for (int index : selection.selectedLines) {
                const int row = mapIndex(index);
                if (row >= 0) selected.push_back(row);
            }
        } else {
            for (const auto &index : selectedIndices)
                if (index.toInt() >= 0 && index.toInt() < count) selected.push_back(index.toInt());
        }
        std::sort(selected.begin(), selected.end());
        selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
        int active = count ? std::clamp(activeIndex, 0, count - 1) : -1;
        if (selection.hasActive) {
            const int row = mapIndex(selection.activeLine);
            if (row >= 0) active = row;
        }
        if (selected.empty() && active >= 0) selected.push_back(active);
        if (!selected.empty() && std::find(selected.begin(), selected.end(), active) == selected.end())
            active = selected.front();
        QVariantList finalSelection;
        for (int row : selected) finalSelection.append(row);
        emit macroSelectionChanged(subtitleProject ? subtitleProject : model, active, finalSelection);
    }
    return true;
}

} // namespace Automation
