// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT
#pragma once
#include <QColor>
#include <QMap>
#include <QRegularExpression>
#include <QString>
#include <QVector>

namespace SrtImport {
// Translate SubRip formatting with nested tag state, as SrtTagParser does upstream.
inline QString toAss(const QString &text) {
    static const QRegularExpression tags("<(/?[a-zA-Z]+)([^>]*)>");
    static const QRegularExpression attributes(R"re((\w+)\s*=\s*("[^"]*"|'[^']*'|[^\s>]+))re");
    QMap<QString,int> levels;
    using Font = QMap<QString,QString>;
    QVector<Font> fonts;
    QString result;
    qsizetype position = 0;
    auto matches = tags.globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += text.mid(position, match.capturedStart()-position);
        position = match.capturedEnd();
        QString name = match.captured(1).toLower();
        const bool closing = name.startsWith('/');
        if (closing) name.remove(0,1);
        if (name.size() == 1 && QStringLiteral("bius").contains(name)) {
            int &level = levels[name];
            if (!closing) {
                if (level++ == 0) result += "{\\" + name + "1}";
            } else if (level > 0 && --level == 0) result += "{\\" + name + "}";
        } else if (name == "font") {
            const Font before = fonts.isEmpty() ? Font{} : fonts.last();
            Font after = before;
            if (closing) {
                if (fonts.isEmpty()) continue;
                fonts.removeLast();
                after = fonts.isEmpty() ? Font{} : fonts.last();
            } else {
                auto attrs = attributes.globalMatch(match.captured(2));
                while (attrs.hasNext()) {
                    auto attr = attrs.next();
                    auto key = attr.captured(1).toLower();
                    auto value = attr.captured(2);
                    if (value.startsWith('"') || value.startsWith('\'')) value = value.mid(1,value.size()-2);
                    if (key == "face") after["fn"] = value;
                    else if (key == "size") after["fs"] = value;
                    else if (key == "color") {
                        const QColor color(value);
                        if (color.isValid()) after["c"] = QString("&H%1%2%3&")
                            .arg(color.blue(),2,16,QLatin1Char('0')).arg(color.green(),2,16,QLatin1Char('0'))
                            .arg(color.red(),2,16,QLatin1Char('0')).toUpper();
                    }
                }
                fonts.append(after);
            }
            for (const auto &key : {QString("fn"), QString("fs"), QString("c")})
                if (before.value(key) != after.value(key)) result += "{\\" + key + after.value(key) + "}";
        } else result += match.captured(0);
    }
    result += text.mid(position);
    result.replace("}{", "");
    return result;
}
}
