// Copyright (c) 2005, Rodrigo Braz Monteiro
// Copyright (c) 2010, Thomas Goyne <plorkyeran@aegisub.org>
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
#include <QString>
#include <QStringList>
#include <functional>
#include <stdexcept>
#include <vector>

// Upstream override prototypes and optional-parameter semantics, without wx.
namespace Automation::AssOverrides {
enum class AssParameterClass {
	NORMAL,
	ABSOLUTE_SIZE_X,
	ABSOLUTE_SIZE_Y,
	ABSOLUTE_SIZE_XY,
	ABSOLUTE_POS_X,
	ABSOLUTE_POS_Y,
	RELATIVE_SIZE_X,
	RELATIVE_SIZE_Y,
	RELATIVE_TIME_START,
	RELATIVE_TIME_END,
	KARAOKE,
	DRAWING,
	ALPHA,
	COLOR
};

enum class VariableDataType {
	INT,
	FLOAT,
	TEXT,
	BOOL,
	BLOCK
};

enum AssParameterOptional {
	NOT_OPTIONAL = 0xFF,
	OPTIONAL_1 = 0x01,
	OPTIONAL_2 = 0x02,
	OPTIONAL_3 = 0x04,
	OPTIONAL_4 = 0x08,
	OPTIONAL_5 = 0x10,
	OPTIONAL_6 = 0x20,
	OPTIONAL_7 = 0x40
};

/// Prototype of a single override parameter
struct AssOverrideParamProto {
	/// ASS_ParameterOptional
	int optional;

	/// Type of this parameter
	VariableDataType type;

	/// Semantic type of this parameter
	AssParameterClass classification;
};

struct AssOverrideTagProto {
	/// Name of the tag, with slash
	QString name;

	/// Parameters to this tag
	std::vector<AssOverrideParamProto> params;

	typedef std::vector<AssOverrideTagProto>::iterator iterator;

	/// @brief Add a parameter to this tag prototype
	/// @param type Data type of the parameter
	/// @param classi Semantic type of the parameter
	/// @param opt Situations in which this parameter is present
	void AddParam(VariableDataType type, AssParameterClass classi = AssParameterClass::NORMAL, int opt = NOT_OPTIONAL) {
		params.push_back(AssOverrideParamProto{opt, type, classi});
	}

