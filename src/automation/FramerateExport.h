// Copyright (c) 2005, Rodrigo Braz Monteiro
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
#include "AssOverrideParser.h"
#include "LuaAssFileBridge.h"
#include <libaegisub/vfr.h>
#include <libaegisub/ass/time.h>
#include <QRegularExpression>
#include <cmath>
#include <limits>

namespace Automation::FramerateExport {
inline int checked(qint64 value) {
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
        throw std::range_error("Transformed subtitle time exceeds supported range");
    return static_cast<int>(value);
}
inline int parameterInt(const QString &value) {
    // Match upstream atoi prefix semantics, but reject overflowing time values.
    static const QRegularExpression prefix(QStringLiteral(R"(^\s*([+-]?\d+))"));
    const auto match = prefix.match(value);
    if (!match.hasMatch()) return 0;
    bool ok = false;
    const auto number = match.captured(1).toLongLong(&ok);
    if (!ok) throw std::range_error("Override time parameter exceeds supported range");
    return checked(number);
}
inline int convertTime(int time, const agi::vfr::Framerate &input, const agi::vfr::Framerate &output) {
    const int frame = output.FrameAtTime(time);
    const int nextFrame = checked(static_cast<qint64>(frame) + 1);
    const int frameStart = output.TimeAtFrame(frame);
    const int frameEnd = output.TimeAtFrame(nextFrame);
    const qint64 duration = static_cast<qint64>(frameEnd) - frameStart;
    if (duration <= 0) throw std::range_error("Frame duration must be positive");
    const double distance = static_cast<double>(static_cast<qint64>(time) - frameStart) / duration;
    const int newStart = input.TimeAtFrame(frame);
    const int newEnd = input.TimeAtFrame(nextFrame);
    const double result = newStart + (static_cast<qint64>(newEnd) - newStart) * distance;
    if (!std::isfinite(result) || result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max())
        throw std::range_error("Transformed subtitle time exceeds supported range");
    return static_cast<int>(result);
}
inline bool transform(std::vector<AssEntryData> &lines, const agi::vfr::Framerate &input,
                      const agi::vfr::Framerate &output, QString &error) {
    using namespace AssOverrides;
    error.clear();
    if (!input.IsLoaded() || !output.IsLoaded()) return true; // Upstream no-op.
    try {
        auto converted = lines;
        for (auto &line : converted) {
            if (line.entryClass != AssEntryClass::Dialogue) continue;
            // AssDialogue uses agi::Time, which rounds input times to centiseconds.
            const int oldStart = agi::Time(line.startTime), oldEnd = agi::Time(line.endTime);
            const int newStart = convertTime(oldStart, input, output) / 10 * 10;
            const int newEnd = checked(static_cast<qint64>(convertTime(oldEnd, input, output)) + 9) / 10 * 10;
            qint64 oldK = 0, newK = 0;
            line.text = processText(line.text, [&](const QString &, Parameter &parameter) {
                if (parameter.type != VariableDataType::INT && parameter.type != VariableDataType::FLOAT) return;
                if (parameter.classification != AssParameterClass::RELATIVE_TIME_START &&
                    parameter.classification != AssParameterClass::RELATIVE_TIME_END &&
                    parameter.classification != AssParameterClass::KARAOKE) return;
                const int value = parameterInt(parameter.value);
                int result = 0;
                switch (parameter.classification) {
                case AssParameterClass::RELATIVE_TIME_START:
                    result = checked(static_cast<qint64>(convertTime(checked(static_cast<qint64>(oldStart / 10 * 10) + value), input, output)) - newStart);
                    if (result == 0 && value != 0) result = 1;
                    break;
                case AssParameterClass::RELATIVE_TIME_END:
                    result = checked(static_cast<qint64>(newEnd) - convertTime(checked(static_cast<qint64>(oldEnd / 10 * 10) - value), input, output));
                    break;
                case AssParameterClass::KARAOKE:
                    result = checked((static_cast<qint64>(convertTime(checked((oldStart / 10 + oldK + value) * 10), input, output)) - newStart) / 10 - newK);
                    oldK += value;
                    newK += result;
                    break;
                default: return;
                }
                parameter.value = QString::number(result);
            });
            line.startTime = agi::Time(newStart);
            line.endTime = agi::Time(newEnd);
        }
        lines.swap(converted);
        return true;
    } catch (const std::exception &exception) {
        error = QString::fromUtf8(exception.what());
        return false;
    }
}
}
