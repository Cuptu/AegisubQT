// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

#include "SubtitleModel.h"
#include "ResolutionResampler.h"
#include "InlineFormatting.h"
#include "SrtExport.h"
#include "SrtImport.h"
#include "AegisubCoreBridge.h"
#include <libaegisub/ass/time.h>
#include <libaegisub/color.h>
#include <libaegisub/ass/uuencode.h>
#include <libaegisub/ass/string_codec.h>
#include <libaegisub/vfr.h>
#include <QRegularExpression>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <QStringConverter>
#include <QTextCodec>
#include <QUrl>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <cmath>
#include <algorithm>
#include <climits>

// SubtitleLine implementations

void SubtitleLine::setStartMs(int ms) {
    startMs = std::max(0, ms);
    agi::Time t(startMs);
    startStr = QString::fromStdString(t.GetAssFormatted());
    updateCps();
}

void SubtitleLine::setEndMs(int ms) {
    endMs = std::max(0, ms);
    agi::Time t(endMs);
    endStr = QString::fromStdString(t.GetAssFormatted());
    updateCps();
}

void SubtitleLine::setStartStr(const QString &str) {
    startStr = str;
    agi::Time t(str.toStdString());
    startMs = static_cast<int>(t);
    updateCps();
}

void SubtitleLine::setEndStr(const QString &str) {
    endStr = str;
    agi::Time t(str.toStdString());
    endMs = static_cast<int>(t);
    updateCps();
}

void SubtitleLine::updateCps() {
    cps = calculateCps(text, startMs, endMs);
}

QString SubtitleLine::calculateCps(const QString &text, int startMs, int endMs) {
    double dur = (endMs - startMs) / 1000.0;
    if (dur <= 0.0) return QStringLiteral("0");
    static const QRegularExpression tagRegex(QStringLiteral(R"(\{[^\}]*\})"));
    QString clean = text;
    clean.remove(tagRegex);
    clean = clean.trimmed();
    int cpsVal = static_cast<int>(std::round(clean.length() / dur));
    return QString::number(cpsVal);
}

QVariantMap SubtitleLine::toMap() const {
    QVariantMap map;
    map[QStringLiteral("lineNumber")] = lineNumber;
    map[QStringLiteral("layer")] = layer;
    map[QStringLiteral("start")] = startStr;
    map[QStringLiteral("end")] = endStr;
    map[QStringLiteral("cps")] = cps;
    map[QStringLiteral("style")] = style;
    map[QStringLiteral("actor")] = actor;
    map[QStringLiteral("effect")] = effect;
    map[QStringLiteral("marginLeft")] = marginLeft;
    map[QStringLiteral("marginRight")] = marginRight;
    map[QStringLiteral("marginVert")] = marginVert;
    map[QStringLiteral("text")] = text;
    map[QStringLiteral("isComment")] = isComment;
    map[QStringLiteral("comment")] = isComment;
    map[QStringLiteral("extra")] = assExtraToVariant(extra);
    return map;
}

SubtitleLine SubtitleLine::fromMap(const QVariantMap &map, int fallbackLineNumber) {
    SubtitleLine line;
    line.lineNumber = map.value(QStringLiteral("lineNumber"), fallbackLineNumber).toInt();
    line.layer = map.value(QStringLiteral("layer"), 0).toInt();
    line.startStr = map.value(QStringLiteral("start"), QStringLiteral("0:00:00.00")).toString();
    line.endStr = map.value(QStringLiteral("end"), QStringLiteral("0:00:05.00")).toString();
    line.setStartStr(line.startStr);
    line.setEndStr(line.endStr);
    line.style = map.value(QStringLiteral("style"), QStringLiteral("Default")).toString();
    line.actor = map.value(QStringLiteral("actor"), QString()).toString();
    line.effect = map.value(QStringLiteral("effect"), QString()).toString();
    line.marginLeft = map.value(QStringLiteral("marginLeft"), 0).toInt();
    line.marginRight = map.value(QStringLiteral("marginRight"), 0).toInt();
    line.marginVert = map.value(QStringLiteral("marginVert"), 0).toInt();
    line.text = map.value(QStringLiteral("text"), QString()).toString();
    line.isComment = map.value(QStringLiteral("isComment"), map.value(QStringLiteral("comment"), false)).toBool();
    line.extra = assExtraFromVariant(map.value(QStringLiteral("extra")));
    line.updateCps();
    return line;
}

// SubtitleModel implementations

QVariantMap SubtitleModel::defaultScriptInfo() {
    QVariantMap info;
    info[QStringLiteral("Title")] = QStringLiteral("Default Aegisub file");
    info[QStringLiteral("ScriptType")] = QStringLiteral("v4.00+");
    info[QStringLiteral("WrapStyle")] = QStringLiteral("0");
    info[QStringLiteral("ScaledBorderAndShadow")] = QStringLiteral("yes");
    info[QStringLiteral("YCbCr Matrix")] = QStringLiteral("None");
    if (!AegisubCoreBridge::getSetting("Subtitle/Default Resolution/Auto", true).toBool()) {
        info[QStringLiteral("PlayResX")] = AegisubCoreBridge::getSetting("Subtitle/Default Resolution/Width", 1280);
        info[QStringLiteral("PlayResY")] = AegisubCoreBridge::getSetting("Subtitle/Default Resolution/Height", 720);
    }
    return info;
}

QVariantList SubtitleModel::defaultStyles() {
    QVariantList list;
    QVariantMap st;
    st[QStringLiteral("name")] = QStringLiteral("Default");
    st[QStringLiteral("font")] = QStringLiteral("Arial");
    st[QStringLiteral("size")] = 48.0;
    st[QStringLiteral("primary")] = QStringLiteral("&H00FFFFFF");
    st[QStringLiteral("secondary")] = QStringLiteral("&H000000FF");
    st[QStringLiteral("outline")] = QStringLiteral("&H00000000");
    st[QStringLiteral("shadow")] = QStringLiteral("&H00000000");
    st[QStringLiteral("bold")] = false;
    st[QStringLiteral("italic")] = false;
    st[QStringLiteral("underline")] = false;
    st[QStringLiteral("strikeout")] = false;
    st[QStringLiteral("scaleX")] = 100.0;
    st[QStringLiteral("scaleY")] = 100.0;
    st[QStringLiteral("spacing")] = 0.0;
    st[QStringLiteral("angle")] = 0.0;
    st[QStringLiteral("borderStyle")] = 1;
    st[QStringLiteral("outlineWidth")] = 2.0;
    st[QStringLiteral("shadowDepth")] = 2.0;
    st[QStringLiteral("alignment")] = 2;
    st[QStringLiteral("marginL")] = 10;
    st[QStringLiteral("marginR")] = 10;
    st[QStringLiteral("marginV")] = 10;
    st[QStringLiteral("encoding")] = 1;
    list.append(st);
    return list;
}

SubtitleModel::SubtitleModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_scriptInfo(defaultScriptInfo())
    , m_styles(defaultStyles()) {
    // Follows Aegisub's native SubsController design: increment commit_id on each mutation signal.
    // Centralizing this hook on contentModified eliminates manual bookkeeping across 40+ mutation sites.
    connect(this, &SubtitleModel::contentModified, this, [this]() {
        if (m_restoringSnapshot) return;
        const bool wasModified = isModified();
        ++m_commitId;
        if (!m_redoStack.empty()) {
            m_redoStack.clear();
            Q_EMIT undoStateChanged();
        }
        if (!wasModified) {
            Q_EMIT modifiedChanged();
        }
    });
}

int SubtitleModel::tryToClose() {
    // 0 = Can close without prompt (clean document); 1 = Prompt user for unsaved changes.
    return isModified() ? 1 : 0;
}

void SubtitleModel::markModified() {
    const bool wasModified = isModified();
    ++m_commitId;
    if (!wasModified) {
        Q_EMIT modifiedChanged();
    }
}

void SubtitleModel::markSaved() {
    if (m_savedCommitId != m_commitId) {
        m_savedCommitId = m_commitId;
        Q_EMIT modifiedChanged();
    }
}

void SubtitleModel::resetModificationTracking() {
    const bool wasModified = isModified();
    m_commitId = 0;
    m_savedCommitId = 0;
    if (wasModified) {
        Q_EMIT modifiedChanged();
    }
}

int SubtitleModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_lines.size());
}

QVariant SubtitleModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_lines.size())) {
        return QVariant();
    }
    const auto &line = m_lines[index.row()];
    switch (role) {
    case LineNumberRole: return line.lineNumber;
    case LayerRole: return line.layer;
    case StartRole: return line.startStr;
    case EndRole: return line.endStr;
    case CpsRole: return line.cps;
    case StyleRole: return line.style;
    case ActorRole: return line.actor;
    case EffectRole: return line.effect;
    case MarginLeftRole: return line.marginLeft;
    case MarginRightRole: return line.marginRight;
    case MarginVertRole: return line.marginVert;
    case TextRole: return line.text;
    case IsCommentRole: return line.isComment;
    default: return QVariant();
    }
}

QHash<int, QByteArray> SubtitleModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[LineNumberRole] = "lineNumber";
    roles[LayerRole] = "layer";
    roles[StartRole] = "start";
    roles[EndRole] = "end";
    roles[CpsRole] = "cps";
    roles[StyleRole] = "style";
    roles[ActorRole] = "actor";
    roles[EffectRole] = "effect";
    roles[MarginLeftRole] = "marginLeft";
    roles[MarginRightRole] = "marginRight";
    roles[MarginVertRole] = "marginVert";
    roles[TextRole] = "text";
    roles[IsCommentRole] = "isComment";
    return roles;
}

QVariantMap SubtitleModel::get(int index) const {
    if (index < 0 || index >= static_cast<int>(m_lines.size())) {
        return QVariantMap();
    }
    return m_lines[index].toMap();
}

bool SubtitleModel::setProperty(int index, const QString &propertyName, const QVariant &value) {
    if (index < 0 || index >= static_cast<int>(m_lines.size())) return false;
    auto &line = m_lines[index];
    QVector<int> changedRoles;

    if (propertyName == QStringLiteral("text")) {
        if (line.text == value.toString()) return true;
        line.text = value.toString();
        line.updateCps();
        changedRoles << TextRole << CpsRole;
    } else if (propertyName == QStringLiteral("start")) {
        if (line.startStr == value.toString()) return true;
        line.setStartStr(value.toString());
        changedRoles << StartRole << CpsRole;
    } else if (propertyName == QStringLiteral("end")) {
        if (line.endStr == value.toString()) return true;
        line.setEndStr(value.toString());
        changedRoles << EndRole << CpsRole;
    } else if (propertyName == QStringLiteral("style")) {
        if (line.style == value.toString()) return true;
        line.style = value.toString();
        changedRoles << StyleRole;
    } else if (propertyName == QStringLiteral("actor")) {
        if (line.actor == value.toString()) return true;
        line.actor = value.toString();
        changedRoles << ActorRole;
    } else if (propertyName == QStringLiteral("effect")) {
        if (line.effect == value.toString()) return true;
        line.effect = value.toString();
        changedRoles << EffectRole;
    } else if (propertyName == QStringLiteral("layer")) {
        if (line.layer == value.toInt()) return true;
        line.layer = value.toInt();
        changedRoles << LayerRole;
    } else if (propertyName == QStringLiteral("marginLeft")) {
        if (line.marginLeft == value.toInt()) return true;
        line.marginLeft = value.toInt();
        changedRoles << MarginLeftRole;
    } else if (propertyName == QStringLiteral("marginRight")) {
        if (line.marginRight == value.toInt()) return true;
        line.marginRight = value.toInt();
        changedRoles << MarginRightRole;
    } else if (propertyName == QStringLiteral("marginVert")) {
        if (line.marginVert == value.toInt()) return true;
        line.marginVert = value.toInt();
        changedRoles << MarginVertRole;
    } else if (propertyName == QStringLiteral("lineNumber")) {
        if (line.lineNumber == value.toInt()) return true;
        line.lineNumber = value.toInt();
        changedRoles << LineNumberRole;
    } else if (propertyName == QStringLiteral("isComment") || propertyName == QStringLiteral("comment")) {
        if (line.isComment == value.toBool()) return true;
        line.isComment = value.toBool();
        changedRoles << IsCommentRole;
    } else {
        return false;
    }

    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, changedRoles);
    emit contentModified();
    return true;
}

void SubtitleModel::append(const QVariantMap &item) {
    int row = static_cast<int>(m_lines.size());
    beginInsertRows(QModelIndex(), row, row);
    m_lines.push_back(SubtitleLine::fromMap(item, row + 1));
    endInsertRows();
    emit countChanged();
    emit contentModified();
}

void SubtitleModel::insert(int index, const QVariantMap &item) {
    int row = qBound(0, index, static_cast<int>(m_lines.size()));
    beginInsertRows(QModelIndex(), row, row);
    m_lines.insert(m_lines.begin() + row, SubtitleLine::fromMap(item, row + 1));
    endInsertRows();
    emit countChanged();
    emit contentModified();
}

void SubtitleModel::remove(int index) {
    if (index < 0 || index >= static_cast<int>(m_lines.size())) return;
    beginRemoveRows(QModelIndex(), index, index);
    m_lines.erase(m_lines.begin() + index);
    endRemoveRows();
    emit countChanged();
    emit contentModified();
}

void SubtitleModel::clear() {
    if (m_lines.empty()) return;
    beginResetModel();
    m_lines.clear();
    endResetModel();
    emit countChanged();
    emit contentModified();
}

void SubtitleModel::renumberLines() {
    bool anyChanged = false;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        int expected = static_cast<int>(i + 1);
        if (m_lines[i].lineNumber != expected) {
            m_lines[i].lineNumber = expected;
            anyChanged = true;
        }
    }
    if (anyChanged && !m_lines.empty()) {
        emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(m_lines.size() - 1), 0), { LineNumberRole });
    }
}

void SubtitleModel::pushUndo(const QString &description, int selectedIndex, const QVariantList &selectedIndices) {
    QList<int> selList;
    for (const auto &v : selectedIndices) selList.append(v.toInt());
    if (selList.isEmpty()) selList.append(selectedIndex);
    pushUndo(description, selectedIndex, selList);
}

