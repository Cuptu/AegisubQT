// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

#include "SubtitleModel.h"
#include "AegisubCoreBridge.h"
#include <libaegisub/ass/time.h>
#include <QRegularExpression>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <QStringConverter>
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
    info[QStringLiteral("PlayResX")] = 1920;
    info[QStringLiteral("PlayResY")] = 1080;
    return info;
}

QVariantList SubtitleModel::defaultStyles() {
    QVariantList list;
    QVariantMap st;
    st[QStringLiteral("name")] = QStringLiteral("Default");
    st[QStringLiteral("font")] = QStringLiteral("Arial");
    st[QStringLiteral("size")] = 20.0;
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
        const bool wasModified = isModified();
        ++m_commitId;
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
        line.text = value.toString();
        line.updateCps();
        changedRoles << TextRole << CpsRole;
    } else if (propertyName == QStringLiteral("start")) {
        line.setStartStr(value.toString());
        changedRoles << StartRole << CpsRole;
    } else if (propertyName == QStringLiteral("end")) {
        line.setEndStr(value.toString());
        changedRoles << EndRole << CpsRole;
    } else if (propertyName == QStringLiteral("style")) {
        line.style = value.toString();
        changedRoles << StyleRole;
    } else if (propertyName == QStringLiteral("actor")) {
        line.actor = value.toString();
        changedRoles << ActorRole;
    } else if (propertyName == QStringLiteral("effect")) {
        line.effect = value.toString();
        changedRoles << EffectRole;
    } else if (propertyName == QStringLiteral("layer")) {
        line.layer = value.toInt();
        changedRoles << LayerRole;
    } else if (propertyName == QStringLiteral("marginLeft")) {
        line.marginLeft = value.toInt();
        changedRoles << MarginLeftRole;
    } else if (propertyName == QStringLiteral("marginRight")) {
        line.marginRight = value.toInt();
        changedRoles << MarginRightRole;
    } else if (propertyName == QStringLiteral("marginVert")) {
        line.marginVert = value.toInt();
        changedRoles << MarginVertRole;
    } else if (propertyName == QStringLiteral("lineNumber")) {
        line.lineNumber = value.toInt();
        changedRoles << LineNumberRole;
    } else if (propertyName == QStringLiteral("isComment") || propertyName == QStringLiteral("comment")) {
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
    m_undoStack.push_back({ description, snapshotLines, selectedIndex, selList, m_scriptInfo, m_styles });
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
    m_redoStack.push_back(currentSnapshot);

    UndoSnapshot target = m_undoStack.back();
    m_undoStack.pop_back();

    beginResetModel();
    m_lines = *target.lines;
    m_scriptInfo = target.scriptInfo;
    m_styles = target.styles;
    endResetModel();

    emit scriptInfoChanged();
    emit stylesChanged();
    emit countChanged();
    emit undoStateChanged();
    emit contentModified();

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
    m_undoStack.push_back(currentSnapshot);

    UndoSnapshot target = m_redoStack.back();
    m_redoStack.pop_back();

    beginResetModel();
    m_lines = *target.lines;
    m_scriptInfo = target.scriptInfo;
    m_styles = target.styles;
    endResetModel();

    emit scriptInfoChanged();
    emit stylesChanged();
    emit countChanged();
    emit undoStateChanged();
    emit contentModified();

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
    if (index >= 0 && index < m_styles.size()) {
        m_styles[index] = style;
        emit stylesChanged();
        emit contentModified();
    }
}

