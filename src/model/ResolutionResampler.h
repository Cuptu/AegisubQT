// Copyright (c) 2013, Thomas Goyne <plorkyeran@aegisub.org>
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
// Adapted from Aegisub resolution_resampler.cpp for the Qt document model.
#pragma once
#include "SubtitleModel.h"
#include "../automation/AssOverrideParser.h"
#include <libaegisub/ycbcr_conv.h>
#include <QRegularExpression>
#include <cmath>
#include <climits>
#include <optional>

namespace ResolutionResampler {
struct Settings {
    int sourceX, sourceY, destX, destY;
    int margins[4]{};
    int mode = 0; // Stretch, AddBorder, RemoveBorder, Manual
    std::optional<std::pair<agi::ycbcr::header_colorspace, agi::ycbcr::header_colorspace>> matrix;
};
inline int integer(double value) {
    if (!std::isfinite(value) || value < INT_MIN || value > INT_MAX)
        throw std::range_error("Resampled value exceeds integer range");
    return static_cast<int>(value);
}
inline QString real(double value) {
    if (!std::isfinite(value)) throw std::range_error("Non-finite resampled value");
    auto text = QString::number(value, 'f', 3);
    while (text.endsWith('0')) text.chop(1);
    if (text.endsWith('.')) text.chop(1);
    return text;
}
inline double numericPrefix(const QString &text, bool integral = false) {
    static const QRegularExpression number("^[\\t ]*([+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?)");
    static const QRegularExpression intNumber("^[\\t ]*([+-]?[0-9]+)");
    const auto match = (integral ? intNumber : number).match(text);
    if (!match.hasMatch()) return 0;
    bool ok;
    const double value = match.captured(1).toDouble(&ok);
    if (!ok || !std::isfinite(value)) throw std::range_error("Invalid numeric override value");
    if (integral) integer(value);
    return value;
}
inline QString drawing(const QString &text, int shiftX, int shiftY, double scaleX, double scaleY) {
    bool x = true;
    QStringList result;
    for (const auto &token : text.split(' ', Qt::KeepEmptyParts)) {
        bool ok;
        double value = token.toDouble(&ok);
        // Upstream's Spirit parser consumes the full token without a skipper.
        // QString::toDouble alone would additionally accept padded whitespace.
        ok = ok && token == token.trimmed();
        if (ok) {
            result.append(real((value + (x ? shiftX : shiftY)) * (x ? scaleX : scaleY)));
            x = !x;
        }
        else if (token.size() == 1 && QStringLiteral("mnlbspc").contains(token.toLower())) {
            result.append(token.toLower());
            x = true;
        }
    }
    return result.join(' ');
}
inline void updateDrawingLevel(const QString &block, int &level) {
    // Only outer \p tags affect following text; transforms do not change the
    // dialogue block type in upstream AssDialogue::ParseTags.
    auto read = [&](const QString &part) {
        const auto tag = Automation::AssOverrides::parseTag(part);
        if (tag.name == "\\p") level = tag.parameters[0].omitted ? 0 : integer(numericPrefix(tag.parameters[0].value, true));
    };
    int depth = 0;
    qsizetype start = 0;
    for (qsizetype i = 1; i < block.size(); ++i) {
        if (depth > 0) { if (block[i] == ')') --depth; }
        else if (block[i] == '\\') { read(block.mid(start, i - start)); start = i; }
        else if (block[i] == '(') ++depth;
    }
    if (!block.isEmpty()) read(block.mid(start));
}
inline void transform(std::vector<SubtitleLine> &lines, QVariantList &styles, QVariantMap &info, Settings settings) {
    using namespace Automation::AssOverrides;
    if (settings.sourceX <= 0 || settings.sourceY <= 0 || settings.destX <= 0 || settings.destY <= 0 || settings.mode < 0 || settings.mode > 3)
        throw std::invalid_argument("Invalid resampling resolution or aspect ratio mode");
    double stretch = 1;
    double oldAR = double(settings.sourceX) / settings.sourceY;
    const double newAR = double(settings.destX) / settings.destY;
    bool horizontal = newAR > oldAR;
    if (std::abs(oldAR - newAR) / newAR > .01) {
        if (settings.mode == 1 || settings.mode == 2) {
            if (settings.mode == 2) horizontal = !horizontal;
            if (horizontal) settings.margins[0] = settings.margins[1] = integer((settings.sourceY * newAR - settings.sourceX) / 2);
            else settings.margins[2] = settings.margins[3] = integer((settings.sourceX / newAR - settings.sourceY) / 2);
        }
        else if (settings.mode == 0) stretch = newAR / oldAR;
        else {
            const double width = double(settings.sourceX) + settings.margins[0] + settings.margins[1];
            const double height = double(settings.sourceY) + settings.margins[2] + settings.margins[3];
            if (width <= 0 || height <= 0) throw std::invalid_argument("Margins remove the entire source resolution");
            oldAR = width / height;
            if (std::abs(oldAR - newAR) / newAR > .01) stretch = newAR / oldAR;
        }
    }
    const double width = double(settings.sourceX) + settings.margins[0] + settings.margins[1];
    const double height = double(settings.sourceY) + settings.margins[2] + settings.margins[3];
    if (width <= 0 || height <= 0) throw std::invalid_argument("Margins remove the entire source resolution");
    const double rx = settings.destX / width, ry = settings.destY / height;
    const double rm = rx == ry ? rx : std::sqrt(rx * ry);
    std::optional<agi::ycbcr_converter> converter;
    if (settings.matrix && settings.matrix->first != settings.matrix->second)
        converter.emplace(settings.matrix->first, settings.matrix->second);
    auto color = [&](const QString &text, bool style) {
        const auto bytes = text.toUtf8();
        const auto converted = converter->rgb_to_rgb(agi::Color(std::string_view(bytes.constData(), bytes.size())));
        return QString::fromStdString(style ? converted.GetAssStyleFormatted() : converted.GetAssOverrideFormatted());
    };
    Callback tags = [&](const QString &, Parameter &parameter) {
        double scale = 1;
        int shift = 0;
        switch (parameter.classification) {
        case AssParameterClass::ABSOLUTE_SIZE_X: scale = rx; break;
        case AssParameterClass::ABSOLUTE_SIZE_Y: scale = ry; break;
        case AssParameterClass::ABSOLUTE_SIZE_XY: scale = rm; break;
        case AssParameterClass::ABSOLUTE_POS_X: scale = rx; shift = settings.margins[0]; break;
        case AssParameterClass::ABSOLUTE_POS_Y: scale = ry; shift = settings.margins[2]; break;
        case AssParameterClass::RELATIVE_SIZE_X: scale = stretch; break;
        case AssParameterClass::RELATIVE_SIZE_Y: break;
        case AssParameterClass::DRAWING:
            parameter.value = drawing(parameter.value, settings.margins[0], settings.margins[2], rx, ry); return;
        case AssParameterClass::COLOR:
            if (converter) parameter.value = color(parameter.value, false); return;
        default: return;
        }
        const auto value = (numericPrefix(parameter.value, parameter.type == VariableDataType::INT) + shift) * scale;
        if (parameter.type == VariableDataType::INT) parameter.value = QString::number(integer(value + .5));
        else if (parameter.type == VariableDataType::FLOAT) parameter.value = real(value);
    };
    for (auto &item : styles) {
        auto style = item.toMap();
        auto value = [&](const char *key, double fallback) {
            bool ok;
            const double result = style.value(key, fallback).toDouble(&ok);
            if (!ok || !std::isfinite(result)) throw std::invalid_argument("Invalid style numeric value");
            return result;
        };
        style["size"] = integer(value("size", 20) * ry + .5);
        auto scaled = [&](double number, double scale) {
            const double result = number * scale;
            if (!std::isfinite(result)) throw std::range_error("Resampled style exceeds numeric range");
            return result;
        };
        for (const char *key : {"outlineWidth", "shadowDepth", "spacing"}) style[key] = scaled(value(key, 0), ry);
        style["scaleX"] = scaled(value("scaleX", 100), stretch);
        const char *keys[] = {"marginL", "marginR", "marginV"};
        for (int i = 0; i < 3; ++i) style[keys[i]] = integer((value(keys[i], 10) + settings.margins[i]) * (i < 2 ? rx : ry) + .5);
        if (converter) for (const char *key : {"primary", "secondary", "outline", "shadow"}) style[key] = color(style.value(key).toString(), true);
        item = style;
    }
    for (auto &line : lines) {
        if (line.isComment && (line.effect.startsWith("template") || line.effect.startsWith("code"))) continue;
        QString result;
        int drawingLevel = 0;
        qsizetype pos = 0;
        auto plain = [&](const QString &text) { result += drawingLevel ? drawing(text, 0, 0, rx / stretch, ry) : text; };
        while (pos < line.text.size()) {
            const auto open = line.text.indexOf('{', pos);
            if (open < 0) { plain(line.text.mid(pos)); break; }
            plain(line.text.mid(pos, open - pos));
            const auto close = line.text.indexOf('}', open);
            if (close < 0) { plain(line.text.mid(open)); break; }
            const auto block = line.text.mid(open + 1, close - open - 1);
            result += '{';
            if (block.contains('\\') || block.isEmpty()) {
                result += processBlock(block, tags);
                updateDrawingLevel(block, drawingLevel);
            }
            else result += block;
            result += '}';
            pos = close + 1;
        }
        line.text = result;
        int *margins[] = {&line.marginLeft, &line.marginRight, &line.marginVert};
        for (int i = 0; i < 3; ++i) if (*margins[i]) *margins[i] = integer((double(*margins[i]) + settings.margins[i]) * (i < 2 ? rx : ry) + .5);
        line.updateCps();
    }
    const int layoutX = info.value("LayoutResX").toInt(), layoutY = info.value("LayoutResY").toInt();
    if (layoutX > 0 && layoutY > 0) {
        const int newY = integer(layoutY + std::round(layoutY * (double(settings.margins[2]) + settings.margins[3]) / settings.sourceX));
        info["LayoutResY"] = newY;
        info["LayoutResX"] = integer(std::round(layoutX * (double(newY) / layoutY) * (double(settings.destX) / settings.destY) / (double(settings.sourceX) / settings.sourceY)));
    }
    info["PlayResX"] = settings.destX;
    info["PlayResY"] = settings.destY;
    if (settings.matrix) info["YCbCr Matrix"] = QString::fromStdString(agi::ycbcr::Header(settings.matrix->second).to_best_practice_string());
}
}