void SubtitleModel::pushUndo(const QString &description, int selectedIndex, const QList<int> &selectedIndices) {
    QList<int> selList = selectedIndices;
    if (selList.isEmpty()) selList.append(selectedIndex);

    auto snapshotLines = std::make_shared<const std::vector<SubtitleLine>>(m_lines);
    m_undoStack.push_back({ description, snapshotLines, selectedIndex, selList, m_scriptInfo, m_styles, m_rawSections, m_commitId });
    if (m_undoStack.size() > kMaxUndoLevels) {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_redoStack.clear();
    emit undoStateChanged();
}

QVariantMap SubtitleModel::undo() {
    if (m_undoStack.empty()) return QVariantMap();

    auto currentLines = std::make_shared<const std::vector<SubtitleLine>>(m_lines);
    UndoSnapshot currentSnapshot;
    currentSnapshot.description = m_undoStack.back().description;
    currentSnapshot.lines = currentLines;
    currentSnapshot.selectedIndex = m_undoStack.back().selectedIndex;
    currentSnapshot.selectedIndices = m_undoStack.back().selectedIndices;
    currentSnapshot.scriptInfo = m_scriptInfo;
    currentSnapshot.styles = m_styles;
    currentSnapshot.rawSections = m_rawSections;
    currentSnapshot.commitId = m_commitId;
    m_redoStack.push_back(currentSnapshot);

    UndoSnapshot target = m_undoStack.back();
    m_undoStack.pop_back();

    const bool wasModified = isModified();
    beginResetModel();
    m_lines = *target.lines;
    m_scriptInfo = target.scriptInfo;
    m_styles = target.styles;
    m_rawSections = target.rawSections;
    endResetModel();

    emit scriptInfoChanged();
    emit stylesChanged();
    emit attachmentsChanged();
    emit countChanged();
    emit undoStateChanged();
    m_commitId = target.commitId;
    m_restoringSnapshot = true;
    emit contentModified();
    m_restoringSnapshot = false;
    if (wasModified != isModified()) emit modifiedChanged();

    QVariantMap result;
    result[QStringLiteral("selectedIndex")] = target.selectedIndex;
    QVariantList selList;
    for (int idx : target.selectedIndices) selList.append(idx);
    result[QStringLiteral("selectedIndices")] = selList;
    return result;
}

QVariantMap SubtitleModel::redo() {
    if (m_redoStack.empty()) return QVariantMap();

    auto currentLines = std::make_shared<const std::vector<SubtitleLine>>(m_lines);
    UndoSnapshot currentSnapshot;
    currentSnapshot.description = m_redoStack.back().description;
    currentSnapshot.lines = currentLines;
    currentSnapshot.selectedIndex = m_redoStack.back().selectedIndex;
    currentSnapshot.selectedIndices = m_redoStack.back().selectedIndices;
    currentSnapshot.scriptInfo = m_scriptInfo;
    currentSnapshot.styles = m_styles;
    currentSnapshot.rawSections = m_rawSections;
    currentSnapshot.commitId = m_commitId;
    m_undoStack.push_back(currentSnapshot);

    UndoSnapshot target = m_redoStack.back();
    m_redoStack.pop_back();

    const bool wasModified = isModified();
    beginResetModel();
    m_lines = *target.lines;
    m_scriptInfo = target.scriptInfo;
    m_styles = target.styles;
    m_rawSections = target.rawSections;
    endResetModel();

    emit scriptInfoChanged();
    emit stylesChanged();
    emit attachmentsChanged();
    emit countChanged();
    emit undoStateChanged();
    m_commitId = target.commitId;
    m_restoringSnapshot = true;
    emit contentModified();
    m_restoringSnapshot = false;
    if (wasModified != isModified()) emit modifiedChanged();

    QVariantMap result;
    result[QStringLiteral("selectedIndex")] = target.selectedIndex;
    QVariantList selList;
    for (int idx : target.selectedIndices) selList.append(idx);
    result[QStringLiteral("selectedIndices")] = selList;
    return result;
}

void SubtitleModel::clearUndo() {
    m_undoStack.clear();
    m_redoStack.clear();
    emit undoStateChanged();
}

QString SubtitleModel::undoDescription() const {
    if (m_undoStack.empty()) return QString();
    return m_undoStack.back().description;
}

QString SubtitleModel::redoDescription() const {
    if (m_redoStack.empty()) return QString();
    return m_redoStack.back().description;
}

void SubtitleModel::setScriptInfo(const QVariantMap &info) {
    if (m_scriptInfo != info) {
        m_scriptInfo = info;
        emit scriptInfoChanged();
        emit contentModified();
    }
}

QVariant SubtitleModel::getScriptInfo(const QString &key, const QVariant &defaultValue) const {
    return m_scriptInfo.value(key, defaultValue);
}

void SubtitleModel::setScriptInfoKey(const QString &key, const QVariant &value) {
    if (m_scriptInfo.value(key) != value) {
        m_scriptInfo[key] = value;
        emit scriptInfoChanged();
        emit contentModified();
    }
}

void SubtitleModel::setStyles(const QVariantList &stylesList) {
    if (m_styles != stylesList) {
        m_styles = stylesList;
        emit stylesChanged();
        emit contentModified();
    }
}

QStringList SubtitleModel::styleNames() const {
    QStringList names;
    names.reserve(m_styles.size());
    for (const auto &v : m_styles) {
        QString n = v.toMap().value(QStringLiteral("name")).toString();
        if (!n.isEmpty() && !names.contains(n)) {
            names.append(n);
        }
    }
    if (names.isEmpty()) names.append(QStringLiteral("Default"));
    return names;
}

QVariantMap SubtitleModel::getStyle(int index) const {
    if (index >= 0 && index < m_styles.size()) {
        return m_styles[index].toMap();
    }
    return QVariantMap();
}

QVariantMap SubtitleModel::getStyleByName(const QString &name) const {
    for (const auto &v : m_styles) {
        QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("name")).toString().compare(name, Qt::CaseInsensitive) == 0) {
            return m;
        }
    }
    return getStyle(0);
}

void SubtitleModel::setStyle(int index, const QVariantMap &style) {
    editStyle(index, style, true);
}

QVariantMap SubtitleModel::styleEditPlan(int index, const QVariantMap &style) const {
    if (index < 0 || index >= m_styles.size()) return {{"success", false}, {"message", tr("The style no longer exists")}};
    const auto name = style.value("name").toString();
    if (name.isEmpty() || name.contains(',') || name.contains('\r') || name.contains('\n') || name.contains(QChar(0)))
        return {{"success", false}, {"message", tr("Enter a valid style name without commas or line breaks")}};
    for (int i = 0; i < m_styles.size(); ++i) {
        if (i != index && m_styles[i].toMap().value("name").toString().compare(name, Qt::CaseInsensitive) == 0)
            return {{"success", false}, {"message", tr("There is already a style with this name")}};
    }
    const auto oldStyle = m_styles[index].toMap();
    const auto sourceName = oldStyle.value("name").toString();
    bool referenced = false;
    if (sourceName != name) {
        try {
            for (const auto &line : m_lines) {
                if (line.style.compare(sourceName, Qt::CaseInsensitive) == 0) { referenced = true; break; }
                Automation::AssOverrides::processText(line.text, [&](const QString &tag, Automation::AssOverrides::Parameter &parameter) {
                    if (tag == "\\r" && parameter.value.compare(sourceName, Qt::CaseInsensitive) == 0) referenced = true;
                });
                if (referenced) break;
            }
        }
        catch (const std::exception &error) { return {{"success", false}, {"message", QString::fromUtf8(error.what())}}; }
    }
    return {{"success", true}, {"changed", oldStyle != style}, {"needsConfirmation", referenced}, {"oldName", sourceName}};
}

QVariantMap SubtitleModel::editStyle(int index, const QVariantMap &style, bool updateReferences, int selectedIndex, const QVariantList &selectedIndices) {
    auto plan = styleEditPlan(index, style);
    if (!plan.value("success").toBool() || !plan.value("changed").toBool()) return plan;
    auto lines = m_lines;
    bool changedLines = false;
    if (updateReferences && plan.value("needsConfirmation").toBool()) {
        const auto source = plan.value("oldName").toString();
        const auto destination = style.value("name").toString();
        try {
            for (auto &line : lines) {
                if (line.style.compare(source, Qt::CaseInsensitive) == 0) { line.style = destination; changedLines = true; }
                bool changedText = false;
                const auto text = Automation::AssOverrides::processText(line.text, [&](const QString &tag, Automation::AssOverrides::Parameter &parameter) {
                    if (tag == "\\r" && parameter.value.compare(source, Qt::CaseInsensitive) == 0) { parameter.value = destination; changedText = true; }
                });
                if (changedText) { line.text = text; line.updateCps(); changedLines = true; }
            }
        }
        catch (const std::exception &error) { return {{"success", false}, {"message", QString::fromUtf8(error.what())}}; }
    }
    auto styles = m_styles;
    styles[index] = style;
    pushUndo(tr("edit style"), selectedIndex, selectedIndices);
    // Install both buffers before notifying views, so observers never see a
    // renamed style while its dialogue references still use the old name.
    if (changedLines) {
        beginResetModel();
        m_lines = std::move(lines);
        m_styles = styles;
        endResetModel();
    }
    else m_styles = styles;
    emit stylesChanged();
    emit contentModified();
    return {{"success", true}, {"changed", true}};
}

void SubtitleModel::setStyleByName(const QString &name, const QVariantMap &style) {
    for (int i = 0; i < m_styles.size(); ++i) {
        if (m_styles[i].toMap().value(QStringLiteral("name")).toString().compare(name, Qt::CaseInsensitive) == 0) {
            setStyle(i, style);
            return;
        }
    }
    addStyle(style);
}

void SubtitleModel::addStyle(const QVariantMap &style) {
    m_styles.append(style);
    emit stylesChanged();
    emit contentModified();
}

void SubtitleModel::removeStyle(int index) {
    if (index >= 0 && index < m_styles.size() && m_styles.size() > 1) {
        m_styles.removeAt(index);
        emit stylesChanged();
        emit contentModified();
    }
}

void SubtitleModel::moveStyle(int from, int to) {
    if (from >= 0 && from < m_styles.size() && to >= 0 && to < m_styles.size() && from != to) {
        m_styles.move(from, to);
        emit stylesChanged();
        emit contentModified();
    }
}

void SubtitleModel::copyStyle(int index) {
    if (index >= 0 && index < m_styles.size()) {
        QVariantMap copy = m_styles[index].toMap();
        copy[QStringLiteral("name")] = copy.value(QStringLiteral("name")).toString() + QStringLiteral(" (copy)");
        m_styles.append(copy);
        emit stylesChanged();
        emit contentModified();
    }
}

void SubtitleModel::sortStyles() {
    auto comp = [](const QVariant &a, const QVariant &b) {
        QString na = a.toMap().value(QStringLiteral("name")).toString();
        QString nb = b.toMap().value(QStringLiteral("name")).toString();
        return na.compare(nb, Qt::CaseInsensitive) < 0;
    };
    std::stable_sort(m_styles.begin(), m_styles.end(), comp);
    emit stylesChanged();
    emit contentModified();
}

void SubtitleModel::setFileName(const QString &name) {
    if (m_fileName != name) {
        m_fileName = name;
        emit fileNameChanged();
    }
}

SubtitleLine SubtitleModel::parseDialogueLine(const QString &rawLine, bool isComment, int lineNumber) const {
    int colonIdx = rawLine.indexOf(QLatin1Char(':'));
    QString content = (colonIdx != -1) ? rawLine.mid(colonIdx + 1) : rawLine;
    if (content.startsWith(QLatin1Char(' '))) content.remove(0, 1);

    SubtitleLine line;
    line.lineNumber = lineNumber;
    line.isComment = isComment;

    int start = 0;
    int partIdx = 0;
    while (partIdx < 9) {
        int commaIdx = content.indexOf(QLatin1Char(','), start);
        if (commaIdx == -1) break;
        QString part = content.mid(start, commaIdx - start).trimmed();
        switch (partIdx) {
        case 0: line.layer = part.toInt(); break;
        case 1: line.setStartStr(part); break;
        case 2: line.setEndStr(part); break;
        case 3: line.style = part.isEmpty() ? QStringLiteral("Default") : part; break;
        case 4: line.actor = part; break;
        case 5: line.marginLeft = part.toInt(); break;
        case 6: line.marginRight = part.toInt(); break;
        case 7: line.marginVert = part.toInt(); break;
        case 8: line.effect = part; break;
        }
        start = commaIdx + 1;
        partIdx++;
    }

    if (partIdx >= 9 && start <= content.length()) {
        line.text = content.mid(start);
    } else {
        line.text = content;
    }
    line.updateCps();
    return line;
}

QString SubtitleModel::formatDialogueLine(const SubtitleLine &line) const {
    QString out;
    out.reserve(line.text.length() + 80);
    out += line.isComment ? QStringLiteral("Comment: ") : QStringLiteral("Dialogue: ");
    out += QString::number(line.layer);
    out += QLatin1Char(',');
    out += line.startStr;
    out += QLatin1Char(',');
    out += line.endStr;
    out += QLatin1Char(',');
    out += line.style.isEmpty() ? QStringLiteral("Default") : line.style;
    out += QLatin1Char(',');
    out += line.actor;
    out += QLatin1Char(',');
    out += QStringLiteral("%1").arg(line.marginLeft, 4, 10, QLatin1Char('0'));
    out += QLatin1Char(',');
    out += QStringLiteral("%1").arg(line.marginRight, 4, 10, QLatin1Char('0'));
    out += QLatin1Char(',');
    out += QStringLiteral("%1").arg(line.marginVert, 4, 10, QLatin1Char('0'));
    out += QLatin1Char(',');
    out += line.effect;
    out += QLatin1Char(',');
    out += line.text;
    return out;
}

