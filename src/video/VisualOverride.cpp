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

#include "VisualToolBase.h"
#include "../automation/AssOverrideParser.h"

QString VisualToolBase::setOverrideTag(const QString &text, const QString &tag, const QString &val)
{
    QString removeTag;
    if (tag == "\\pos") removeTag = "\\move";
    else if (tag == "\\move") removeTag = "\\pos";
    else if (tag == "\\frz") removeTag = "\\fr";
    else if (tag == "\\fr") removeTag = "\\frz";
    else if (tag == "\\1c") removeTag = "\\c";
    else if (tag == "\\c") removeTag = "\\1c";
    else if (tag == "\\clip") removeTag = "\\iclip";
    else if (tag == "\\iclip") removeTag = "\\clip";

    // Check if line already starts with an override block {...}
    int firstBrace = text.indexOf('{');
    int closeBrace = text.indexOf('}');

    if (firstBrace == 0 && closeBrace > firstBrace &&
        (text.mid(1, closeBrace - 1).contains('\\') || closeBrace == 1)) {
        QString block = text.mid(1, closeBrace - 1);
        QString rest = text.mid(closeBrace + 1);

        bool balanced = false;
        const auto rawTags = Automation::AssOverrides::splitTopLevelTags(block, &balanced);
        // Appending to an unterminated transform would put the new static tag
        // inside it. Preserve malformed input instead of modifying its scope.
        if (!balanced) return text;
        block.clear();
        for (const auto &raw : rawTags) {
            const auto parsed = Automation::AssOverrides::parseTag(raw);
            if (parsed.name != tag && (removeTag.isEmpty() || parsed.name != removeTag)) block += raw;
        }

        // Append new tag at the end of the leading block
        block += tag + val;
        return QString("{%1}%2").arg(block, rest);
    } else {
        // No leading override block; prepend one
        return QString("{%1%2}%3").arg(tag, val, text);
    }
}
