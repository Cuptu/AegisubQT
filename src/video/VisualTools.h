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

#include "VisualToolBase.h"
#include <QVariantList>
#include <QVariantMap>
#include <vector>

/// Crosshair coordinate inspection tool (Mode 0).
/// Displays live mouse coordinates in script space and commits \pos on double-click.
class VisualToolCross : public VisualToolBase {
public:
    explicit VisualToolCross(VideoDisplayController *parent);

    void mousePressed(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void mouseMoved(const QPointF &pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers mods) override;
    void mouseReleased(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void mouseDoubleClicked(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;

    Qt::CursorShape cursorShape() const override { return Qt::CrossCursor; }

    QString scriptCoordText() const;
};

/// Subtitle position and motion trajectory manipulation tool (Mode 1).
/// Manages drag handles for \pos, \move start/end points, and \org rotation centers.
class VisualToolDrag : public VisualToolBase {
public:
    enum PinType {
        DragStart = 0,  ///< Square handle: line anchor or \pos position
        DragEnd = 1,    ///< Circle handle: \move destination point
        DragOrigin = 2  ///< Triangle handle: \org rotation center
    };

    struct Pin {
        PinType type = DragStart;
        QPointF scriptPos;
        QPointF canvasPos;
        bool isHovered = false;
        bool isSelected = false;
    };

    explicit VisualToolDrag(VideoDisplayController *parent);

    void mousePressed(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void mouseMoved(const QPointF &pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers mods) override;
    void mouseReleased(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void mouseDoubleClicked(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;

    Qt::CursorShape cursorShape() const override;

    void refreshFeatures();
    void toggleMoveOrPos();

    QVariantList getPinsData() const;
    QVariantList getConnectingLines() const;

private:
    std::vector<Pin> m_pins;
    int m_activePinIndex = -1;
    QPointF m_dragStartMousePos;
    QPointF m_dragStartScriptPos;
    bool m_hasMove = false;
    int m_moveT1 = 0;
    int m_moveT2 = 0;
};

/// Z-axis rotation compass tool (Mode 2).
/// Manipulates subtitle angular orientation (\frz) around the origin (\org or position anchor).
class VisualToolRotateZ : public VisualToolBase {
public:
    explicit VisualToolRotateZ(VideoDisplayController *parent);

    void mousePressed(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void mouseMoved(const QPointF &pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers mods) override;
    void mouseReleased(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void mouseDoubleClicked(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;

    Qt::CursorShape cursorShape() const override;

    void refreshFeatures();
    QVariantMap getRotationData() const;

private:
    QPointF m_originScriptPos;
    QPointF m_originCanvasPos;
    QPointF m_posCanvasPos;
    double  m_radius = 60.0;
    double  m_angle = 0.0;
    double  m_origAngle = 0.0;
    double  m_startMouseAngle = 0.0;
};

/// Subtitle glyph scaling tool (Mode 4).
/// Adjusts horizontal (\fscx) and vertical (\fscy) scaling factors via drag gestures.
class VisualToolScale : public VisualToolBase {
public:
    explicit VisualToolScale(VideoDisplayController *parent);

    void mousePressed(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void mouseMoved(const QPointF &pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers mods) override;
    void mouseReleased(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void mouseDoubleClicked(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;

    Qt::CursorShape cursorShape() const override;

    void refreshFeatures();
    QVariantMap getScaleData() const;

private:
    QPointF m_posCanvas;
    double m_scaleX = 100.0;
    double m_scaleY = 100.0;
    double m_initScaleX = 100.0;
    double m_initScaleY = 100.0;
    QPointF m_dragStartPos;
};

/// Rectangular clipping tool (Mode 5).
/// Interactively creates and adjusts \clip and \iclip bounding boxes.
class VisualToolClip : public VisualToolBase {
public:
    explicit VisualToolClip(VideoDisplayController *parent);

    void mousePressed(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void mouseMoved(const QPointF &pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers mods) override;
    void mouseReleased(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void mouseDoubleClicked(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;

    Qt::CursorShape cursorShape() const override;

    void refreshFeatures();
    QVariantMap getClipData() const;

private:
    QPointF m_p1Script;
    QPointF m_p2Script;
    QPointF m_p1Canvas;
    QPointF m_p2Canvas;
    bool m_inverse = false;
    bool m_activeClip = false;
    int  m_activeCorner = -1; // -1: new box drag, 0: top-left, 1: top-right, 2: bottom-left, 3: bottom-right
};