QString SubtitleModel::serializeClipboardLines(const QVariantList &selectedIndices) const {
    QSet<int> unique;
    for (const auto &value : selectedIndices) {
        bool ok = false;
        const int row = value.toInt(&ok);
        if (ok && row >= 0 && row < static_cast<int>(m_lines.size())) unique.insert(row);
    }
    QList<int> rows = unique.values();
    std::sort(rows.begin(), rows.end());
    QStringList output;
    for (int row : rows) {
        auto line = m_lines[row];
        line.style.replace(',', ';');
        line.actor.replace(',', ';');
        line.effect.replace(',', ';');
        line.text.replace(QStringLiteral("\r\n"), QStringLiteral("\\N"));
        line.text.replace('\r', QStringLiteral("\\N"));
        line.text.replace('\n', QStringLiteral("\\N"));
        output.append(formatDialogueLine(line));
    }
    return output.join(QStringLiteral("\r\n"));
}

QVariantList SubtitleModel::parseClipboardLines(const QString &text) const {
    QVariantList output;
    static const QRegularExpression prefix(QStringLiteral("^\\s*(Dialogue|Comment): ?"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression time(QStringLiteral("^\\d+:[0-5]\\d:[0-5]\\d\\.\\d{1,3}$"));
    for (const auto &raw : text.split(QRegularExpression(QStringLiteral("\r\n|\n|\r")))) {
        if (raw.isEmpty()) continue;
        const auto match = prefix.match(raw);
        const QString payload = raw.mid(match.capturedEnd());
        const auto parts = payload.split(',');
        bool valid = match.hasMatch() && parts.size() >= 10;
        if (valid) {
            bool ok = false;
            parts[0].trimmed().toInt(&ok);
            valid = ok && time.match(parts[1].trimmed()).hasMatch() && time.match(parts[2].trimmed()).hasMatch();
            for (int margin = 5; valid && margin <= 7; ++margin) {
                parts[margin].trimmed().toInt(&ok);
                valid = ok;
            }
        }
        if (valid) {
            output.append(parseDialogueLine(raw.mid(match.capturedStart()),
                match.captured(1).compare(QStringLiteral("Comment"), Qt::CaseInsensitive) == 0,
                output.size() + 1).toMap());
        } else {
            SubtitleLine line;
            line.setStartMs(0);
            line.setEndMs(0);
            line.text = raw;
            line.updateCps();
            output.append(line.toMap());
        }
    }
    return output;
}

bool SubtitleModel::loadFromFile(const QString &filePath) {
    return loadFromFileWithCharset(filePath, QString());
}

bool SubtitleModel::loadFromFileWithCharset(const QString &filePath, const QString &charset) {
    QString cleanPath = filePath;
    if (cleanPath.startsWith(QStringLiteral("file:"))) {
        const QUrl url(cleanPath);
        if (url.isLocalFile()) cleanPath = url.toLocalFile();
    }
    QFile file(cleanPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QByteArray rawData = file.readAll();
    file.close();

    QString encodingName = charset.trimmed();
    const bool utf8Bom = rawData.startsWith("\xEF\xBB\xBF");
    const bool utf16LeBom = rawData.startsWith("\xFF\xFE");
    const bool utf16BeBom = rawData.startsWith("\xFE\xFF");
    if (encodingName.isEmpty()) {
        if (utf16LeBom) encodingName = QStringLiteral("UTF-16LE");
        else if (utf16BeBom) encodingName = QStringLiteral("UTF-16BE");
        else encodingName = QStringLiteral("UTF-8");
    }
    QTextCodec *codec = QTextCodec::codecForName(encodingName.toUtf8());
    if (!codec) return false;
    QTextDecoder decoder(codec);
    QString content = decoder.toUnicode(rawData);
    if (decoder.hasFailure()) return false;
    const bool encodingBom = utf8Bom || utf16LeBom || utf16BeBom;
    if (!content.isEmpty() && content.at(0) == QChar(0xFEFF)) {
        content.remove(0, 1);
    }

    QStringList lines = content.split(QRegularExpression(QStringLiteral("\r\n|\n|\r")));
    const QString suffix = QFileInfo(cleanPath).suffix().toLower();
    if (suffix == QStringLiteral("srt")) {
        static const QRegularExpression timing(
            QStringLiteral(R"(^\s*(\d{1,3}):([0-5]\d):([0-5]\d)[,.](\d{1,3})\s*-->\s*(\d{1,3}):([0-5]\d):([0-5]\d)[,.](\d{1,3})\s*$)"));
        auto milliseconds = [](const QRegularExpressionMatch &match, int first) -> int {
            const qint64 hours = match.captured(first).toLongLong();
            const qint64 minutes = match.captured(first + 1).toLongLong();
            const qint64 seconds = match.captured(first + 2).toLongLong();
            const QString fraction = match.captured(first + 3).leftJustified(3, QLatin1Char('0'));
            const qint64 total = ((hours * 60 + minutes) * 60 + seconds) * 1000 + fraction.toInt();
            return total <= INT_MAX ? static_cast<int>(total) : -1;
        };
        std::vector<SubtitleLine> srtLines;
        for (qsizetype index = 0; index < lines.size();) {
            while (index < lines.size() && lines[index].trimmed().isEmpty()) ++index;
            if (index == lines.size()) break;
            if (QRegularExpression(QStringLiteral(R"(^\d+$)")).match(lines[index].trimmed()).hasMatch()) ++index;
            if (index == lines.size()) return false;
            const auto match = timing.match(lines[index++]);
            if (!match.hasMatch()) return false;
            const int start = milliseconds(match, 1);
            const int end = milliseconds(match, 5);
            if (start < 0 || end < start) return false;
            QStringList body;
            while (index < lines.size() && !lines[index].trimmed().isEmpty()) {
                body.append(lines[index++].replace(QStringLiteral("\\"), QStringLiteral("\\\\")));
            }
            SubtitleLine line;
            line.lineNumber = static_cast<int>(srtLines.size()) + 1;
            line.setStartMs(start);
            line.setEndMs(end);
            line.text = SrtImport::toAss(body.join(QStringLiteral("\\N")));
            line.updateCps();
            srtLines.push_back(std::move(line));
        }
        if (srtLines.empty()) return false;
        beginResetModel();
        m_lines = std::move(srtLines);
        m_scriptInfo = defaultScriptInfo();
        m_styles = defaultStyles();
        m_rawSections.clear();
        m_fileName = cleanPath;
        m_encodingName = encodingName;
        m_encodingBom = encodingBom;
        m_undoStack.clear();
        m_redoStack.clear();
        endResetModel();
        emit scriptInfoChanged();
        emit stylesChanged();
        emit attachmentsChanged();
        emit fileNameChanged();
        emit countChanged();
        emit undoStateChanged();
        emit contentModified();
        resetModificationTracking();
        return true;
    }
    if (suffix == QStringLiteral("sub") || suffix == QStringLiteral("vtt")) return false;

    QString currentSection;
    bool recognizedAssSection = false;
    QVariantMap newScriptInfo = defaultScriptInfo();
    QVariantList newStyles;
    std::vector<SubtitleLine> newLines;
    QList<AssRawSection> newRawSections;

    QMap<quint32, QPair<QByteArray, QByteArray>> extraEntries;
    std::vector<QList<quint32>> extraReferences;
    static const QRegularExpression extraRecord(QStringLiteral(R"(^Data:\s*(\d+),([^,]*),([eu])(.*)$)"));
    static const QRegularExpression extraPrefix(QStringLiteral(R"(^\{(?:=\d+)+\})"));
    static const QRegularExpression extraId(QStringLiteral(R"(=(\d+))"));
    int dialogueCount = 0;
    for (const QString &rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            currentSection = line;
            if (currentSection == QStringLiteral("[Script Info]") || currentSection == QStringLiteral("[Events]"))
                recognizedAssSection = true;
            if (currentSection != QStringLiteral("[Script Info]") &&
                currentSection != QStringLiteral("[V4+ Styles]") &&
                currentSection != QStringLiteral("[V4 Styles]") &&
                currentSection != QStringLiteral("[Events]") &&
                currentSection.compare(QStringLiteral("[Aegisub Extradata]"), Qt::CaseInsensitive) != 0) {
                newRawSections.append({ currentSection, {} });
            }
            continue;
        }

        if (currentSection == QStringLiteral("[Script Info]")) {
            if (line.startsWith(QLatin1Char(';'))) continue;
            int colonIdx = line.indexOf(QLatin1Char(':'));
            if (colonIdx > 0) {
                QString k = line.left(colonIdx).trimmed();
                QString v = line.mid(colonIdx + 1).trimmed();
                newScriptInfo[k] = v;
            }
        } else if (currentSection == QStringLiteral("[V4+ Styles]") || currentSection == QStringLiteral("[V4 Styles]")) {
            if (line.startsWith(QStringLiteral("Style:"))) {
                QString styleData = line.mid(6).trimmed();
                QStringList sParts = styleData.split(QLatin1Char(','));
                const bool ssaStyle = currentSection == QStringLiteral("[V4 Styles]");
                if (sParts.size() == (ssaStyle ? 18 : 23)) {
                    QVariantMap st;
                    st[QStringLiteral("name")] = sParts[0].trimmed();
                    st[QStringLiteral("font")] = sParts[1].trimmed();
                    st[QStringLiteral("size")] = sParts[2].trimmed().toDouble();
                    if (ssaStyle) {
                        auto ssaColor = [&sParts](int field) {
                            return QString::fromStdString(agi::Color(sParts[field].trimmed().toStdString()).GetAssStyleFormatted());
                        };
                        st[QStringLiteral("primary")] = ssaColor(3);
                        st[QStringLiteral("secondary")] = ssaColor(4);
                        st[QStringLiteral("outline")] = ssaColor(6);
                        st[QStringLiteral("shadow")] = ssaColor(6);
                        st[QStringLiteral("bold")] = sParts[7].trimmed().toInt() != 0;
                        st[QStringLiteral("italic")] = sParts[8].trimmed().toInt() != 0;
                        st[QStringLiteral("underline")] = false;
                        st[QStringLiteral("strikeout")] = false;
                        st[QStringLiteral("scaleX")] = 100.0;
                        st[QStringLiteral("scaleY")] = 100.0;
                        st[QStringLiteral("spacing")] = 0.0;
                        st[QStringLiteral("angle")] = 0.0;
                        st[QStringLiteral("borderStyle")] = sParts[9].trimmed().toInt();
                        st[QStringLiteral("outlineWidth")] = sParts[10].trimmed().toDouble();
                        st[QStringLiteral("shadowDepth")] = sParts[11].trimmed().toDouble();
                        static constexpr int ssaAlignments[] = {2, 1, 2, 3, 2, 7, 8, 9, 2, 4, 5, 6};
                        const int align = sParts[12].trimmed().toInt();
                        st[QStringLiteral("alignment")] = align >= 0 && align < 12 ? ssaAlignments[align] : 2;
                        st[QStringLiteral("marginL")] = sParts[13].trimmed().toInt();
                        st[QStringLiteral("marginR")] = sParts[14].trimmed().toInt();
                        st[QStringLiteral("marginV")] = sParts[15].trimmed().toInt();
                        st[QStringLiteral("encoding")] = sParts[17].trimmed().toInt();
                    } else {
                        st[QStringLiteral("primary")] = sParts[3].trimmed();
                        st[QStringLiteral("secondary")] = sParts[4].trimmed();
                        st[QStringLiteral("outline")] = sParts[5].trimmed();
                        st[QStringLiteral("shadow")] = sParts[6].trimmed();
                        st[QStringLiteral("bold")] = sParts[7].trimmed().toInt() != 0;
                        st[QStringLiteral("italic")] = sParts[8].trimmed().toInt() != 0;
                        st[QStringLiteral("underline")] = sParts[9].trimmed().toInt() != 0;
                        st[QStringLiteral("strikeout")] = sParts[10].trimmed().toInt() != 0;
                        st[QStringLiteral("scaleX")] = sParts[11].trimmed().toDouble();
                        st[QStringLiteral("scaleY")] = sParts[12].trimmed().toDouble();
                        st[QStringLiteral("spacing")] = sParts[13].trimmed().toDouble();
                        st[QStringLiteral("angle")] = sParts[14].trimmed().toDouble();
                        st[QStringLiteral("borderStyle")] = sParts[15].trimmed().toInt();
                        st[QStringLiteral("outlineWidth")] = sParts[16].trimmed().toDouble();
                        st[QStringLiteral("shadowDepth")] = sParts[17].trimmed().toDouble();
                        st[QStringLiteral("alignment")] = sParts[18].trimmed().toInt();
                        st[QStringLiteral("marginL")] = sParts[19].trimmed().toInt();
                        st[QStringLiteral("marginR")] = sParts[20].trimmed().toInt();
                        st[QStringLiteral("marginV")] = sParts[21].trimmed().toInt();
                        st[QStringLiteral("encoding")] = sParts[22].trimmed().toInt();
                    }
                    newStyles.append(st);
                } else {
                    return false;
                }
            }
        } else if (currentSection == QStringLiteral("[Events]")) {
            bool isDialogue = line.startsWith(QStringLiteral("Dialogue:"));
            bool isComment = line.startsWith(QStringLiteral("Comment:"));
            if (isDialogue || isComment) {
                dialogueCount++;
                SubtitleLine parsed = parseDialogueLine(rawLine, isComment, dialogueCount);
                QList<quint32> references;
                const auto prefix = extraPrefix.match(parsed.text);
                if (prefix.hasMatch()) {
                    auto ids = extraId.globalMatch(prefix.captured());
                    while (ids.hasNext()) {
                        bool ok = false;
                        const auto id = ids.next().captured(1).toUInt(&ok);
                        if (ok) references.append(id);
                    }
                    parsed.text.remove(0, prefix.capturedLength());
                    parsed.updateCps();
                }
                extraReferences.push_back(references);
                newLines.push_back(std::move(parsed));
            }
        } else if (currentSection.compare(QStringLiteral("[Aegisub Extradata]"), Qt::CaseInsensitive) == 0) {
            const auto record = extraRecord.match(rawLine);
            if (!record.hasMatch()) continue;
            bool ok = false;
            const auto id = record.captured(1).toUInt(&ok);
            if (!ok) continue;
            auto decode = [](const QString &value) { return QByteArray::fromStdString(agi::ass::inline_string_decode(value.toUtf8().toStdString())); };
            const auto key = decode(record.captured(2));
            QByteArray value;
            if (record.captured(3) == "e") value = decode(record.captured(4));
            else {
                const auto encoded = record.captured(4).toLatin1();
                const auto bytes = agi::ass::UUDecode(encoded.constData(), encoded.constData() + encoded.size());
                value = QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size()));
            }
            extraEntries[id] = qMakePair(key, value);
        } else if (!newRawSections.isEmpty()) {
            newRawSections.last().lines.append(rawLine);
        }
    }

    if (!recognizedAssSection) return false;
    // Extradata usually follows [Events]; resolve references after the whole file.
    for (size_t row = 0; row < extraReferences.size(); ++row) {
        for (auto id : extraReferences[row]) {
            if (!extraEntries.contains(id)) continue;
            const auto entry = extraEntries.value(id);
            newLines[row].extra[entry.first] = entry.second;
        }
    }

    if (newStyles.isEmpty()) {
        newStyles = defaultStyles();
    }
    if (newLines.empty()) {
        SubtitleLine def;
        def.lineNumber = 1;
        def.setStartMs(0);
        def.setEndMs(5000);
        newLines.push_back(def);
    }

    beginResetModel();
    m_lines = std::move(newLines);
    m_scriptInfo = newScriptInfo;
    m_styles = newStyles;
    m_rawSections = std::move(newRawSections);
    m_fileName = cleanPath;
    m_encodingName = encodingName;
    m_encodingBom = encodingBom;
    m_undoStack.clear();
    m_redoStack.clear();
    endResetModel();

    emit scriptInfoChanged();
    emit stylesChanged();
    emit attachmentsChanged();
    emit fileNameChanged();
    emit countChanged();
    emit undoStateChanged();
    emit contentModified();

    // Newly loaded document is synchronized with disk state and marked unmodified.
    resetModificationTracking();

    return true;
}

