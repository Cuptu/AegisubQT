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
#include "VideoDisplayController.h"

QPointF VisualToolBase::toScriptCoords(const QPointF &pt) const
{
    if (!m_parent) return pt;
    const QRectF vRect = m_parent->videoRect();
    const QPointF res = m_parent->scriptResolution();
    if (vRect.width() <= 0.0 || vRect.height() <= 0.0) return pt;

    // (pt - video_pos) * script_res / video_size
    qreal sx = (pt.x() - vRect.left()) * res.x() / vRect.width();
    qreal sy = (pt.y() - vRect.top()) * res.y() / vRect.height();
    return QPointF(sx, sy);
}

QPointF VisualToolBase::fromScriptCoords(const QPointF &pt) const
{
    if (!m_parent) return pt;
    const QRectF vRect = m_parent->videoRect();
    const QPointF res = m_parent->scriptResolution();
    if (res.x() <= 0.0 || res.y() <= 0.0) return pt;

    // (pt * video_size / script_res) + video_pos
    qreal cx = (pt.x() * vRect.width() / res.x()) + vRect.left();
    qreal cy = (pt.y() * vRect.height() / res.y()) + vRect.top();
    return QPointF(cx, cy);
}

QString VisualToolBase::setOverrideTag(const QString &text, const QString &tag, const QString &val)
{
    QString removeTag;
    if (tag == "\\pos") removeTag = "\\move";
    else if (tag == "\\move") removeTag = "\\pos";
    else if (tag == "\\frz") removeTag = "\\fr";
    else if (tag == "\\1c") removeTag = "\\c";
    else if (tag == "\\clip") removeTag = "\\iclip";
    else if (tag == "\\iclip") removeTag = "\\clip";

    // Check if line already starts with an override block {...}
    int firstBrace = text.indexOf('{');
    int closeBrace = text.indexOf('}');

    if (firstBrace == 0 && closeBrace > firstBrace) {
        QString block = text.mid(1, closeBrace - 1);
        QString rest = text.mid(closeBrace + 1);

        // Strip existing target tag and mutually exclusive conflicts
        auto removeSpecificTag = [&](const QString &t) {
            if (t.isEmpty()) return;
            // Remove parenthesized tags: \pos(...), \move(...), \org(...), \clip(...)
            QString escaped = QRegularExpression::escape(t);
            QRegularExpression reParen(escaped + "\\s*\\([^\\)]*\\)");
            block.remove(reParen);
            // Remove numeric value tags: \frz<val>, \fr<val>, \fscx<val>
            QRegularExpression reValue(escaped + "[\\+\\-]?[0-9\\.]+");
            block.remove(reValue);
        };

        removeSpecificTag(tag);
        removeSpecificTag(removeTag);

        // Append new tag at the end of the leading block
        block += tag + val;
        return QString("{%1}%2").arg(block, rest);
    } else {
        // No leading override block; prepend one
        return QString("{%1%2}%3").arg(tag, val, text);
    }
}

bool VisualToolBase::getLinePosition(const QString &text, const QPointF &scriptRes, QPointF &pos)
{
    // Match \pos(X, Y)
    QRegularExpression rePos(R"(\\pos\s*\(\s*([-\d\.]+)\s*,\s*([-\d\.]+)\s*\))");
    auto match = rePos.match(text);
    if (match.hasMatch()) {
        pos = QPointF(match.captured(1).toDouble(), match.captured(2).toDouble());
        return true;
    }

    // Match \move(X1, Y1, ...)
    QRegularExpression reMove(R"(\\move\s*\(\s*([-\d\.]+)\s*,\s*([-\d\.]+)\s*,)");
    match = reMove.match(text);
    if (match.hasMatch()) {
        pos = QPointF(match.captured(1).toDouble(), match.captured(2).toDouble());
        return true;
    }

    // Fall back to default bottom-center position (\an2)
    pos = QPointF(scriptRes.x() / 2.0, scriptRes.y() - 30.0);
    return false;
}

