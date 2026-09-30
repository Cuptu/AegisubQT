// Copyright (c) 2006, Rodrigo Braz Monteiro
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

#include "SubtitleModel.h"
#include <algorithm>
#include <list>

// Adapted from upstream SubtitleFormat::RecombineOverlaps/MergeIdentical and
// SRTSubtitleFormat::ConvertTags. Work only on copies of the document rows.
namespace SrtExport {
struct Cue { int start; int end; QString text; };

inline bool integerNonzero(QString value) {
    value = value.trimmed();
    if (value.startsWith('(')) {
        value = value.mid(1);
        const auto comma = value.indexOf(',');
        const auto close = value.indexOf(')');
        const auto end = comma < 0 ? close : close < 0 ? comma : std::min(comma, close);
        if (end >= 0) value.truncate(end);
        value = value.trimmed();
    }
    qsizetype pos = 0;
    if (value.startsWith('+') || value.startsWith('-')) ++pos;
    bool nonzero = false;
    for (; pos < value.size() && value[pos] >= '0' && value[pos] <= '9'; ++pos)
        nonzero |= value[pos] != '0';
    return nonzero;
}

inline QString convertTags(QString text, const QString &newline) {
    QString result;
    const char names[] = {'b', 'i', 's', 'u'};
    bool enabled[] = {false, false, false, false};
    bool drawing = false;
    auto tag = [&](const QString &part) {
        if (!part.startsWith('\\') || part.size() < 2) return;
        // Longer upstream prototypes precede the one-character prototypes.
        static const QStringList longer{QStringLiteral("\\bord"), QStringLiteral("\\be"),
            QStringLiteral("\\blur"), QStringLiteral("\\iclip"), QStringLiteral("\\shad"),
            QStringLiteral("\\pbo"), QStringLiteral("\\pos")};
        for (const auto &name : longer) if (part.startsWith(name)) return;
        const auto value = integerNonzero(part.mid(2));
        if (part[1] == 'p') { drawing = value; return; }
        for (int i = 0; i < 4; ++i) {
            if (part[1] != QLatin1Char(names[i])) continue;
            if (value != enabled[i]) {
                result += value ? QStringLiteral("<%1>").arg(QLatin1Char(names[i]))
                                : QStringLiteral("</%1>").arg(QLatin1Char(names[i]));
                enabled[i] = value;
            }
        }
    };
    for (qsizetype pos = 0; pos < text.size();) {
        if (text[pos] == '{') {
            const auto end = text.indexOf('}', pos);
            if (end >= 0) {
                const auto block = text.mid(pos + 1, end - pos - 1);
                qsizetype start = 0;
                int depth = 0;
                for (qsizetype i = 1; i < block.size(); ++i) {
                    if (depth > 0) {
                        if (block[i] == ')') --depth;
                    } else if (block[i] == '\\') {
                        tag(block.mid(start, i - start));
                        start = i;
                    } else if (block[i] == '(') ++depth;
                }
                tag(block.mid(start));
                pos = end + 1;
                continue;
            }
        }
        const auto end = text.indexOf('{', pos + 1);
        const auto count = end < 0 ? text.size() - pos : end - pos;
        if (!drawing) {
            // The SRT reader escapes literal backslashes in its ASS model.
            // Decode pairs before considering ASS line breaks, so a literal
            // SRT "\N" survives save/reopen instead of becoming a new line.
            for (qsizetype i = pos; i < pos + count; ++i) {
                if (text[i] == '\\' && i + 1 < pos + count) {
                    const auto ch = text[i + 1];
                    if (ch == '\\') { result += '\\'; ++i; continue; }
                    if (ch == 'N' || ch == 'n') { result += newline; ++i; continue; }
                    if (ch == 'h') { result += ' '; ++i; continue; }
                }
                result += text[i];
            }
        }
        pos += count;
    }
    for (int i = 0; i < 4; ++i)
        if (enabled[i]) result += QStringLiteral("</%1>").arg(QLatin1Char(names[i]));
    return result;
}

inline std::list<Cue> convert(const std::vector<SubtitleLine> &rows) {
    std::list<Cue> cues;
    for (const auto &row : rows)
        if (!row.isComment && !row.text.isEmpty()) cues.push_back({row.startMs, row.endMs, row.text});
    cues.sort([](const Cue &a, const Cue &b) { return a.start < b.start; });
    if (cues.empty()) return cues;
    auto cur = cues.begin();
    auto next = std::next(cur);
    while (next != cues.end()) {
        if (next == cues.begin() || cur->end <= next->start) { cur = next++; continue; }
        const auto a = *cur, b = *next;
        cues.erase(cur);
        next = cues.erase(next);
        auto insert = [&](Cue cue) {
            cues.insert(std::find_if(next, cues.end(), [&](const Cue &pos) { return pos.start >= cue.start; }), std::move(cue));
        };
        if (b.start > a.start) insert({a.start, b.start, a.text});
        insert({b.start, std::min(a.end, b.end), b.text + QStringLiteral("\\N") + a.text});
        if (a.end > b.end) insert({b.end, a.end, a.text});
        if (b.end > a.end) insert({a.end, b.end, b.text});
        if (next != cues.begin()) cur = std::prev(next);
    }
    cur = cues.begin();
    next = std::next(cur);
    while (next != cues.end()) {
        if (cur->end == next->start && cur->text == next->text) {
            next->start = std::min(cur->start, next->start);
            next->end = std::max(cur->end, next->end);
            cues.erase(cur);
        }
        cur = next++;
    }
    return cues;
}
}