QString SubtitleModel::previewAss() const {
    QString result;
    serializeDocument(QStringLiteral("preview.ass"), &result);
    return result;
}

void SubtitleModel::initializeVideoResolution(int width, int height) {
    if (width <= 0 || height <= 0 || m_scriptInfo.value("PlayResX").toInt() || m_scriptInfo.value("PlayResY").toInt()) return;
    m_scriptInfo[QStringLiteral("PlayResX")] = width;
    m_scriptInfo[QStringLiteral("PlayResY")] = height;
    emit scriptInfoChanged();
    emit contentModified();
}

bool SubtitleModel::serializeDocument(const QString &target, QString *preview) const {
    QString document;
    QTextStream out(&document, QIODevice::WriteOnly);
    auto writeDocument = [&]() {
        out.flush();
        if (out.status() != QTextStream::Ok) return false;
        if (preview) { *preview = document; return true; }
        QTextCodec *codec = QTextCodec::codecForName(m_encodingName.toUtf8());
        if (!codec || !codec->canEncode(document)) return false;
        QByteArray encoded = codec->fromUnicode(document);
        if (m_encodingBom) {
            if (m_encodingName.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0 &&
                !encoded.startsWith("\xEF\xBB\xBF")) encoded.prepend("\xEF\xBB\xBF");
            else if (m_encodingName.compare(QStringLiteral("UTF-16LE"), Qt::CaseInsensitive) == 0 &&
                     !encoded.startsWith("\xFF\xFE")) encoded.prepend("\xFF\xFE");
            else if (m_encodingName.compare(QStringLiteral("UTF-16BE"), Qt::CaseInsensitive) == 0 &&
                     !encoded.startsWith("\xFE\xFF")) encoded.prepend("\xFE\xFF");
        }
        QSaveFile file(target);
        if (!file.open(QIODevice::WriteOnly)) return false;
        if (file.write(encoded) != encoded.size()) return false;
        return file.commit();
    };

    if (QFileInfo(target).suffix().compare(QStringLiteral("srt"), Qt::CaseInsensitive) == 0) {
        const QString newline =
#ifdef Q_OS_WIN
            QStringLiteral("\r\n");
#else
            QStringLiteral("\n");
#endif
        int index = 0;
        for (const auto &cue : SrtExport::convert(m_lines)) {
            out << ++index << newline
                << AegisubCoreBridge::formatSrtTime(cue.start) << " --> "
                << AegisubCoreBridge::formatSrtTime(cue.end) << newline
                << SrtExport::convertTags(cue.text, newline) << newline << newline;
        }
        return writeDocument();
    }

    out << "[Script Info]\n";
    out << "; Script generated by Aegisub Qt Quick\n";
    out << "; http://www.aegisub.org/\n";

    for (auto it = m_scriptInfo.begin(); it != m_scriptInfo.end(); ++it) {
        out << it.key() << ": " << it.value().toString() << "\n";
    }
    out << "\n";

    for (const auto &sec : m_rawSections) {
        if (sec.header == QStringLiteral("[Aegisub Project Garbage]")) {
            out << sec.header << "\n";
            for (const auto &l : sec.lines) out << l << "\n";
            out << "\n";
        }
    }

    out << "[V4+ Styles]\n";
    out << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n";
    for (const auto &v : m_styles) {
        QVariantMap s = v.toMap();
        out << "Style: "
            << s.value(QStringLiteral("name"), QStringLiteral("Default")).toString() << ","
            << s.value(QStringLiteral("font"), QStringLiteral("Arial")).toString() << ","
            << s.value(QStringLiteral("size"), 20.0).toDouble() << ","
            << s.value(QStringLiteral("primary"), QStringLiteral("&H00FFFFFF")).toString() << ","
            << s.value(QStringLiteral("secondary"), QStringLiteral("&H000000FF")).toString() << ","
            << s.value(QStringLiteral("outline"), QStringLiteral("&H00000000")).toString() << ","
            << s.value(QStringLiteral("shadow"), QStringLiteral("&H00000000")).toString() << ","
            << (s.value(QStringLiteral("bold")).toBool() ? -1 : 0) << ","
            << (s.value(QStringLiteral("italic")).toBool() ? -1 : 0) << ","
            << (s.value(QStringLiteral("underline")).toBool() ? -1 : 0) << ","
            << (s.value(QStringLiteral("strikeout")).toBool() ? -1 : 0) << ","
            << s.value(QStringLiteral("scaleX"), 100.0).toDouble() << ","
            << s.value(QStringLiteral("scaleY"), 100.0).toDouble() << ","
            << s.value(QStringLiteral("spacing"), 0.0).toDouble() << ","
            << s.value(QStringLiteral("angle"), 0.0).toDouble() << ","
            << s.value(QStringLiteral("borderStyle"), 1).toInt() << ","
            << s.value(QStringLiteral("outlineWidth"), 2.0).toDouble() << ","
            << s.value(QStringLiteral("shadowDepth"), 2.0).toDouble() << ","
            << s.value(QStringLiteral("alignment"), 2).toInt() << ","
            << s.value(QStringLiteral("marginL"), 10).toInt() << ","
            << s.value(QStringLiteral("marginR"), 10).toInt() << ","
            << s.value(QStringLiteral("marginV"), 10).toInt() << ","
            << s.value(QStringLiteral("encoding"), 1).toInt() << "\n";
    }
    out << "\n";

    out << "[Events]\n";
    out << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
    QMap<QPair<QByteArray, QByteArray>, int> extraIds;
    QList<QPair<QByteArray, QByteArray>> extraEntries;
    for (const auto &line : m_lines) {
        QString prefix;
        if (!line.extra.isEmpty()) {
            prefix = "{";
            QList<int> ids;
            for (auto it = line.extra.cbegin(); it != line.extra.cend(); ++it) {
                const auto entry = qMakePair(it.key(), it.value());
                if (!extraIds.contains(entry)) {
                    extraIds[entry] = static_cast<int>(extraEntries.size());
                    extraEntries.append(entry);
                }
                ids.append(extraIds.value(entry));
            }
            std::sort(ids.begin(), ids.end());
            for (int id : ids) prefix += "=" + QString::number(id);
            prefix += "}";
        }
        auto serialized = line;
        serialized.text.prepend(prefix);
        out << formatDialogueLine(serialized) << "\n";
    }
    if (!extraEntries.isEmpty()) {
        out << "\n[Aegisub Extradata]\n";
        for (int id = 0; id < extraEntries.size(); ++id) {
            const auto &entry = extraEntries[id];
            const auto escaped = assExtraEncode(entry.second);
            const auto uu = agi::ass::UUEncode(entry.second.constData(), entry.second.constData() + entry.second.size(), false);
            const bool useUu = uu.size() < static_cast<size_t>(escaped.size());
            out << "Data: " << id << "," << assExtraEncode(entry.first) << ","
                << (useUu ? "u" : "e") << (useUu ? QString::fromLatin1(uu.data(), static_cast<qsizetype>(uu.size())) : escaped) << "\n";
        }
    }

    for (const auto &sec : m_rawSections) {
        if (sec.header != QStringLiteral("[Aegisub Project Garbage]")) {
            out << "\n" << sec.header << "\n";
            for (const auto &l : sec.lines) out << l << "\n";
        }
    }

    return writeDocument();
}

bool SubtitleModel::exportToFile(const QString &filePath, const QString &charset,
                                  const SubtitleModel &content) const {
    QString target = filePath;
    if (target.startsWith(QStringLiteral("file:"))) {
        const QUrl url(target);
        if (!url.isLocalFile()) return false;
        target = url.toLocalFile();
    }
    const auto extension = QFileInfo(target).suffix().toLower();
    if (target.isEmpty() || (extension != "ass" && extension != "srt")) return false;
    auto *codec = QTextCodec::codecForName(charset.toUtf8());
    if (!codec) return false;
    SubtitleModel snapshot;
    snapshot.m_lines = content.m_lines;
    snapshot.m_scriptInfo = content.m_scriptInfo;
    snapshot.m_styles = content.m_styles;
    // Project paths, export preferences and UI state belong to the editor save,
    // not the exported subtitle file (upstream AssSubtitleFormat::ExportFile).
    for (const auto &section : m_rawSections)
        if (section.header.compare(QStringLiteral("[Aegisub Project Garbage]"), Qt::CaseInsensitive) != 0)
            snapshot.m_rawSections.append(section);
    snapshot.m_encodingName = QString::fromLatin1(codec->name());
    snapshot.m_encodingBom = snapshot.m_encodingName.startsWith("UTF-", Qt::CaseInsensitive);
    return snapshot.serializeDocument(target);
}

bool SubtitleModel::saveToFile(const QString &filePath) {
    QString target = filePath.isEmpty() ? m_fileName : filePath;
    if (target.isEmpty() || target == QStringLiteral("Untitled")) return false;
    if (target.startsWith(QStringLiteral("file:"))) {
        const QUrl url(target);
        if (url.isLocalFile()) target = url.toLocalFile();
    }
    if (!serializeDocument(target)) return false;

    m_fileName = target;
    emit fileNameChanged();
    // File written to disk: align savedCommitId with commitId to clear modified state.
    markSaved();
    return true;
}

bool SubtitleModel::saveBackup(bool autosaveKind) {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Aegisub"), QStringLiteral("Aegisub"));
    const QString key = autosaveKind ? QStringLiteral("Autosave/Path") : QStringLiteral("Backup/Path");
    const QString fallback = autosaveKind ? QStringLiteral("?user/autosave") : QStringLiteral("?user/autobackup");
    QString dirPath = AegisubCoreBridge::resolveUserPath(settings.value(key, fallback).toString());
    if (!QDir().mkpath(dirPath)) return false;

    QString stem = QStringLiteral("Untitled");
    if (!m_fileName.isEmpty() && m_fileName != QStringLiteral("Untitled")) {
        stem = QFileInfo(m_fileName).completeBaseName();
    }
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
    const QString suffix = autosaveKind ? QStringLiteral("AUTOSAVE") : QStringLiteral("BACKUP");
    const QString target = QDir(dirPath).filePath(QStringLiteral("%1.%2.%3.ass").arg(stem, stamp, suffix));
    return serializeDocument(target);
}

std::vector<SubtitleModel::AttachmentLocation> SubtitleModel::attachmentLocations() const {
    std::vector<AttachmentLocation> result;
    for (int sectionIndex = 0; sectionIndex < m_rawSections.size(); ++sectionIndex) {
        const auto &section = m_rawSections[sectionIndex];
        const bool isFont = section.header.compare(QStringLiteral("[Fonts]"), Qt::CaseInsensitive) == 0;
        const bool isGraphic = section.header.compare(QStringLiteral("[Graphics]"), Qt::CaseInsensitive) == 0;
        if (!isFont && !isGraphic) continue;
        const QString prefix = isFont ? QStringLiteral("fontname: ") : QStringLiteral("filename: ");
        for (int first = 0; first < section.lines.size();) {
            if (!section.lines[first].startsWith(prefix, Qt::CaseInsensitive)) {
                ++first;
                continue;
            }
            int end = first + 1;
            QByteArray encoded;
            while (end < section.lines.size() && !section.lines[end].startsWith(prefix, Qt::CaseInsensitive)) {
                encoded += section.lines[end].toLatin1();
                encoded += '\n';
                ++end;
            }
            const QString rawName = section.lines[first].mid(prefix.size()).trimmed();
            QString displayName = QFileInfo(rawName).fileName();
            if (isFont && displayName.endsWith(QStringLiteral(".ttf"), Qt::CaseInsensitive)) {
                const int underscore = displayName.lastIndexOf(QLatin1Char('_'));
                if (underscore >= 0 &&
                    QRegularExpression(QStringLiteral(R"(^\d+$)"))
                        .match(displayName.mid(underscore + 1, displayName.size() - underscore - 5)).hasMatch())
                    displayName = displayName.left(underscore) + QStringLiteral(".ttf");
            }
            result.push_back({sectionIndex, first, end, rawName, displayName,
                              section.header, std::move(encoded)});
            first = end;
        }
    }
    return result;
}

