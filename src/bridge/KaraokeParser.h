// Copyright (c) 2011, Thomas Goyne <plorkyeran@aegisub.org>
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
// Adapted from Aegisub ass_karaoke.cpp using the shared override parser.
#pragma once
#include "../automation/AssOverrideParser.h"
#include <libaegisub/ass/karaoke.h>
#include <QRegularExpression>
#include <climits>
#include <cstdint>

namespace KaraokeParser {
inline int checked(int64_t value) {
    if (value < INT_MIN || value > INT_MAX) throw std::range_error("Karaoke timing exceeds integer range");
    return static_cast<int>(value);
}
inline int integer(const QString &text) {
    static const QRegularExpression prefix("^[+-]?[0-9]+");
    const auto match = prefix.match(text.trimmed());
    if (!match.hasMatch()) return 0;
    bool ok;
    const auto value = match.captured().toLongLong(&ok);
    if (!ok) throw std::range_error("Karaoke parameter exceeds integer range");
    return checked(value);
}
inline std::vector<agi::ass::KaraokeSyllable> parse(const QString &text, int startTime) {
    using namespace Automation::AssOverrides;
    std::vector<agi::ass::KaraokeSyllable> result;
    agi::ass::KaraokeSyllable syllable;
    syllable.start_time = startTime;
    syllable.duration = 0;
    syllable.tag_type = "\\k";
    int drawingLevel = 0;
    auto overrideText = [&](const QString &part) { syllable.ovr_tags[syllable.text.size()] += part.toStdString(); };
    auto plain = [&](const QString &part) {
        if (drawingLevel) overrideText(part);
        else syllable.text += part.toStdString();
    };
    auto block = [&](const QString &body) {
        bool inTag = false;
        auto tagPart = [&](const QString &part) {
            const auto tag = parseTag(part);
            if (!tag.parameters.empty() && tag.name.startsWith("\\k", Qt::CaseInsensitive)) {
                if (inTag) { overrideText("}"); inTag = false; }
                if (syllable.duration > 0 || !syllable.text.empty()) {
                    result.push_back(syllable);
                    syllable.text.clear();
                    syllable.ovr_tags.clear();
                }
                syllable.tag_type = (tag.name == "\\K" ? QString("\\kf") : tag.name).toStdString();
                syllable.start_time = checked(int64_t(syllable.start_time) + syllable.duration);
                syllable.duration = tag.parameters[0].omitted ? 0 : checked(int64_t(integer(tag.parameters[0].value)) * 10);
            }
            else {
                if (!inTag) { overrideText("{"); inTag = true; }
                overrideText(serialize(tag));
                if (tag.name == "\\p" && !tag.parameters.empty())
                    drawingLevel = tag.parameters[0].omitted ? 0 : integer(tag.parameters[0].value);
            }
        };
        int depth = 0;
        qsizetype begin = 0;
        for (qsizetype i = 1; i < body.size(); ++i) {
            if (depth > 0) { if (body[i] == ')') --depth; }
            else if (body[i] == '\\') { tagPart(body.mid(begin, i - begin)); begin = i; }
            else if (body[i] == '(') ++depth;
        }
        if (!body.isEmpty()) tagPart(body.mid(begin));
        if (inTag) overrideText("}");
    };
    qsizetype pos = 0;
    while (pos < text.size()) {
        const auto open = text.indexOf('{', pos);
        if (open < 0) { plain(text.mid(pos)); break; }
        plain(text.mid(pos, open - pos));
        const auto close = text.indexOf('}', open);
        if (close < 0) { plain(text.mid(open)); break; }
        const auto body = text.mid(open + 1, close - open - 1);
        if (!body.isEmpty() && !body.contains('\\')) overrideText(text.mid(open, close - open + 1));
        else block(body);
        pos = close + 1;
    }
    checked(int64_t(syllable.start_time) + syllable.duration);
    result.push_back(std::move(syllable));
    return result;
}
}