	/// @brief Convenience function for single-argument tags
	/// @param name Name of the tag, with slash
	/// @param type Data type of the parameter
	/// @param classi Semantic type of the parameter
	/// @param opt Situations in which this parameter is present
	void Set(const char *name, VariableDataType type, AssParameterClass classi = AssParameterClass::NORMAL, int opt = NOT_OPTIONAL) {
		this->name = name;
		params.push_back(AssOverrideParamProto{opt, type, classi});
	}
};

inline const std::vector<AssOverrideTagProto> &prototypes() {
    static const auto table = [] {
    std::vector<AssOverrideTagProto> proto;

	proto.resize(56);
	int i = 0;

	// Longer tag names must appear before shorter tag names

	proto[0].Set("\\alpha", VariableDataType::TEXT, AssParameterClass::ALPHA); // \alpha&H<aa>&

	// FIXME: convert \bord and \shad to \xbord\ybord and \xshad\yshad during anamorphic resampling
	proto[++i].Set("\\bord", VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_SIZE_Y); // \bord<depth>
	proto[++i].Set("\\xbord", VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_SIZE_X); // \xbord<depth>
	proto[++i].Set("\\ybord", VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_SIZE_Y); // \ybord<depth>
	proto[++i].Set("\\shad", VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_SIZE_Y); // \shad<depth>
	proto[++i].Set("\\xshad", VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_SIZE_X); // \xshad<depth>
	proto[++i].Set("\\yshad", VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_SIZE_Y); // \yshad<depth>

	// \fade(<a1>,<a2>,<a3>,<t1>,<t2>,<t3>,<t4>)
	i++;
	proto[i].name = "\\fade";
	proto[i].AddParam(VariableDataType::INT);
	proto[i].AddParam(VariableDataType::INT);
	proto[i].AddParam(VariableDataType::INT);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::RELATIVE_TIME_START);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::RELATIVE_TIME_START);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::RELATIVE_TIME_START);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::RELATIVE_TIME_START);

	// \move(<x1>,<y1>,<x2>,<y2>[,<t1>,<t2>])
	i++;
	proto[i].name = "\\move";
	proto[i].AddParam(VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_POS_X);
	proto[i].AddParam(VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_POS_Y);
	proto[i].AddParam(VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_POS_X);
	proto[i].AddParam(VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_POS_Y);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::RELATIVE_TIME_START);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::RELATIVE_TIME_START);

	// If these are rearranged, keep rect clip and vector clip adjacent in this order
	// \clip(<x1>,<y1>,<x2>,<y2>)
	i++;
	proto[i].name = "\\clip";
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::ABSOLUTE_POS_X);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::ABSOLUTE_POS_Y);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::ABSOLUTE_POS_X);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::ABSOLUTE_POS_Y);

	// \clip([<scale>,]<some drawings>)
	i++;
	proto[i].name = "\\clip";
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::NORMAL,OPTIONAL_2);
	proto[i].AddParam(VariableDataType::TEXT, AssParameterClass::DRAWING);

	// \iclip(<x1>,<y1>,<x2>,<y2>)
	i++;
	proto[i].name = "\\iclip";
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::ABSOLUTE_POS_X);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::ABSOLUTE_POS_Y);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::ABSOLUTE_POS_X);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::ABSOLUTE_POS_Y);

	// \iclip([<scale>,]<some drawings>)
	i++;
	proto[i].name = "\\iclip";
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::NORMAL,OPTIONAL_2);
	proto[i].AddParam(VariableDataType::TEXT, AssParameterClass::DRAWING);

	proto[++i].Set("\\fscx", VariableDataType::FLOAT, AssParameterClass::RELATIVE_SIZE_X); // \fscx<percent>
	proto[++i].Set("\\fscy", VariableDataType::FLOAT, AssParameterClass::RELATIVE_SIZE_Y); // \fscy<percent>
	// \pos(<x>,<y>)
	i++;
	proto[i].name = "\\pos";
	proto[i].AddParam(VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_POS_X);
	proto[i].AddParam(VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_POS_Y);

	// \org(<x>,<y>)
	i++;
	proto[i].name = "\\org";
	proto[i].AddParam(VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_POS_X);
	proto[i].AddParam(VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_POS_Y);

	proto[++i].Set("\\pbo", VariableDataType::INT, AssParameterClass::ABSOLUTE_SIZE_Y); // \pbo<y>
	// \fad(<t1>,<t2>)
	i++;
	proto[i].name = "\\fad";
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::RELATIVE_TIME_START);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::RELATIVE_TIME_END);

	proto[++i].Set("\\fsp", VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_SIZE_Y); // \fsp<pixels> (affected by \fscx)
	proto[++i].Set("\\frx", VariableDataType::FLOAT); // \frx<degrees>
	proto[++i].Set("\\fry", VariableDataType::FLOAT); // \fry<degrees>
	proto[++i].Set("\\frz", VariableDataType::FLOAT); // \frz<degrees>
	proto[++i].Set("\\fr", VariableDataType::FLOAT); // \fr<degrees>
	proto[++i].Set("\\fax", VariableDataType::FLOAT); // \fax<factor>
	proto[++i].Set("\\fay", VariableDataType::FLOAT); // \fay<factor>
	proto[++i].Set("\\1c", VariableDataType::TEXT, AssParameterClass::COLOR); // \1c&H<bbggrr>&
	proto[++i].Set("\\2c", VariableDataType::TEXT, AssParameterClass::COLOR); // \2c&H<bbggrr>&
	proto[++i].Set("\\3c", VariableDataType::TEXT, AssParameterClass::COLOR); // \3c&H<bbggrr>&
	proto[++i].Set("\\4c", VariableDataType::TEXT, AssParameterClass::COLOR); // \4c&H<bbggrr>&
	proto[++i].Set("\\1a", VariableDataType::TEXT, AssParameterClass::ALPHA); // \1a&H<aa>&
	proto[++i].Set("\\2a", VariableDataType::TEXT, AssParameterClass::ALPHA); // \2a&H<aa>&
	proto[++i].Set("\\3a", VariableDataType::TEXT, AssParameterClass::ALPHA); // \3a&H<aa>&
	proto[++i].Set("\\4a", VariableDataType::TEXT, AssParameterClass::ALPHA); // \4a&H<aa>&
	proto[++i].Set("\\fe", VariableDataType::TEXT); // \fe<charset>
	proto[++i].Set("\\ko", VariableDataType::INT, AssParameterClass::KARAOKE); // \ko<duration>
	proto[++i].Set("\\kf", VariableDataType::INT, AssParameterClass::KARAOKE); // \kf<duration>
	proto[++i].Set("\\be", VariableDataType::INT); // \be<strength>
	proto[++i].Set("\\blur", VariableDataType::FLOAT); // \blur<strength>
	proto[++i].Set("\\fn", VariableDataType::TEXT); // \fn<name>
	proto[++i].Set("\\fs+", VariableDataType::FLOAT); // \fs+<size>
	proto[++i].Set("\\fs-", VariableDataType::FLOAT); // \fs-<size>
	proto[++i].Set("\\fs", VariableDataType::FLOAT, AssParameterClass::ABSOLUTE_SIZE_Y); // \fs<size>
	proto[++i].Set("\\an", VariableDataType::INT); // \an<alignment>
	proto[++i].Set("\\c", VariableDataType::TEXT, AssParameterClass::COLOR); // \c&H<bbggrr>&
	proto[++i].Set("\\b", VariableDataType::INT); // \b<0/1/weight>
	proto[++i].Set("\\i", VariableDataType::BOOL); // \i<0/1>
	proto[++i].Set("\\u", VariableDataType::BOOL); // \u<0/1>
	proto[++i].Set("\\s", VariableDataType::BOOL); // \s<0/1>
	proto[++i].Set("\\a", VariableDataType::INT); // \a<alignment>
	proto[++i].Set("\\k", VariableDataType::INT, AssParameterClass::KARAOKE); // \k<duration>
	proto[++i].Set("\\K", VariableDataType::INT, AssParameterClass::KARAOKE); // \K<duration>
	proto[++i].Set("\\q", VariableDataType::INT); // \q<0-3>
	proto[++i].Set("\\p", VariableDataType::INT); // \p<n>
	proto[++i].Set("\\r", VariableDataType::TEXT); // \r[<name>]

	// \t([<t1>,<t2>,][<accel>,]<style modifiers>)
	i++;
	proto[i].name = "\\t";
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::RELATIVE_TIME_START,OPTIONAL_3 | OPTIONAL_4);
	proto[i].AddParam(VariableDataType::INT, AssParameterClass::RELATIVE_TIME_START,OPTIONAL_3 | OPTIONAL_4);
	proto[i].AddParam(VariableDataType::FLOAT, AssParameterClass::NORMAL,OPTIONAL_2 | OPTIONAL_4);
	proto[i].AddParam(VariableDataType::BLOCK);
    return proto;
    }();
    return table;
}