QVariantList SubtitleModel::attachments() const {
    QVariantList result;
    for (const auto &attachment : attachmentLocations()) {
        const auto decoded = agi::ass::UUDecode(attachment.encoded.constData(),
                                                attachment.encoded.constData() + attachment.encoded.size());
        result.append(QVariantMap{
            {QStringLiteral("filename"), attachment.displayName},
            {QStringLiteral("sizeStr"), QStringLiteral("%1 B").arg(decoded.size())},
            {QStringLiteral("typeStr"), attachment.type}
        });
    }
    return result;
}

bool SubtitleModel::addAttachmentFile(const QString &path, bool font) {
    QString localPath = path;
    if (localPath.startsWith(QStringLiteral("file:"))) {
        const QUrl url(localPath);
        if (!url.isLocalFile()) return false;
        localPath = url.toLocalFile();
    }
    QFile input(localPath);
    if (!input.open(QIODevice::ReadOnly)) return false;
    const QByteArray bytes = input.readAll();
    if (input.error() != QFileDevice::NoError) return false;
    QString name = QFileInfo(localPath).fileName();
    if (name.isEmpty()) return false;
    if (font && name.endsWith(QStringLiteral(".ttf"), Qt::CaseInsensitive))
        name.insert(name.size() - 4, QStringLiteral("_0"));
    const std::string encoded = agi::ass::UUEncode(bytes.constData(), bytes.constData() + bytes.size());
    const QString header = font ? QStringLiteral("[Fonts]") : QStringLiteral("[Graphics]");
    int sectionIndex = -1;
    for (int index = 0; index < m_rawSections.size(); ++index) {
        if (m_rawSections[index].header.compare(header, Qt::CaseInsensitive) == 0) {
            sectionIndex = index;
            break;
        }
    }
    pushUndo(font ? QStringLiteral("attach font") : QStringLiteral("attach graphic"));
    if (sectionIndex < 0) {
        m_rawSections.append({header, {}});
        sectionIndex = m_rawSections.size() - 1;
    }
    auto &section = m_rawSections[sectionIndex];
    section.lines.append((font ? QStringLiteral("fontname: ") : QStringLiteral("filename: ")) + name);
    if (!encoded.empty()) {
        const QStringList dataLines = QString::fromLatin1(encoded.data(), encoded.size()).split(QStringLiteral("\r\n"));
        section.lines.append(dataLines);
    }
    emit attachmentsChanged();
    emit contentModified();
    return true;
}

bool SubtitleModel::removeAttachment(int index) {
    const auto locations = attachmentLocations();
    if (index < 0 || index >= static_cast<int>(locations.size())) return false;
    const auto &location = locations[index];
    pushUndo(QStringLiteral("remove attachment"));
    auto &lines = m_rawSections[location.sectionIndex].lines;
    for (int line = location.endLine - 1; line >= location.firstLine; --line) lines.removeAt(line);
    emit attachmentsChanged();
    emit contentModified();
    return true;
}

bool SubtitleModel::extractAttachment(int index, const QString &folder) const {
    const auto locations = attachmentLocations();
    if (index < 0 || index >= static_cast<int>(locations.size())) return false;
    const auto &attachment = locations[index];
    if (attachment.displayName.isEmpty() || attachment.displayName == QStringLiteral(".") ||
        attachment.displayName == QStringLiteral("..")) return false;
    QString localFolder = folder;
    if (localFolder.startsWith(QStringLiteral("file:"))) {
        const QUrl url(localFolder);
        if (!url.isLocalFile()) return false;
        localFolder = url.toLocalFile();
    }
    const QDir directory(localFolder);
    if (!directory.exists()) return false;
    const QString target = directory.filePath(attachment.displayName);
    if (QFileInfo::exists(target)) return false;
    const auto bytes = agi::ass::UUDecode(attachment.encoded.constData(),
                                         attachment.encoded.constData() + attachment.encoded.size());
    QSaveFile output(target);
    if (!output.open(QIODevice::WriteOnly)) return false;
    if (output.write(bytes.data(), static_cast<qint64>(bytes.size())) != static_cast<qint64>(bytes.size())) return false;
    return output.commit();
}

QVariantMap SubtitleModel::splitSelectedByKaraoke(const QVariantList &selectedIndices, int activeIndex) {
    QList<int> selected;
    for (const auto &value : selectedIndices) {
        bool ok;
        const double number = value.toDouble(&ok);
        if (ok && std::isfinite(number) && std::trunc(number) == number && number >= 0 && number < m_lines.size())
            selected.append(static_cast<int>(number));
    }
    std::sort(selected.begin(), selected.end());
    selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
    if (selected.isEmpty()) return {{"success", true}, {"changed", false}};
    std::vector<SubtitleLine> output;
    QVariantList newSelection;
    bool changed = false;
    int newActive = -1;
    for (int index = 0; index < static_cast<int>(m_lines.size()); ++index) {
        const auto &original = m_lines[index];
        if (std::binary_search(selected.begin(), selected.end(), index)) {
            const auto syllables = AegisubCoreBridge::parseKaraokeLine(original.text, original.startMs, original.endMs, false);
            if (syllables.isEmpty()) return {{"success", false}, {"message", tr("Invalid karaoke timing")}};
            if (syllables.size() >= 2) {
                changed = true;
                for (const auto &syllable : syllables) {
                    auto piece = original;
                    piece.setStartMs(syllable.value("startTime").toInt());
                    piece.setEndMs(syllable.value("endTime").toInt());
                    piece.text = syllable.value("textWithTags").toString();
                    piece.updateCps();
                    newSelection.append(static_cast<int>(output.size()));
                    output.push_back(std::move(piece));
                }
                continue;
            }
        }
        if (index == activeIndex) newActive = static_cast<int>(output.size());
        output.push_back(original);
    }
    if (!changed) return {{"success", true}, {"changed", false}};
    if (newActive < 0 || !newSelection.contains(newActive)) newActive = newSelection.front().toInt();
    pushUndo(tr("split by karaoke"), activeIndex, selected);
    setRawLines(std::move(output));
    return {{"success", true}, {"changed", true}, {"selectedIndices", newSelection}, {"activeIndex", newActive}};
}

void SubtitleModel::newDocument() {
    beginResetModel();
    m_lines.clear();
    SubtitleLine def;
    def.lineNumber = 1;
    def.setStartMs(0);
    def.setEndMs(5000);
    m_lines.push_back(def);
    m_scriptInfo = defaultScriptInfo();
    m_styles = defaultStyles();
    m_rawSections.clear();
    m_fileName = QStringLiteral("Untitled");
    m_encodingName = QStringLiteral("UTF-8");
    m_encodingBom = true;
    m_undoStack.clear();
    m_redoStack.clear();
    endResetModel();

    emit scriptInfoChanged();
    emit stylesChanged();
    emit attachmentsChanged();
    emit fileNameChanged();
    emit countChanged();
    emit undoStateChanged();
    emit contentModified();

    // Freshly created blank document starts in unmodified state.
    resetModificationTracking();
}

QVariantList SubtitleModel::getAllLines() const {
    QVariantList list;
    list.reserve(static_cast<qsizetype>(m_lines.size()));
    for (const auto &line : m_lines) {
        list.append(line.toMap());
    }
    return list;
}

void SubtitleModel::setRawLines(std::vector<SubtitleLine> lines) {
    beginResetModel();
    m_lines = std::move(lines);
    for (size_t i = 0; i < m_lines.size(); ++i) {
        m_lines[i].lineNumber = static_cast<int>(i + 1);
    }
    endResetModel();
    emit countChanged();
    emit contentModified();
}

QVariantMap SubtitleModel::resampleResolution(const QVariantMap &options, int selectedIndex, const QVariantList &selectedIndices) {
    try {
        auto read = [&](const char *key, int fallback, bool required = false) {
            if (required && !options.contains(key)) throw std::invalid_argument("Missing resampling resolution");
            const auto variant = options.value(key, fallback);
            bool ok;
            const double number = variant.toDouble(&ok);
            if (!ok || variant.metaType().id() == QMetaType::Bool || !std::isfinite(number) || std::trunc(number) != number)
                throw std::invalid_argument("Invalid resampling integer");
            return ResolutionResampler::integer(number);
        };
        ResolutionResampler::Settings settings{read("sourceX", 0, true), read("sourceY", 0, true), read("destX", 0, true), read("destY", 0, true)};
        settings.mode = read("mode", 0);
        const char *keys[] = {"left", "right", "top", "bottom"};
        for (int i = 0; i < 4; ++i) settings.margins[i] = read(keys[i], 0);
        if (options.contains("sourceMatrix") || options.contains("destMatrix")) {
            const agi::ycbcr::Header source(options.value("sourceMatrix").toString().toStdString());
            const agi::ycbcr::Header destination(options.value("destMatrix").toString().toStdString());
            const auto *from = std::get_if<agi::ycbcr::header_colorspace>(&source);
            const auto *to = std::get_if<agi::ycbcr::header_colorspace>(&destination);
            if (!from || !to) throw std::invalid_argument("Invalid conversion color matrix");
            settings.matrix = std::make_pair(*from, *to);
        }
        auto lines = m_lines;
        auto styles = m_styles;
        auto info = m_scriptInfo;
        ResolutionResampler::transform(lines, styles, info, settings);
        if (getAllLines() == [&] { QVariantList result; for (const auto &line : lines) result.append(line.toMap()); return result; }() && styles == m_styles && info == m_scriptInfo)
            return {{"success", true}, {"changed", false}};
        pushUndo(tr("resolution resampling"), selectedIndex, selectedIndices);
        setRawLines(std::move(lines));
        setStyles(styles);
        setScriptInfo(info);
        return {{"success", true}, {"changed", true}};
    }
    catch (const std::exception &error) {
        return {{"success", false}, {"message", QString::fromUtf8(error.what())}};
    }
}

void SubtitleModel::setAllLines(const QVariantList &lines) {
    beginResetModel();
    m_lines.clear();
    m_lines.reserve(static_cast<size_t>(lines.size()));
    for (int i = 0; i < lines.size(); ++i) {
        m_lines.push_back(SubtitleLine::fromMap(lines[i].toMap(), i + 1));
    }
    endResetModel();
    emit countChanged();
    emit contentModified();
}

int SubtitleModel::getLineStartMs(int index) const {
    if (index < 0 || index >= static_cast<int>(m_lines.size())) return 0;
    return m_lines[index].startMs;
}

int SubtitleModel::getLineEndMs(int index) const {
    if (index < 0 || index >= static_cast<int>(m_lines.size())) return 0;
    return m_lines[index].endMs;
}

QList<int> SubtitleModel::getSnapPoints(int excludeIndex) const {
    QList<int> pts;
    pts.reserve(static_cast<qsizetype>(m_lines.size() * 2));
    for (size_t i = 0; i < m_lines.size(); ++i) {
        if (static_cast<int>(i) == excludeIndex) continue;
        const auto &line = m_lines[i];
        if (line.startMs > 0) pts.append(line.startMs);
        if (line.endMs > 0) pts.append(line.endMs);
    }
    return pts;
}

// Timing Operations

void SubtitleModel::shiftTimes(int amountMs, bool shiftStart, bool shiftEnd, int affectMode, const QVariantList &selectedIndices) {
    if (m_lines.empty()) return;
    QSet<int> selSet;
    for (const auto &v : selectedIndices) selSet.insert(v.toInt());
    int minSelected = static_cast<int>(m_lines.size());
    for (int idx : selSet) {
        if (idx < minSelected) minSelected = idx;
    }

    for (size_t i = 0; i < m_lines.size(); ++i) {
        int idx = static_cast<int>(i);
        if (affectMode == 1 && !selSet.contains(idx)) continue;
        if (affectMode == 2 && idx < minSelected) continue;

        auto &line = m_lines[i];
        if (shiftStart) line.setStartMs(std::max(0, line.startMs + amountMs));
        if (shiftEnd) line.setEndMs(std::max(0, line.endMs + amountMs));
    }
    emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(m_lines.size() - 1), 0));
    emit contentModified();
}

void SubtitleModel::makeTimesContinuous(int curIndex, bool changeStart) {
    if (m_lines.empty() || curIndex < 0 || curIndex >= static_cast<int>(m_lines.size())) return;
    if (changeStart) {
        if (curIndex > 0) {
            m_lines[curIndex].setStartMs(m_lines[curIndex - 1].endMs);
            emit dataChanged(createIndex(curIndex, 0), createIndex(curIndex, 0));
            emit contentModified();
        }
    } else {
        if (curIndex < static_cast<int>(m_lines.size() - 1)) {
            m_lines[curIndex].setEndMs(m_lines[curIndex + 1].startMs);
            emit dataChanged(createIndex(curIndex, 0), createIndex(curIndex, 0));
            emit contentModified();
        }
    }
}

void SubtitleModel::snapStartTime(int curIndex, int videoMs) {
    if (curIndex < 0 || curIndex >= static_cast<int>(m_lines.size())) return;
    m_lines[curIndex].setStartMs(videoMs);
    emit dataChanged(createIndex(curIndex, 0), createIndex(curIndex, 0));
    emit contentModified();
}

bool SubtitleModel::setLineTimes(int index, int startMs, int endMs) {
    if (index < 0 || index >= static_cast<int>(m_lines.size()) || startMs < 0 || endMs < startMs)
        return false;
    auto &line = m_lines[index];
    if (line.startMs == startMs && line.endMs == endMs) return true;
    line.setStartMs(startMs);
    line.setEndMs(endMs);
    emit dataChanged(createIndex(index, 0), createIndex(index, 0));
    emit contentModified();
    return true;
}

QVariantMap SubtitleModel::inlineFormattingState(int row, int position) const {
    if (row < 0 || row >= static_cast<int>(m_lines.size()) || position < 0 || position > m_lines[row].text.size()) return {};
    auto style = getStyleByName(m_lines[row].style);
    if (style.isEmpty()) style = defaultStyles().first().toMap();
    return InlineFormatting::state(m_lines[row].text, position, style,
        [this](const QString &name) { return getStyleByName(name); });
}

QVariantMap SubtitleModel::toggleInlineFormatting(const QVariantList &indices, int activeRow, int start, int end, const QString &tag) {
    if (tag != "b" && tag != "i" && tag != "u" && tag != "s") return {{"success", false}};
    return editInlineFormatting(indices, activeRow, start, end, tag, nullptr);
}

