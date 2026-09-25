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

#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QRegularExpression>
#include <cmath>
#include <algorithm>

class VideoDisplayController;

/// Abstract base class for interactive on-screen subtitle manipulation tools.
///
/// Handles coordinate space conversions between viewport display pixels
/// and script resolution pixels, and provides standard ASS override tag
/// extraction and serialization helpers.
class VisualToolBase {
public:
    explicit VisualToolBase(VideoDisplayController *parent)
        : m_parent(parent)
    {
    }

    virtual ~VisualToolBase() = default;

    /// Converts a point from viewport display coordinates (pixels) to ASS script coordinates.
    /// Formula: (point - video_pos) * script_res / video_size
    QPointF toScriptCoords(const QPointF &pt) const;

    /// Converts a point from ASS script coordinates to viewport display coordinates (pixels).
    /// Formula: (point * video_size / script_res) + video_pos
    QPointF fromScriptCoords(const QPointF &pt) const;

    // Mouse input dispatch
    virtual void mousePressed(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) = 0;
    virtual void mouseMoved(const QPointF &pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers mods) = 0;
    virtual void mouseReleased(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) = 0;
    virtual void mouseDoubleClicked(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) = 0;
    virtual void mouseLeft() { m_mousePos = QPointF(-1, -1); }

    // State and cursor feedback
    const QPointF& mousePos() const { return m_mousePos; }
    virtual Qt::CursorShape cursorShape() const { return Qt::ArrowCursor; }

    // ASS override tag parsing and injection helpers
    static QString setOverrideTag(const QString &text, const QString &tag, const QString &val);
    static bool getLinePosition(const QString &text, const QPointF &scriptRes, QPointF &pos);
    static bool getLineOrigin(const QString &text, QPointF &org);
    static bool getLineMove(const QString &text, QPointF &p1, QPointF &p2, int &t1, int &t2);
    static bool getLineRotation(const QString &text, double &rx, double &ry, double &rz);
    static bool getLineScale(const QString &text, double &sx, double &sy);
    static bool getLineClip(const QString &text, QPointF &p1, QPointF &p2, bool &inverse);

protected:
    VideoDisplayController *m_parent = nullptr;
    QPointF m_mousePos = QPointF(-1, -1);
    bool m_dragging = false;
    bool m_holding = false;
};