struct Parameter {
    QString value;
    VariableDataType type;
    AssParameterClass classification;
    bool omitted = true;
};
struct Tag {
    QString name;
    std::vector<Parameter> parameters;
};
// Preserve raw spelling while separating only top-level tags. Nested transform
// arguments belong to their parent tag, including unknown/extended parameters.
inline QStringList splitTopLevelTags(const QString &body, bool *balanced = nullptr) {
    QStringList result;
    int depth = 0, start = 0;
    bool valid = true;
    for (int i = 0; i < body.size(); ++i) {
        if (body[i] == '(') ++depth;
        else if (body[i] == ')') { if (depth) --depth; else valid = false; }
        else if (body[i] == '\\' && !depth) {
            if (i > start) result.append(body.mid(start, i - start));
            start = i;
        }
    }
    if (start < body.size()) result.append(body.mid(start));
    if (balanced) *balanced = valid && depth == 0;
    return result;
}
inline QStringList tokenize(QString text) {
    QStringList parameters;
    if (text.isEmpty()) return parameters;
    if (!text.startsWith('(')) return {text.trimmed()};
    qsizetype i = 0;
    int depth = 1;
    while (i < text.size() && depth > 0) {
        const auto start = ++i;
        while (i < text.size() && depth > 0) {
            const auto c = text[i];
            if (c == ',' && depth == 1) break;
            if (c == '(') ++depth;
            else if (c == ')' && --depth == 0) break;
            ++i;
        }
        parameters.append(text.mid(start, i - start).trimmed());
    }
    if (i + 1 < text.size()) parameters.append(text.mid(i + 1));
    return parameters;
}
inline Tag parseTag(const QString &text) {
    const auto &table = prototypes();
    for (auto it = table.begin(); it != table.end(); ++it) {
        if (!text.startsWith(it->name)) continue;
        Tag tag{it->name, {}};
        const auto values = tokenize(text.mid(it->name.size()));
        // Original code shifts by total-1 even for zero/huge argument counts.
        // Keep its optional masks while making the shift defined.
        const unsigned mask = values.isEmpty() || values.size() > 8 ? 0 : 1u << (values.size() - 1);
        if ((tag.name == "\\clip" || tag.name == "\\iclip") && values.size() != 4) ++it;
        qsizetype current = 0;
        for (const auto &parameter : it->params) {
            Parameter value{{}, parameter.type, parameter.classification, true};
            if ((parameter.optional & mask) && current < values.size()) {
                value.value = values[current++];
                value.omitted = false;
            }
            tag.parameters.push_back(std::move(value));
        }
        return tag;
    }
    return {text, {}};
}
inline QString serialize(const Tag &tag) {
    QStringList values;
    for (const auto &parameter : tag.parameters) if (!parameter.omitted) values.append(parameter.value);
    const auto joined = values.join(',');
    return tag.parameters.size() > 1 ? tag.name + '(' + joined + ')' : tag.name + joined;
}
using Callback = std::function<void(const QString &, Parameter &)>;
inline QString processBlock(const QString &text, const Callback &callback, int level = 0) {
    if (level > 64) throw std::range_error("Override nesting exceeds supported depth");
    QString result;
    auto process = [&](const QString &part) {
        auto tag = parseTag(part);
        for (auto &parameter : tag.parameters) {
            if (parameter.omitted) continue;
            callback(tag.name, parameter);
            if (parameter.type == VariableDataType::BLOCK)
                parameter.value = processBlock(parameter.value, callback, level + 1);
        }
        result += serialize(tag);
    };
    int depth = 0;
    qsizetype start = 0;
    for (qsizetype i = 1; i < text.size(); ++i) {
        if (depth > 0) { if (text[i] == ')') --depth; }
        else if (text[i] == '\\') { process(text.mid(start, i - start)); start = i; }
        else if (text[i] == '(') ++depth;
    }
    if (!text.isEmpty()) process(text.mid(start));
    return result;
}
inline QString processText(const QString &text, const Callback &callback) {
    QString result;
    qsizetype pos = 0;
    while (pos < text.size()) {
        const auto open = text.indexOf('{', pos);
        if (open < 0) { result += text.mid(pos); break; }
        result += text.mid(pos, open - pos);
        const auto close = text.indexOf('}', open);
        if (close < 0) { result += text.mid(open); break; }
        const auto block = text.mid(open + 1, close - open - 1);
        result += '{';
        result += block.contains('\\') || block.isEmpty() ? processBlock(block, callback) : block;
        result += '}';
        pos = close + 1;
    }
    return result;
}
}