QVariantMap SubtitleModel::applyInlineFont(const QVariantList &indices, int activeRow, int start, int end, const QFont &font) {
    if (font.family().isEmpty() || font.family().contains(QRegularExpression("[{}\\\\\\r\\n]")) ||
        !std::isfinite(font.pointSizeF()) || font.pointSizeF() <= 0 || font.pointSizeF() > 1000000)
        return {{"success", false}};
    return editInlineFormatting(indices, activeRow, start, end, {}, &font);
}

QVariantMap SubtitleModel::editInlineFormatting(const QVariantList &indices, int activeRow, int start, int end, const QString &tag, const QFont *font) {
    if (activeRow < 0 || activeRow >= static_cast<int>(m_lines.size()) || start < 0 || end < start || end > m_lines[activeRow].text.size())
        return {{"success", false}};
    QList<int> selected;
    for (const auto &value : indices) {
        bool ok = false;
        const int row = value.toInt(&ok);
        if (!ok || row < 0 || row >= static_cast<int>(m_lines.size())) return {{"success", false}};
        if (!selected.contains(row)) selected.append(row);
    }
    if (!selected.contains(activeRow)) selected.append(activeRow);
    const auto &activeText = m_lines[activeRow].text;
    const int normStart = InlineFormatting::plainPosition(activeText, start);
    const int normEnd = InlineFormatting::plainPosition(activeText, end);
    auto updated = m_lines;
    bool changed = false;
    int selectionStart = start, selectionEnd = end;
    for (int row : selected) {
        auto &text = updated[row].text;
        int first = row == activeRow ? start : InlineFormatting::rawPosition(text, normStart);
        int last = row == activeRow ? end : InlineFormatting::rawPosition(text, normEnd);
        const auto initial = inlineFormattingState(row, first);
        if (!font) {
            const auto following = inlineFormattingState(row, last);
            if (first != last) {
                auto edit = InlineFormatting::put(text, last, "\\" + tag, following.value(tag).toBool() ? "1" : "0");
                first = edit.map(first, false);
                last = edit.map(last, false);
            }
            const auto edit = InlineFormatting::put(text, first, "\\" + tag, initial.value(tag).toBool() ? "0" : "1");
            first = edit.map(first, true);
            last = edit.map(last, start == end);
        } else {
            const QList<QPair<QString, QVariant>> values{{"font", font->family()}, {"size", font->pointSizeF()},
                {"b", font->bold()}, {"i", font->italic()}, {"u", font->underline()}, {"s", font->strikeOut()}};
            for (const auto &value : values) {
                if (initial.value(value.first) == value.second) continue;
                const QString name = value.first == "font" ? "\\fn" : value.first == "size" ? "\\fs" : "\\" + value.first;
                const QString argument = value.first == "font" ? value.second.toString()
                    : value.first == "size" ? QString::number(value.second.toDouble(), 'g', 12) : value.second.toBool() ? "1" : "0";
                const auto edit = InlineFormatting::put(text, first, name, argument);
                first = edit.map(first, true);
                last = edit.map(last, start == end);
            }
        }
        if (row == activeRow) { selectionStart = first; selectionEnd = last; }
        changed |= text != m_lines[row].text;
        updated[row].updateCps();
    }
    if (changed) {
        pushUndo(font ? tr("set font") : tr("toggle formatting"), activeRow, selected);
        setRawLines(std::move(updated));
    }
    return {{"success", true}, {"changed", changed}, {"selectionStart", selectionStart}, {"selectionEnd", selectionEnd}};
}

void SubtitleModel::snapEndTime(int curIndex, int videoMs) {
    if (curIndex < 0 || curIndex >= static_cast<int>(m_lines.size())) return;
    m_lines[curIndex].setEndMs(videoMs);
    emit dataChanged(createIndex(curIndex, 0), createIndex(curIndex, 0));
    emit contentModified();
}

QVariantMap SubtitleModel::recombineSelectedLines(const QVariantList &selectedIndices, int activeIndex) {
    if (selectedIndices.size() < 2) return {{"success", true}, {"changed", false}};
    struct WorkLine {
        int originalIndex;
        SubtitleLine line;
        bool removed = false;
    };
    std::vector<WorkLine> work;
    work.reserve(selectedIndices.size());
    QSet<int> seen;
    for (const auto &value : selectedIndices) {
        bool ok = false;
        const int index = value.toInt(&ok);
        if (!ok || index < 0 || index >= static_cast<int>(m_lines.size()))
            return {{"success", false}, {"message", tr("Invalid selected line")}};
        if (seen.contains(index)) continue;
        seen.insert(index);
        work.push_back({index, m_lines[index], false});
    }
    if (work.size() < 2) return {{"success", true}, {"changed", false}};
    std::stable_sort(work.begin(), work.end(), [](const WorkLine &a, const WorkLine &b) {
        return a.line.startMs < b.line.startMs;
    });

    static const QRegularExpression leading(QStringLiteral(R"(^(?:[ \t]|\\[nNh])+)"));
    static const QRegularExpression trailing(QStringLiteral(R"((?:[ \t]|\\[nNh])+$)"));
    auto trimText = [&](const QString &text) {
        QString result = text;
        result.remove(leading);
        result.remove(trailing);
        return result;
    };
    auto expandTimes = [](WorkLine &destination, const WorkLine &source) {
        destination.line.setStartMs(std::min(destination.line.startMs, source.line.startMs));
        destination.line.setEndMs(std::max(destination.line.endMs, source.line.endMs));
    };
    auto checkStart = [&](WorkLine &destination, const WorkLine &source) {
        if (!destination.line.text.startsWith(source.line.text)) return false;
        destination.line.text = trimText(destination.line.text.mid(source.line.text.size()));
        destination.line.updateCps();
        expandTimes(destination, source);
        return true;
    };
    auto checkEnd = [&](WorkLine &destination, const WorkLine &source) {
        if (!destination.line.text.endsWith(source.line.text)) return false;
        destination.line.text = trimText(destination.line.text.left(destination.line.text.size() - source.line.text.size()));
        destination.line.updateCps();
        expandTimes(destination, source);
        return true;
    };

    for (auto &item : work) {
        item.line.text = trimText(item.line.text);
        item.line.updateCps();
    }
    const auto end = work.size() - 1;
    for (size_t current = 0; current < end; ++current) {
        auto &first = work[current];
        size_t next = current + 1;
        if (first.line.text == work[next].line.text) {
            expandTimes(work[next], first);
            first.removed = true;
            continue;
        }
        if (first.line.text.isEmpty()) {
            first.removed = true;
            continue;
        }
        if (next == end && work[next].line.text.isEmpty()) {
            work[next].removed = true;
            continue;
        }
        while (next <= end && checkStart(work[next], first)) ++next;
        while (next <= end && checkEnd(work[next], first)) ++next;
        while (next <= end && checkEnd(first, work[next])) ++next;
        while (next <= end && checkStart(first, work[next])) ++next;
    }

    std::vector<bool> removed(m_lines.size(), false);
    auto updated = m_lines;
    bool changed = false;
    for (const auto &item : work) {
        if (item.removed) {
            removed[item.originalIndex] = true;
            changed = true;
        } else {
            if (item.line.toMap() != m_lines[item.originalIndex].toMap()) changed = true;
            updated[item.originalIndex] = item.line;
        }
    }
    if (!changed) return {{"success", true}, {"changed", false}};

    std::vector<SubtitleLine> result;
    result.reserve(m_lines.size());
    std::vector<int> newIndex(m_lines.size(), -1);
    for (size_t i = 0; i < updated.size(); ++i) {
        if (removed[i]) continue;
        newIndex[i] = static_cast<int>(result.size());
        result.push_back(std::move(updated[i]));
    }
    QVariantList newSelection;
    for (int index = 0; index < static_cast<int>(m_lines.size()); ++index)
        if (seen.contains(index) && newIndex[index] >= 0) newSelection.append(newIndex[index]);
    int newActive = activeIndex >= 0 && activeIndex < static_cast<int>(newIndex.size()) ? newIndex[activeIndex] : -1;
    if (newActive < 0) newActive = newSelection.isEmpty() ? (result.empty() ? -1 : 0) : newSelection.first().toInt();

    pushUndo(tr("recombine lines"), activeIndex, selectedIndices);
    setRawLines(std::move(result));
    return {{"success", true}, {"changed", true}, {"selectedIndices", newSelection}, {"selectedIndex", newActive}};
}

void SubtitleModel::shiftToCurrentFrame(int videoMs, const QVariantList &selectedIndices) {
    if (m_lines.empty() || selectedIndices.isEmpty()) return;
    QList<int> sorted;
    for (const auto &v : selectedIndices) sorted.append(v.toInt());
    std::sort(sorted.begin(), sorted.end());

    int firstIdx = sorted[0];
    if (firstIdx < 0 || firstIdx >= static_cast<int>(m_lines.size())) return;
    int offset = videoMs - m_lines[firstIdx].startMs;

    for (int idx : sorted) {
        if (idx >= 0 && idx < static_cast<int>(m_lines.size())) {
            m_lines[idx].setStartMs(std::max(0, m_lines[idx].startMs + offset));
            m_lines[idx].setEndMs(std::max(0, m_lines[idx].endMs + offset));
            emit dataChanged(createIndex(idx, 0), createIndex(idx, 0));
        }
    }
    emit contentModified();
}

QVariantMap SubtitleModel::processTiming(const QVariantMap &options, const QVariantList &selectedIndices, int activeIndex) {
    if (m_lines.empty()) return {{"success", true}, {"changed", false}, {"modifiedCount", 0}};
    try {
        auto number = [&](const char *key, int fallback = 0) {
            bool ok = false;
            const auto value = options.value(QLatin1String(key), fallback);
            const int result = value.toInt(&ok);
            if (!ok || result < 0) throw std::invalid_argument("Invalid timing threshold");
            return result;
        };
        const int leadIn = number("leadIn");
        const int leadOut = number("leadOut");
        const bool adjacent = options.value(QStringLiteral("adjacentEnabled")).toBool();
        const int maxGap = number("maxGap");
        const int maxOverlap = number("maxOverlap");
        const double bias = options.value(QStringLiteral("bias"), 0.5).toDouble();
        if (!std::isfinite(bias) || bias < 0.0 || bias > 1.0)
            throw std::invalid_argument("Invalid adjacent bias");
        const bool selectedOnly = options.value(QStringLiteral("selectedOnly")).toBool();
        const QStringList allowedStyles = options.value(QStringLiteral("allowedStyles")).toStringList();
        if (allowedStyles.isEmpty()) return {{"success", true}, {"changed", false}, {"modifiedCount", 0}};
        QSet<int> selected;
        for (const auto &value : selectedIndices) {
            bool ok = false;
            const int index = value.toInt(&ok);
            if (ok && index >= 0 && index < static_cast<int>(m_lines.size())) selected.insert(index);
        }
        std::vector<int> targets;
        targets.reserve(m_lines.size());
        for (int index = 0; index < static_cast<int>(m_lines.size()); ++index) {
            const auto &line = m_lines[index];
            if (line.isComment || (selectedOnly && !selected.contains(index)) ||
                !allowedStyles.contains(line.style)) continue;
            if (line.startMs > line.endMs)
                throw std::invalid_argument("Selected subtitle has negative duration");
            targets.push_back(index);
        }
        if (targets.empty()) return {{"success", true}, {"changed", false}, {"modifiedCount", 0}};
        std::stable_sort(targets.begin(), targets.end(), [&](int a, int b) {
            return m_lines[a].startMs < m_lines[b].startMs;
        });

        const bool keysEnabled = options.value(QStringLiteral("keyframesEnabled")).toBool();
        std::vector<int> keyframes;
        agi::vfr::Framerate framerate;
        int beforeStart = 0, afterStart = 0, beforeEnd = 0, afterEnd = 0;
        if (keysEnabled) {
            const auto context = options.value(QStringLiteral("framerate")).toMap();
            if (!context.value(QStringLiteral("available")).toBool())
                throw std::invalid_argument("Video timecodes are unavailable");
            if (context.value(QStringLiteral("isVfr")).toBool()) {
                std::vector<int> times;
                for (const auto &value : context.value(QStringLiteral("timecodes")).toList()) times.push_back(value.toInt());
                framerate = agi::vfr::Framerate(std::move(times));
            } else {
                framerate = agi::vfr::Framerate(context.value(QStringLiteral("outputFps")).toDouble());
            }
            if (!framerate.IsLoaded()) throw std::invalid_argument("Invalid video timecodes");
            for (const auto &value : options.value(QStringLiteral("keyframes")).toList()) keyframes.push_back(value.toInt());
            const int frameCount = number("frameCount");
            if (frameCount > 0) keyframes.push_back(frameCount - 1);
            std::sort(keyframes.begin(), keyframes.end());
            keyframes.erase(std::unique(keyframes.begin(), keyframes.end()), keyframes.end());
            if (keyframes.empty()) throw std::invalid_argument("Video keyframes are unavailable");
            beforeStart = number("beforeStart"); afterStart = number("afterStart");
            beforeEnd = number("beforeEnd"); afterEnd = number("afterEnd");
        }
        if (!(leadIn || leadOut || adjacent || keysEnabled))
            return {{"success", true}, {"changed", false}, {"modifiedCount", 0}};

        auto updated = m_lines;
        auto collides = [](const SubtitleLine &a, const SubtitleLine &b) {
            return a.startMs < b.startMs ? b.startMs < a.endMs : a.startMs < b.endMs;
        };
        for (size_t pos = 0; pos < targets.size(); ++pos) {
            auto &line = updated[targets[pos]];
            if (leadIn) {
                qint64 start = static_cast<qint64>(line.startMs) - leadIn;
                for (size_t previous = 0; previous < pos; ++previous) {
                    const auto &other = updated[targets[previous]];
                    if (!collides(line, other)) start = std::max(start, static_cast<qint64>(other.endMs));
                }
                line.setStartMs(static_cast<int>(std::max<qint64>(0, start)));
            }
        }
        for (size_t pos = 0; pos < targets.size(); ++pos) {
            auto &line = updated[targets[pos]];
            if (leadOut) {
                qint64 end = static_cast<qint64>(line.endMs) + leadOut;
                for (size_t next = pos + 1; next < targets.size(); ++next) {
                    const auto &other = updated[targets[next]];
                    if (!collides(line, other)) end = std::min(end, static_cast<qint64>(other.startMs));
                }
                if (end > INT_MAX) throw std::range_error("Timing exceeds supported range");
                line.setEndMs(static_cast<int>(end));
            }
        }
        if (adjacent) {
            for (size_t pos = 1; pos < targets.size(); ++pos) {
                auto &previous = updated[targets[pos - 1]];
                auto &current = updated[targets[pos]];
                const qint64 distance = static_cast<qint64>(current.startMs) - previous.endMs;
                if ((distance < 0 && -distance <= maxOverlap) || (distance > 0 && distance <= maxGap)) {
                    const qint64 midpoint = previous.endMs + static_cast<qint64>(distance * bias);
                    if (midpoint < 0 || midpoint > INT_MAX) throw std::range_error("Timing exceeds supported range");
                    previous.setEndMs(static_cast<int>(midpoint));
                    current.setStartMs(static_cast<int>(midpoint));
                }
            }
        }
        if (keysEnabled) {
            auto closestKeyframe = [&](int frame) {
                auto upper = std::upper_bound(keyframes.begin(), keyframes.end(), frame);
                if (upper == keyframes.end()) return keyframes.back();
                if (upper == keyframes.begin() || *upper - frame < frame - *(upper - 1)) return *upper;
                return *(upper - 1);
            };
            for (int index : targets) {
                auto &line = updated[index];
                const int startFrame = framerate.FrameAtTime(line.startMs, agi::vfr::START);
                const int endFrame = framerate.FrameAtTime(line.endMs, agi::vfr::END);
                const int startKey = closestKeyframe(startFrame);
                const int startTime = framerate.TimeAtFrame(startKey, agi::vfr::START);
                if ((startKey > startFrame && startTime - line.startMs <= beforeStart) ||
                    (startKey < startFrame && line.startMs - startTime <= afterStart)) line.setStartMs(startTime);
                const int endKey = closestKeyframe(endFrame) - 1;
                if (endKey >= 0) {
                    const int endTime = framerate.TimeAtFrame(endKey, agi::vfr::END);
                    if ((endKey > endFrame && endTime - line.endMs <= beforeEnd) ||
                        (endKey < endFrame && line.endMs - endTime <= afterEnd)) line.setEndMs(endTime);
                }
            }
        }
        int modifiedCount = 0;
        for (int index : targets)
            if (updated[index].startMs != m_lines[index].startMs || updated[index].endMs != m_lines[index].endMs)
                ++modifiedCount;
        if (!modifiedCount) return {{"success", true}, {"changed", false}, {"modifiedCount", 0}};
        pushUndo(tr("timing post-processor"), activeIndex, selectedIndices);
        m_lines.swap(updated);
        emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(m_lines.size() - 1), 0));
        emit contentModified();
        return {{"success", true}, {"changed", true}, {"modifiedCount", modifiedCount}};
    } catch (const std::exception &error) {
        return {{"success", false}, {"message", QString::fromUtf8(error.what())}};
    }
}