void SubtitleModel::setStyleByName(const QString &name, const QVariantMap &style) {
    for (int i = 0; i < m_styles.size(); ++i) {
        if (m_styles[i].toMap().value(QStringLiteral("name")).toString().compare(name, Qt::CaseInsensitive) == 0) {
            m_styles[i] = style;
            emit stylesChanged();
            emit contentModified();
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
    QString content = (colonIdx != -1) ? rawLine.mid(colonIdx + 1).trimmed() : rawLine.trimmed();

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

bool SubtitleModel::loadFromFile(const QString &filePath) {
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

    QString content = QString::fromUtf8(rawData);
    if (!content.isEmpty() && content.at(0) == QChar(0xFEFF)) {
        content.remove(0, 1);
    }

    QStringList lines = content.split(QRegularExpression(QStringLiteral("\r\n|\n|\r")));

    QString currentSection;
    QVariantMap newScriptInfo = defaultScriptInfo();
    QVariantList newStyles;
    std::vector<SubtitleLine> newLines;
    QList<AssRawSection> newRawSections;

    int dialogueCount = 0;
    for (const QString &rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            currentSection = line;
            if (currentSection != QStringLiteral("[Script Info]") &&
                currentSection != QStringLiteral("[V4+ Styles]") &&
                currentSection != QStringLiteral("[V4 Styles]") &&
                currentSection != QStringLiteral("[Events]")) {
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
                if (sParts.size() >= 23) {
                    QVariantMap st;
                    st[QStringLiteral("name")] = sParts[0].trimmed();
                    st[QStringLiteral("font")] = sParts[1].trimmed();
                    st[QStringLiteral("size")] = sParts[2].trimmed().toDouble();
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
                    newStyles.append(st);
                }
            }
        } else if (currentSection == QStringLiteral("[Events]")) {
            bool isDialogue = line.startsWith(QStringLiteral("Dialogue:"));
            bool isComment = line.startsWith(QStringLiteral("Comment:"));
            if (isDialogue || isComment) {
                dialogueCount++;
                SubtitleLine parsed = parseDialogueLine(line, isComment, dialogueCount);
                newLines.push_back(parsed);
            }
        } else if (!newRawSections.isEmpty()) {
            newRawSections.last().lines.append(rawLine);
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
    m_undoStack.clear();
    m_redoStack.clear();
    endResetModel();

    emit scriptInfoChanged();
    emit stylesChanged();
    emit fileNameChanged();
    emit countChanged();
    emit undoStateChanged();
    emit contentModified();

    // Newly loaded document is synchronized with disk state and marked unmodified.
    resetModificationTracking();

    return true;
}

bool SubtitleModel::serializeDocument(const QString &target) const {
    // QSaveFile writes to a temp file then atomically renames on commit:
    // a crash mid-write never corrupts the document, and on Windows the
    // replace succeeds even when another process holds the old file open.
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);

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
    if (m_styles.isEmpty()) {
        out << "Style: Default,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1\n";
    } else {
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
    }
    out << "\n";

    out << "[Events]\n";
    out << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
    for (const auto &line : m_lines) {
        out << formatDialogueLine(line) << "\n";
    }

    for (const auto &sec : m_rawSections) {
        if (sec.header != QStringLiteral("[Aegisub Project Garbage]")) {
            out << "\n" << sec.header << "\n";
            for (const auto &l : sec.lines) out << l << "\n";
        }
    }

    out.flush();
    if (out.status() != QTextStream::Ok) {
        return false;
    }
    return file.commit();
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

void SubtitleModel::splitSelectedByKaraoke(const QVariantList &selectedIndices) {
    if (m_lines.empty() || selectedIndices.isEmpty()) return;
    QList<int> sorted;
    for (const auto &v : selectedIndices) sorted.append(v.toInt());
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    if (sorted.isEmpty()) return;

    int grown = 0;
    // Process bottom-up so row insertions never shift pending higher indices.
    for (int i = sorted.size() - 1; i >= 0; --i) {
        const int idx = sorted[i];
        if (idx < 0 || idx >= static_cast<int>(m_lines.size())) continue;
        const SubtitleLine original = m_lines[idx];
        const QList<QVariantMap> syls = AegisubCoreBridge::parseKaraokeLine(
            original.text, original.startMs, original.endMs, false);
        if (syls.size() <= 1) continue;

        // Slice the original timing across syllables: each boundary is the next start,
        // and the final syllable stretches to the original line end (upstream semantics).
        QList<SubtitleLine> pieces;
        pieces.reserve(syls.size());
        int cursor = original.startMs;
        for (int s = 0; s < syls.size(); ++s) {
            SubtitleLine piece = original;
            const int dur = syls[s].value(QStringLiteral("duration")).toInt();
            const int sylStart = cursor;
            const int sylEnd = (s + 1 < syls.size())
                ? sylStart + dur
                : std::max(sylStart, original.endMs);
            piece.setStartMs(sylStart);
            piece.setEndMs(sylEnd);
            piece.text = syls[s].value(QStringLiteral("text")).toString();
            pieces.append(piece);
            cursor = sylEnd;
        }

        m_lines[idx] = pieces[0];
        emit dataChanged(createIndex(idx, 0), createIndex(idx, 0));

        const int insPos = idx + 1;
        beginInsertRows(QModelIndex(), insPos, insPos + static_cast<int>(pieces.size()) - 2);
        for (int p = 1; p < pieces.size(); ++p) {
            m_lines.insert(m_lines.begin() + idx + p, pieces[p]);
        }
        endInsertRows();
        grown += static_cast<int>(pieces.size()) - 1;
    }

    if (grown > 0) {
        renumberLines();
        emit countChanged();
        emit contentModified();
    }
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
    m_undoStack.clear();
    m_redoStack.clear();
    endResetModel();

    emit scriptInfoChanged();
    emit stylesChanged();
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

void SubtitleModel::snapEndTime(int curIndex, int videoMs) {
    if (curIndex < 0 || curIndex >= static_cast<int>(m_lines.size())) return;
    m_lines[curIndex].setEndMs(videoMs);
    emit dataChanged(createIndex(curIndex, 0), createIndex(curIndex, 0));
    emit contentModified();
}

void SubtitleModel::recombineSelectedLines(const QVariantList &selectedIndices) {
    if (selectedIndices.size() < 2) return;
    QList<int> sorted;
    for (const auto &v : selectedIndices) sorted.append(v.toInt());
    std::sort(sorted.begin(), sorted.end());

    for (int i = 0; i < sorted.size() - 1; ++i) {
        int idx = sorted[i];
        int nextIdx = sorted[i + 1];
        if (idx >= 0 && idx < static_cast<int>(m_lines.size()) &&
            nextIdx >= 0 && nextIdx < static_cast<int>(m_lines.size())) {
            m_lines[idx].setEndMs(m_lines[nextIdx].startMs);
            emit dataChanged(createIndex(idx, 0), createIndex(idx, 0));
        }
    }
    emit contentModified();
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

int SubtitleModel::processTiming(int leadIn, int leadOut, int gapThresh, double bias,
                                 bool selectedOnly, const QVariantList &selectedIndices,
                                 const QStringList &allowedStyles) {
    if (m_lines.empty()) return 0;
    QSet<int> selSet;
    for (const auto &v : selectedIndices) selSet.insert(v.toInt());

    int modifiedCount = 0;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        if (selectedOnly && !selSet.contains(static_cast<int>(i))) continue;
        auto &line = m_lines[i];
        if (!allowedStyles.isEmpty() && !allowedStyles.contains(line.style)) continue;

        int s = line.startMs;
        int e = line.endMs;

        if (leadIn > 0) s = std::max(0, s - leadIn);
        if (leadOut > 0) e = e + leadOut;

        if (gapThresh > 0 && i > 0) {
            auto &prevLine = m_lines[i - 1];
            int gap = s - prevLine.endMs;
            if (gap > 0 && gap <= gapThresh) {
                int mid = static_cast<int>(std::round(prevLine.endMs + gap * bias));
                prevLine.setEndMs(mid);
                s = mid;
            }
        }

        line.setStartMs(s);
        line.setEndMs(e);
        modifiedCount++;
    }

    if (modifiedCount > 0) {
        emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(m_lines.size() - 1), 0));
        emit contentModified();
    }
    return modifiedCount;
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

void SubtitleModel::splitLineAtFrame(int curIndex, int shift, int videoMs, double videoFps) {
    if (curIndex < 0 || curIndex >= static_cast<int>(m_lines.size())) return;
    const auto &item = m_lines[curIndex];
    int lineStart = item.startMs;
    int lineEnd = item.endMs;
    int frameDuration = (videoFps > 0) ? static_cast<int>(std::round(1000.0 / videoFps)) : 41;

    int part1End, part2Start;
    if (shift < 0) {
        part1End = std::max(lineStart, videoMs - 1);
        part2Start = videoMs;
    } else {
        part1End = videoMs + frameDuration;
        part2Start = part1End + 1;
    }

    if (part2Start >= lineEnd) {
        part2Start = std::max(lineStart + 10, lineEnd - 500);
        part1End = part2Start - 1;
    }

    SubtitleLine part2 = item;
    part2.setStartMs(part2Start);
    part2.setEndMs(lineEnd);

    m_lines[curIndex].setEndMs(part1End);

    beginInsertRows(QModelIndex(), curIndex + 1, curIndex + 1);
    m_lines.insert(m_lines.begin() + curIndex + 1, part2);
    endInsertRows();

    renumberLines();
    emit dataChanged(createIndex(curIndex, 0), createIndex(curIndex, 0));
    emit countChanged();
    emit contentModified();
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
        if (field == QStringLiteral("start")) {
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

        QString original = *targetStr;
        if (useRegex) {
            if (replaceAll) {
                *targetStr = original.replace(rx, replaceWith);
            } else {
                auto match = rx.match(original);
                if (match.hasMatch()) {
                    QString res = original;
                    res.replace(match.capturedStart(), match.capturedLength(), replaceWith);
                    *targetStr = res;
                }
            }
        } else {
            if (replaceAll) {
                *targetStr = original.replace(query, replaceWith, matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive);
            } else {
                int pos = original.indexOf(query, 0, matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive);
                if (pos >= 0) {
                    QString res = original;
                    res.replace(pos, query.length(), replaceWith);
                    *targetStr = res;
                }
            }
        }

        if (*targetStr != original) {
            line.updateCps();
            replacedCount++;
            emit dataChanged(createIndex(i, 0), createIndex(i, 0));
            if (!replaceAll) break;
        }
    }

    if (replacedCount > 0) {
        emit contentModified();
    }
    return replacedCount;
}

int SubtitleModel::findNext(const QString &query, const QVariantMap &options, int startIndex) {
    if (query.isEmpty() || m_lines.empty()) return -1;
    bool matchCase = options.value(QStringLiteral("matchCase"), false).toBool();
    bool useRegex = options.value(QStringLiteral("useRegex"), false).toBool();
    bool skipComments = options.value(QStringLiteral("skipComments"), true).toBool();
    QString field = options.value(QStringLiteral("field"), QStringLiteral("text")).toString();
    const int total = static_cast<int>(m_lines.size());
    int cur = std::clamp(startIndex, 0, total - 1);

    QRegularExpression rx;
    if (useRegex) {
        QRegularExpression::PatternOptions pOpts = QRegularExpression::NoPatternOption;
        if (!matchCase) pOpts |= QRegularExpression::CaseInsensitiveOption;
        rx = QRegularExpression(query, pOpts);
        if (!rx.isValid()) return -1;
    }

    for (int step = 0; step < total; ++step) {
        int i = (cur + step) % total;
        const auto &line = m_lines[i];
        if (skipComments && line.isComment) continue;

        const QString *targetStr = nullptr;
        if (field == QStringLiteral("text")) targetStr = &line.text;
        else if (field == QStringLiteral("style")) targetStr = &line.style;
        else if (field == QStringLiteral("actor")) targetStr = &line.actor;
        else if (field == QStringLiteral("effect")) targetStr = &line.effect;
        if (!targetStr) continue;

        if (useRegex) {
            if (rx.match(*targetStr).hasMatch()) return i;
        } else {
            if (targetStr->contains(query, matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive)) return i;
        }
    }
    return -1;
}