bool VisualToolBase::getLineOrigin(const QString &text, QPointF &org)
{
    QRegularExpression reOrg(R"(\\org\s*\(\s*([-\d\.]+)\s*,\s*([-\d\.]+)\s*\))");
    auto match = reOrg.match(text);
    if (match.hasMatch()) {
        org = QPointF(match.captured(1).toDouble(), match.captured(2).toDouble());
        return true;
    }
    return false;
}

bool VisualToolBase::getLineMove(const QString &text, QPointF &p1, QPointF &p2, int &t1, int &t2)
{
    QRegularExpression reMove(R"(\\move\s*\(\s*([-\d\.]+)\s*,\s*([-\d\.]+)\s*,\s*([-\d\.]+)\s*,\s*([-\d\.]+)(?:\s*,\s*([-\d]+)\s*,\s*([-\d]+))?\s*\))");
    auto match = reMove.match(text);
    if (match.hasMatch()) {
        p1 = QPointF(match.captured(1).toDouble(), match.captured(2).toDouble());
        p2 = QPointF(match.captured(3).toDouble(), match.captured(4).toDouble());
        t1 = match.captured(5).isEmpty() ? 0 : match.captured(5).toInt();
        t2 = match.captured(6).isEmpty() ? 0 : match.captured(6).toInt();
        return true;
    }
    return false;
}

bool VisualToolBase::getLineRotation(const QString &text, double &rx, double &ry, double &rz)
{
    rx = ry = rz = 0.0;
    bool found = false;

    QRegularExpression reFrz(R"(\\frz([-\d\.]+))");
    auto match = reFrz.match(text);
    if (match.hasMatch()) {
        rz = match.captured(1).toDouble();
        found = true;
    } else {
        QRegularExpression reFr(R"(\\fr([-\d\.]+))");
        match = reFr.match(text);
        if (match.hasMatch()) {
            rz = match.captured(1).toDouble();
            found = true;
        }
    }

    QRegularExpression reFrx(R"(\\frx([-\d\.]+))");
    match = reFrx.match(text);
    if (match.hasMatch()) {
        rx = match.captured(1).toDouble();
        found = true;
    }

    QRegularExpression reFry(R"(\\fry([-\d\.]+))");
    match = reFry.match(text);
    if (match.hasMatch()) {
        ry = match.captured(1).toDouble();
        found = true;
    }

    return found;
}

bool VisualToolBase::getLineScale(const QString &text, double &sx, double &sy)
{
    sx = sy = 100.0;
    bool found = false;
    QRegularExpression reFscx(R"(\\fscx([-\d\.]+))");
    auto match = reFscx.match(text);
    if (match.hasMatch()) {
        sx = match.captured(1).toDouble();
        found = true;
    }
    QRegularExpression reFscy(R"(\\fscy([-\d\.]+))");
    match = reFscy.match(text);
    if (match.hasMatch()) {
        sy = match.captured(1).toDouble();
        found = true;
    }
    return found;
}

bool VisualToolBase::getLineClip(const QString &text, QPointF &p1, QPointF &p2, bool &inverse)
{
    inverse = false;
    QRegularExpression reIClip(R"(\\iclip\s*\(\s*([-\d\.]+)\s*,\s*([-\d\.]+)\s*,\s*([-\d\.]+)\s*,\s*([-\d\.]+)\s*\))");
    auto match = reIClip.match(text);
    if (match.hasMatch()) {
        inverse = true;
        p1 = QPointF(match.captured(1).toDouble(), match.captured(2).toDouble());
        p2 = QPointF(match.captured(3).toDouble(), match.captured(4).toDouble());
        return true;
    }

    QRegularExpression reClip(R"(\\clip\s*\(\s*([-\d\.]+)\s*,\s*([-\d\.]+)\s*,\s*([-\d\.]+)\s*,\s*([-\d\.]+)\s*\))");
    match = reClip.match(text);
    if (match.hasMatch()) {
        inverse = false;
        p1 = QPointF(match.captured(1).toDouble(), match.captured(2).toDouble());
        p2 = QPointF(match.captured(3).toDouble(), match.captured(4).toDouble());
        return true;
    }

    return false;
}