// Line Operations

int SubtitleModel::insertLine(int baseIndex, bool before, int startMs, int endMs, const QVariantMap &defaults) {
    int insertIdx = before ? baseIndex : (baseIndex + 1);
    insertIdx = qBound(0, insertIdx, static_cast<int>(m_lines.size()));

    SubtitleLine line;
    if (baseIndex >= 0 && baseIndex < static_cast<int>(m_lines.size())) {
        line = m_lines[baseIndex];
    }
    line.setStartMs(startMs);
    line.setEndMs(endMs);
    line.text.clear();
    if (defaults.contains(QStringLiteral("style"))) line.style = defaults.value(QStringLiteral("style")).toString();
    if (defaults.contains(QStringLiteral("layer"))) line.layer = defaults.value(QStringLiteral("layer")).toInt();

    beginInsertRows(QModelIndex(), insertIdx, insertIdx);
    m_lines.insert(m_lines.begin() + insertIdx, line);
    endInsertRows();

    renumberLines();
    emit countChanged();
    emit contentModified();
    return insertIdx;
}

QList<int> SubtitleModel::duplicateSelectedLines(const QVariantList &selectedIndices) {
    if (m_lines.empty() || selectedIndices.isEmpty()) return {};
    QList<int> sorted;
    for (const auto &v : selectedIndices) sorted.append(v.toInt());
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    if (sorted.isEmpty()) return {};

    int insertPos = std::min(sorted.last() + 1, static_cast<int>(m_lines.size()));
    QList<SubtitleLine> copies;
    copies.reserve(sorted.size());
    for (int idx : sorted) {
        if (idx >= 0 && idx < static_cast<int>(m_lines.size())) {
            copies.append(m_lines[idx]);
        }
    }
    if (copies.isEmpty()) return {};

    beginInsertRows(QModelIndex(), insertPos, insertPos + static_cast<int>(copies.size()) - 1);
    QList<int> newIndices;
    newIndices.reserve(copies.size());
    for (int i = 0; i < copies.size(); ++i) {
        m_lines.insert(m_lines.begin() + insertPos + i, copies[i]);
        newIndices.append(insertPos + i);
    }
    endInsertRows();

    renumberLines();
    emit countChanged();
    emit contentModified();
    return newIndices;
}

int SubtitleModel::deleteSelectedLines(const QVariantList &selectedIndices) {
    if (m_lines.empty() || selectedIndices.isEmpty()) return 0;
    QList<int> sorted;
    for (const auto &v : selectedIndices) sorted.append(v.toInt());
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    if (sorted.isEmpty()) return 0;

    for (int idx : sorted) {
        if (idx >= 0 && idx < static_cast<int>(m_lines.size())) {
            beginRemoveRows(QModelIndex(), idx, idx);
            m_lines.erase(m_lines.begin() + idx);
            endRemoveRows();
        }
    }

    if (m_lines.empty()) {
        beginInsertRows(QModelIndex(), 0, 0);
        SubtitleLine def;
        def.lineNumber = 1;
        def.setStartMs(0);
        def.setEndMs(5000);
        m_lines.push_back(def);
        endInsertRows();
    }

    renumberLines();
    emit countChanged();
    emit contentModified();

    int nextIdx = sorted.last();
    if (nextIdx >= static_cast<int>(m_lines.size())) {
        nextIdx = static_cast<int>(m_lines.size()) - 1;
    }
    return std::max(0, nextIdx);
}

void SubtitleModel::swapSelectedLines(const QVariantList &selectedIndices) {
    if (selectedIndices.size() != 2) return;
    int idx1 = selectedIndices[0].toInt();
    int idx2 = selectedIndices[1].toInt();
    if (idx1 < 0 || idx1 >= static_cast<int>(m_lines.size()) ||
        idx2 < 0 || idx2 >= static_cast<int>(m_lines.size()) || idx1 == idx2) return;

    std::swap(m_lines[idx1], m_lines[idx2]);
    std::swap(m_lines[idx1].lineNumber, m_lines[idx2].lineNumber);

    emit dataChanged(createIndex(idx1, 0), createIndex(idx1, 0));
    emit dataChanged(createIndex(idx2, 0), createIndex(idx2, 0));
    emit contentModified();
}

void SubtitleModel::joinSelectedLines(const QVariantList &selectedIndices, int mode) {
    if (selectedIndices.size() < 2) return;
    QList<int> sorted;
    for (const auto &v : selectedIndices) sorted.append(v.toInt());
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    if (sorted.size() < 2) return;

    int minStart = INT_MAX;
    int maxEnd = INT_MIN;
    QStringList texts;
    for (int idx : sorted) {
        if (idx >= 0 && idx < static_cast<int>(m_lines.size())) {
            const auto &l = m_lines[idx];
            if (l.startMs < minStart) minStart = l.startMs;
            if (l.endMs > maxEnd) maxEnd = l.endMs;
            texts.append(l.text);
        }
    }

    int firstIdx = sorted[0];
    QString joinedText;
    if (mode == 0) {
        joinedText = texts.join(QLatin1Char(' '));
    } else if (mode == 1) {
        joinedText = texts.isEmpty() ? QString() : texts[0];
    } else if (mode == 2) {
        QStringList kParts;
        for (int idx : sorted) {
            const auto &item = m_lines[idx];
            int durCs = static_cast<int>(std::round((item.endMs - item.startMs) / 10.0));
            kParts.append(QStringLiteral("{\\k%1}%2").arg(durCs).arg(item.text));
        }
        joinedText = kParts.join(QLatin1Char(' '));
    }

    m_lines[firstIdx].setStartMs(minStart);
    m_lines[firstIdx].setEndMs(maxEnd);
    m_lines[firstIdx].text = joinedText;
    m_lines[firstIdx].updateCps();

    for (int i = sorted.size() - 1; i >= 1; --i) {
        int delIdx = sorted[i];
        beginRemoveRows(QModelIndex(), delIdx, delIdx);
        m_lines.erase(m_lines.begin() + delIdx);
        endRemoveRows();
    }

    renumberLines();
    emit dataChanged(createIndex(firstIdx, 0), createIndex(firstIdx, 0));
    emit countChanged();
    emit contentModified();
}

QList<int> SubtitleModel::splitLinesAtFrame(const QVariantList &selectedIndices,
    int firstEndMs, int secondStartMs, int frameStartMs, int frameEndMs, int frameExactMs) {
    QSet<int> selected;
    for (const auto &value : selectedIndices) {
        const int row = value.toInt();
        if (row >= 0 && row < static_cast<int>(m_lines.size())) selected.insert(row);
    }
    if (selected.isEmpty()) return {};
    std::vector<SubtitleLine> lines;
    lines.reserve(m_lines.size() + selected.size());
    QList<int> newIndices;
    for (int row = 0; row < static_cast<int>(m_lines.size());) {
        if (!selected.contains(row)) {
            lines.push_back(m_lines[row++]);
            continue;
        }
        // Insert each duplicated contiguous selection immediately after its source block.
        std::vector<SubtitleLine> copies;
        do {
            auto original = m_lines[row];
            auto copy = original;
            if (original.startMs > frameExactMs || original.endMs <= frameExactMs) {
                copy.setStartMs(frameStartMs);
                copy.setEndMs(frameEndMs);
            } else {
                original.setEndMs(firstEndMs);
                copy.setStartMs(secondStartMs);
            }
            lines.push_back(std::move(original));
            copies.push_back(std::move(copy));
            ++row;
        } while (row < static_cast<int>(m_lines.size()) && selected.contains(row));
        for (auto &copy : copies) {
            newIndices.append(static_cast<int>(lines.size()));
            lines.push_back(std::move(copy));
        }
    }
    beginResetModel();
    m_lines = std::move(lines);
    for (size_t row = 0; row < m_lines.size(); ++row) m_lines[row].lineNumber = static_cast<int>(row + 1);
    endResetModel();
    emit countChanged();
    emit contentModified();
    return newIndices;
}

void SubtitleModel::splitLineAtCursor(int curIndex, int pos, int mode, int videoMs) {
    if (curIndex < 0 || curIndex >= static_cast<int>(m_lines.size()) || pos <= 0) return;
    const auto &item = m_lines[curIndex];
    if (pos >= item.text.length()) return;

    QString text1 = item.text.left(pos);
    QString text2 = item.text.mid(pos);
    int sMs = item.startMs;
    int eMs = item.endMs;
    int e1 = eMs, s2 = eMs;

    if (mode == 0) {
        e1 = eMs; s2 = sMs;
    } else if (mode == 1) {
        int totalLen = std::max(1, static_cast<int>(item.text.length()));
        int dur = eMs - sMs;
        int dur1 = static_cast<int>(std::round(dur * (static_cast<double>(pos) / totalLen)));
        e1 = sMs + dur1;
        s2 = e1;
    } else if (mode == 2) {
        e1 = std::clamp(videoMs, sMs, eMs);
        s2 = e1;
    }

    SubtitleLine part2 = item;
    part2.setStartMs(s2);
    part2.setEndMs(eMs);
    part2.text = text2;
    part2.updateCps();

    m_lines[curIndex].setEndMs(e1);
    m_lines[curIndex].text = text1;
    m_lines[curIndex].updateCps();

    beginInsertRows(QModelIndex(), curIndex + 1, curIndex + 1);
    m_lines.insert(m_lines.begin() + curIndex + 1, part2);
    endInsertRows();

    renumberLines();
    emit dataChanged(createIndex(curIndex, 0), createIndex(curIndex, 0));
    emit countChanged();
    emit contentModified();
}

void SubtitleModel::sortLines(const QString &field, const QVariantList &selectedIndices, bool ascending) {
    if (m_lines.size() <= 1) return;
    QList<int> indices;
    if (!selectedIndices.isEmpty()) {
        for (const auto &v : selectedIndices) indices.append(v.toInt());
        std::sort(indices.begin(), indices.end());
    } else {
        for (size_t i = 0; i < m_lines.size(); ++i) indices.append(static_cast<int>(i));
    }
    if (indices.size() <= 1) return;

    std::vector<SubtitleLine> subset;
    subset.reserve(indices.size());
    for (int idx : indices) {
        subset.push_back(m_lines[idx]);
    }

    auto comp = [&](const SubtitleLine &a, const SubtitleLine &b) {
        if (field == QStringLiteral("layer")) {
            return ascending ? (a.layer < b.layer) : (a.layer > b.layer);
        } else if (field == QStringLiteral("start")) {
            return ascending ? (a.startMs < b.startMs) : (a.startMs > b.startMs);
        } else if (field == QStringLiteral("end")) {
            return ascending ? (a.endMs < b.endMs) : (a.endMs > b.endMs);
        } else if (field == QStringLiteral("style")) {
            int cmp = a.style.compare(b.style, Qt::CaseInsensitive);
            return ascending ? (cmp < 0) : (cmp > 0);
        } else if (field == QStringLiteral("actor")) {
            int cmp = a.actor.compare(b.actor, Qt::CaseInsensitive);
            return ascending ? (cmp < 0) : (cmp > 0);
        } else if (field == QStringLiteral("effect")) {
            int cmp = a.effect.compare(b.effect, Qt::CaseInsensitive);
            return ascending ? (cmp < 0) : (cmp > 0);
        }
        return false;
    };

    std::stable_sort(subset.begin(), subset.end(), comp);

    for (size_t i = 0; i < subset.size(); ++i) {
        m_lines[indices[i]] = subset[i];
    }
    renumberLines();
    emit dataChanged(createIndex(indices.first(), 0), createIndex(indices.last(), 0));
    emit contentModified();
}

