// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT
#pragma once
#include "../automation/AssOverrideParser.h"
#include <QFont>
#include <QVariantMap>
#include <functional>

namespace InlineFormatting {
struct Block { int start, end; QString body; bool override; };
inline QList<Block> blocks(const QString &text) {
    QList<Block> result;
    for (int pos = 0; pos < text.size();) {
        const int open = text.indexOf('{', pos), close = text.indexOf('}', open + 1);
        if (open < 0 || close < 0) break;
        const auto body = text.mid(open + 1, close - open - 1);
        result.append({open, close + 1, body, body.contains('\\') || body.isEmpty()});
        pos = close + 1;
    }
    return result;
}
// Split only top-level tags: transform arguments are not current static state.
inline QStringList tags(const QString &body) {
    return Automation::AssOverrides::splitTopLevelTags(body);
}
inline QVariantMap defaults(const QVariantMap &style) {
    return {{"font",style.value("font", "Arial")}, {"size",style.value("size",20.0)},
        {"b",style.value("bold",false)}, {"i",style.value("italic",false)},
        {"u",style.value("underline",false)}, {"s",style.value("strikeout",false)}};
}
inline QVariantMap state(const QString &text, int position, const QVariantMap &style,
                         const std::function<QVariantMap(QString)> &findStyle) {
    auto base = defaults(style), result = base;
    for (const auto &block : blocks(text)) {
        if (block.start > position) break;
        if (!block.override) continue;
        for (const auto &raw : tags(block.body)) {
            const auto tag = Automation::AssOverrides::parseTag(raw);
            const auto value = tag.parameters.empty() || tag.parameters[0].omitted ? QString() : tag.parameters[0].value;
            if (tag.name == "\\r") {
                const auto reset = value.isEmpty() ? style : findStyle(value);
                base = defaults(reset.isEmpty() ? style : reset);
                result = base;
            } else if (tag.name == "\\fn") result["font"] = value.isEmpty() ? base.value("font") : value;
            else if (tag.name == "\\fs") {
                bool valid = false;
                const double size = value.toDouble(&valid);
                if (valid && size > 0) result["size"] = size;
                else if (value.isEmpty()) result["size"] = base.value("size");
            } else if (tag.name == "\\b" || tag.name == "\\i" || tag.name == "\\u" || tag.name == "\\s") {
                bool valid = false;
                const int number = value.toInt(&valid);
                const auto key = tag.name.mid(1);
                if (valid) result[key] = number != 0;
                else if (value.isEmpty()) result[key] = base.value(key);
            }
        }
    }
    return result;
}
inline int plainPosition(const QString &text, int rawPosition) {
    int result = qBound(0, rawPosition, int(text.size()));
    for (const auto &block : blocks(text)) {
        if (block.start >= rawPosition) break;
        result -= qMin(rawPosition, block.end) - block.start;
    }
    return result;
}
inline int rawPosition(const QString &text, int plainPosition) {
    int pos = qMax(0, plainPosition);
    for (const auto &block : blocks(text)) if (block.start <= pos) pos += block.end - block.start;
    return qMin(pos, int(text.size()));
}
struct Edit {
    int start = 0, removed = 0, added = 0;
    int map(int position, bool after) const {
        if (position < start || (position == start && !after)) return position;
        if (position >= start + removed) return position + added - removed;
        return start + added;
    }
};
inline Edit put(QString &text, int position, const QString &name, const QString &value) {
    position = qBound(0, position, int(text.size()));
    const auto parts = blocks(text);
    int drawing = 0;
    const Block *previousOverride = nullptr, *target = nullptr;
    for (const auto &block : parts) {
        if (block.start > position) break;
        if (block.override) {
            previousOverride = &block;
            for (const auto &raw : tags(block.body)) {
                const auto tag = Automation::AssOverrides::parseTag(raw);
                if (tag.name == "\\p" && !tag.parameters.empty()) drawing = tag.parameters[0].value.toInt();
            }
        }
        if (position <= block.end) {
            if (block.override) target = &block;
            else position = block.start; // Insert before comments, never inside them.
            break;
        }
    }
    if (!target && drawing) target = previousOverride; // Do not split drawing commands.
    if (target) {
        QString body;
        for (const auto &raw : tags(target->body))
            if (Automation::AssOverrides::parseTag(raw).name != name) body += raw;
        body += name + value;
        const QString replacement = '{' + body + '}';
        const Edit edit{target->start, target->end - target->start, int(replacement.size())};
        text.replace(edit.start, edit.removed, replacement);
        return edit;
    }
    const QString replacement = '{' + name + value + '}';
    text.insert(position, replacement);
    return {position, 0, int(replacement.size())};
}
inline QFont font(const QVariantMap &state) {
    QFont result(state.value("font").toString());
    result.setPointSizeF(state.value("size").toDouble());
    result.setBold(state.value("b").toBool());
    result.setItalic(state.value("i").toBool());
    result.setUnderline(state.value("u").toBool());
    result.setStrikeOut(state.value("s").toBool());
    return result;
}
}