void SubtitleModel::sortByColumn(int col, bool ascending) {
    if (m_lines.size() <= 1) return;
    auto comp = [&](const SubtitleLine &a, const SubtitleLine &b) {
        switch (col) {
        case 0: return ascending ? (a.lineNumber < b.lineNumber) : (a.lineNumber > b.lineNumber);
        case 1: return ascending ? (a.layer < b.layer) : (a.layer > b.layer);
        case 2: return ascending ? (a.startMs < b.startMs) : (a.startMs > b.startMs);
        case 3: return ascending ? (a.endMs < b.endMs) : (a.endMs > b.endMs);
        case 4: return ascending ? (a.cps.toDouble() < b.cps.toDouble()) : (a.cps.toDouble() > b.cps.toDouble());
        case 5: {
            int c = a.style.compare(b.style, Qt::CaseInsensitive);
            return ascending ? (c < 0) : (c > 0);
        }
        case 6: {
            int c = a.actor.compare(b.actor, Qt::CaseInsensitive);
            return ascending ? (c < 0) : (c > 0);
        }
        case 7: {
            int c = a.effect.compare(b.effect, Qt::CaseInsensitive);
            return ascending ? (c < 0) : (c > 0);
        }
        case 8: return ascending ? (a.marginLeft < b.marginLeft) : (a.marginLeft > b.marginLeft);
        case 9: return ascending ? (a.marginRight < b.marginRight) : (a.marginRight > b.marginRight);
        case 10: return ascending ? (a.marginVert < b.marginVert) : (a.marginVert > b.marginVert);
        case 11: {
            int c = a.text.compare(b.text, Qt::CaseInsensitive);
            return ascending ? (c < 0) : (c > 0);
        }
        default: return false;
        }
    };

    std::stable_sort(m_lines.begin(), m_lines.end(), comp);
    renumberLines();
    emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(m_lines.size() - 1), 0));
    emit contentModified();
}

int SubtitleModel::applyKanjiCopy(const QString &srcStyle, const QString &dstStyle) {
    if (m_lines.empty()) return 0;
    std::vector<int> srcIndices;
    std::vector<int> dstIndices;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        if (m_lines[i].style == srcStyle) srcIndices.push_back(static_cast<int>(i));
        else if (m_lines[i].style == dstStyle) dstIndices.push_back(static_cast<int>(i));
    }
    size_t count = std::min(srcIndices.size(), dstIndices.size());
    for (size_t j = 0; j < count; ++j) {
        int srcIdx = srcIndices[j];
        int dstIdx = dstIndices[j];
        m_lines[dstIdx].setStartMs(m_lines[srcIdx].startMs);
        m_lines[dstIdx].setEndMs(m_lines[srcIdx].endMs);
        emit dataChanged(createIndex(dstIdx, 0), createIndex(dstIdx, 0));
    }
    if (count > 0) {
        emit contentModified();
    }
    return static_cast<int>(count);
}

// Search and Filter Operations
namespace {
struct SearchView {
    QString text;
    QVector<qsizetype> positions;
    qsizetype sourceLength = 0;

    qsizetype sourceAt(qsizetype offset) const {
        return offset >= 0 && offset < positions.size() ? positions[offset] : sourceLength;
    }
};

SearchView makeSearchView(const QString &source, bool skipTags) {
    SearchView view;
    view.sourceLength = source.size();
    for (qsizetype i = 0; i < source.size(); ++i) {
        if (skipTags && source[i] == QLatin1Char('{')) {
            const qsizetype end = source.indexOf(QLatin1Char('}'), i + 1);
            if (end >= 0) {
                i = end;
                continue;
            }
        }
        view.text += source[i];
        view.positions.append(i);
    }
    return view;
}

QString expandRegexReplacement(const QString &replacement, const QRegularExpressionMatch &match) {
    QString expanded;
    expanded.reserve(replacement.size());
    for (qsizetype pos = 0; pos < replacement.size(); ++pos) {
        if (replacement[pos] == QLatin1Char('\\') && pos + 1 < replacement.size()) {
            if (replacement[pos + 1] == QLatin1Char('\\')) {
                expanded += QLatin1Char('\\');
                ++pos;
                continue;
            }
            if (replacement[pos + 1].isDigit()) {
                int group = replacement[++pos].digitValue();
                if (pos + 1 < replacement.size() && replacement[pos + 1].isDigit())
                    group = group * 10 + replacement[++pos].digitValue();
                expanded += match.captured(group);
                continue;
            }
        }
        expanded += replacement[pos];
    }
    return expanded;
}
}

QList<int> SubtitleModel::selectLines(int action, int fieldIdx, int mode, bool invert, bool matchCase,
                                      bool comments, bool dialogues, const QString &query,
                                      const QVariantList &currentSelectedIndices) {
    QList<int> matches;
    QRegularExpression rx;
    if (mode == 2) {
        QRegularExpression::PatternOptions pOpts = matchCase ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption;
        rx = QRegularExpression(query, pOpts);
    }

    for (size_t i = 0; i < m_lines.size(); ++i) {
        const auto &line = m_lines[i];
        if (line.isComment && !comments) continue;
        if (!line.isComment && !dialogues) continue;

        QString val;
        switch (fieldIdx) {
        case 0: val = line.text; break;
        case 1: val = line.style; break;
        case 2: val = line.actor; break;
        case 3: val = line.effect; break;
        }

        bool isMatch = false;
        if (mode == 0) {
            isMatch = matchCase ? (val == query) : (val.compare(query, Qt::CaseInsensitive) == 0);
        } else if (mode == 1) {
            isMatch = matchCase ? val.contains(query) : val.contains(query, Qt::CaseInsensitive);
        } else if (mode == 2 && rx.isValid()) {
            isMatch = rx.match(val).hasMatch();
        }

        if (invert) isMatch = !isMatch;
        if (isMatch) matches.append(static_cast<int>(i));
    }

    QList<int> current;
    for (const auto &v : currentSelectedIndices) current.append(v.toInt());

    QList<int> finalSel;
    if (action == 0) {
        finalSel = matches;
    } else if (action == 1) {
        QSet<int> set(current.begin(), current.end());
        for (int m : matches) set.insert(m);
        finalSel = set.values();
    } else if (action == 2) {
        QSet<int> matchSet(matches.begin(), matches.end());
        for (int c : current) {
            if (!matchSet.contains(c)) finalSel.append(c);
        }
    } else if (action == 3) {
        QSet<int> matchSet(matches.begin(), matches.end());
        for (int c : current) {
            if (matchSet.contains(c)) finalSel.append(c);
        }
    }

    std::sort(finalSel.begin(), finalSel.end());
    return finalSel;
}

int SubtitleModel::findAndReplace(const QString &query, const QString &replaceWith,
                                  const QVariantMap &options, bool replaceAll, int currentIndex) {
    if (query.isEmpty() || m_lines.empty()) return 0;
    bool matchCase = options.value(QStringLiteral("matchCase"), false).toBool();
    bool useRegex = options.value(QStringLiteral("useRegex"), false).toBool();
    bool skipComments = options.value(QStringLiteral("skipComments"), true).toBool();
    bool skipTags = options.value(QStringLiteral("skipTags"), false).toBool();
    QString field = options.value(QStringLiteral("field"), QStringLiteral("text")).toString();
    bool selectedOnly = options.value(QStringLiteral("selectedOnly"), false).toBool();
    QList<int> selList;
    if (selectedOnly) {
        for (const auto &v : options.value(QStringLiteral("selectedIndices")).toList()) {
            selList.append(v.toInt());
        }
    }

    QRegularExpression rx;
    if (useRegex) {
        QRegularExpression::PatternOptions pOpts = QRegularExpression::NoPatternOption;
        if (!matchCase) pOpts |= QRegularExpression::CaseInsensitiveOption;
        rx = QRegularExpression(query, pOpts);
        if (!rx.isValid()) return -1;
    }

    int replacedCount = 0;
    bool changedAny = false;
    int startIdx = replaceAll ? 0 : std::max(0, currentIndex);
    int endIdx = replaceAll ? static_cast<int>(m_lines.size()) : (startIdx + 1);

    for (int i = startIdx; i < endIdx && i < static_cast<int>(m_lines.size()); ++i) {
        if (selectedOnly && !selList.contains(i)) continue;
        auto &line = m_lines[i];
        if (skipComments && line.isComment) continue;

        QString *targetStr = nullptr;
        if (field == QStringLiteral("text")) targetStr = &line.text;
        else if (field == QStringLiteral("style")) targetStr = &line.style;
        else if (field == QStringLiteral("actor")) targetStr = &line.actor;
        else if (field == QStringLiteral("effect")) targetStr = &line.effect;
        if (!targetStr) continue;

        const QString original = *targetStr;
        const SearchView view = makeSearchView(original, skipTags && field == QStringLiteral("text"));
        struct Replacement { qsizetype start; qsizetype length; QString value; };
        QVector<Replacement> replacements;
        if (useRegex) {
            auto matches = rx.globalMatch(view.text);
            while (matches.hasNext()) {
                const auto match = matches.next();
                replacements.append({match.capturedStart(), match.capturedLength(),
                                     expandRegexReplacement(replaceWith, match)});
                if (!replaceAll) break;
            }
        } else {
            const auto sensitivity = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
            for (qsizetype pos = view.text.indexOf(query, 0, sensitivity); pos >= 0;
                 pos = view.text.indexOf(query, pos + query.size(), sensitivity)) {
                replacements.append({pos, query.size(), replaceWith});
                if (!replaceAll) break;
            }
        }
        if (replacements.isEmpty()) continue;
        QString result = original;
        for (auto it = replacements.crbegin(); it != replacements.crend(); ++it) {
            const qsizetype start = view.sourceAt(it->start);
            const qsizetype end = it->length ? view.sourceAt(it->start + it->length - 1) + 1 : start;
            result.replace(start, end - start, it->value);
        }
        replacedCount += replacements.size();
        if (result != original) {
            *targetStr = result;
            line.updateCps();
            changedAny = true;
            emit dataChanged(createIndex(i, 0), createIndex(i, 0));
        }
        if (!replaceAll) break;
    }

    if (changedAny) {
        emit contentModified();
    }
    return replacedCount;
}

int SubtitleModel::findNext(const QString &query, const QVariantMap &options, int startIndex) {
    const QVariantMap match = findNextMatch(query, options, startIndex, 0);
    return match.value(QStringLiteral("row"), -1).toInt();
}

QVariantMap SubtitleModel::findNextMatch(const QString &query, const QVariantMap &options,
                                         int startIndex, int startOffset) {
    if (query.isEmpty() || m_lines.empty()) return {};
    const bool matchCase = options.value(QStringLiteral("matchCase"), false).toBool();
    const bool useRegex = options.value(QStringLiteral("useRegex"), false).toBool();
    const bool skipComments = options.value(QStringLiteral("skipComments"), true).toBool();
    const bool skipTags = options.value(QStringLiteral("skipTags"), false).toBool();
    const bool selectedOnly = options.value(QStringLiteral("selectedOnly"), false).toBool();
    QSet<int> selected;
    if (selectedOnly) {
        for (const auto &value : options.value(QStringLiteral("selectedIndices")).toList())
            selected.insert(value.toInt());
    }
    const QString field = options.value(QStringLiteral("field"), QStringLiteral("text")).toString();
    const int total = static_cast<int>(m_lines.size());
    const int firstRow = std::max(0, startIndex) % total;
    const int firstOffset = std::max(0, startOffset);

    QRegularExpression rx;
    if (useRegex) {
        auto patternOptions = matchCase ? QRegularExpression::NoPatternOption
                                        : QRegularExpression::CaseInsensitiveOption;
        rx = QRegularExpression(query, patternOptions);
        if (!rx.isValid()) return {};
    }
    for (int step = 0; step <= total; ++step) {
        const int row = (firstRow + step) % total;
        if (selectedOnly && !selected.contains(row)) continue;
        const auto &line = m_lines[row];
        if (skipComments && line.isComment) continue;
        const QString *source = nullptr;
        if (field == QStringLiteral("text")) source = &line.text;
        else if (field == QStringLiteral("style")) source = &line.style;
        else if (field == QStringLiteral("actor")) source = &line.actor;
        else if (field == QStringLiteral("effect")) source = &line.effect;
        if (!source) continue;
        const SearchView view = makeSearchView(*source, skipTags && field == QStringLiteral("text"));
        const qsizetype offset = step == 0
            ? std::distance(view.positions.cbegin(),
                            std::lower_bound(view.positions.cbegin(), view.positions.cend(), firstOffset))
            : 0;
        qsizetype matchStart = -1;
        qsizetype matchLength = 0;
        if (useRegex) {
            const auto match = rx.match(view.text, offset);
            if (match.hasMatch()) {
                matchStart = match.capturedStart();
                matchLength = match.capturedLength();
            }
        } else {
            matchStart = view.text.indexOf(query, offset,
                                           matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive);
            matchLength = query.size();
        }
        if (matchStart < 0) continue;
        const qsizetype rawStart = view.sourceAt(matchStart);
        if (step == total && rawStart >= firstOffset) continue;
        const qsizetype rawEnd = matchLength ? view.sourceAt(matchStart + matchLength - 1) + 1 : rawStart;
        return {{QStringLiteral("row"), row},
                {QStringLiteral("start"), static_cast<int>(rawStart)},
                {QStringLiteral("length"), static_cast<int>(rawEnd - rawStart)}};
    }
    return {};
}
