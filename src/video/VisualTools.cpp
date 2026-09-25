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

#include "VisualTools.h"
#include "VideoDisplayController.h"
#include <cmath>

static constexpr double kRad2Deg = 180.0 / 3.14159265358979323846;
static constexpr double kDeg2Rad = 3.14159265358979323846 / 180.0;

// VisualToolCross: Crosshair coordinate reader

VisualToolCross::VisualToolCross(VideoDisplayController *parent)
    : VisualToolBase(parent)
{
}

void VisualToolCross::mousePressed(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(button);
    Q_UNUSED(mods);
    m_mousePos = pos;
}

void VisualToolCross::mouseMoved(const QPointF &pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(buttons);
    Q_UNUSED(mods);
    m_mousePos = pos;
}

void VisualToolCross::mouseReleased(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos);
    Q_UNUSED(button);
    Q_UNUSED(mods);
}

void VisualToolCross::mouseDoubleClicked(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods);
    if (button != Qt::LeftButton || !m_parent) return;

    QPointF scriptCoord = toScriptCoords(pos);
    int sx = static_cast<int>(std::round(scriptCoord.x()));
    int sy = static_cast<int>(std::round(scriptCoord.y()));

    QString text = m_parent->activeSubtitleText();
    QString newText = setOverrideTag(text, "\\pos", QString("(%1,%2)").arg(sx).arg(sy));
    m_parent->updateLiveSubtitleText(newText);
    m_parent->commitChanges("positioning");
}

QString VisualToolCross::scriptCoordText() const
{
    if (m_mousePos.x() < 0 || m_mousePos.y() < 0) return QString();
    QPointF s = toScriptCoords(m_mousePos);
    return QString("%1, %2").arg(static_cast<int>(std::round(s.x()))).arg(static_cast<int>(std::round(s.y())));
}

// VisualToolDrag: Subtitle position, motion vector, and rotation origin manipulator

VisualToolDrag::VisualToolDrag(VideoDisplayController *parent)
    : VisualToolBase(parent)
{
    refreshFeatures();
}

void VisualToolDrag::refreshFeatures()
{
    m_pins.clear();
    if (!m_parent) return;

    QString text = m_parent->activeSubtitleText();
    QPointF scriptRes = m_parent->scriptResolution();

    QPointF p1, p2, org;
    m_hasMove = getLineMove(text, p1, p2, m_moveT1, m_moveT2);

    if (m_hasMove) {
        // Start Pin (Square)
        Pin pinStart;
        pinStart.type = DragStart;
        pinStart.scriptPos = p1;
        pinStart.canvasPos = fromScriptCoords(p1);
        m_pins.push_back(pinStart);

        // End Pin (Circle)
        Pin pinEnd;
        pinEnd.type = DragEnd;
        pinEnd.scriptPos = p2;
        pinEnd.canvasPos = fromScriptCoords(p2);
        m_pins.push_back(pinEnd);
    } else {
        getLinePosition(text, scriptRes, p1);
        Pin pinPos;
        pinPos.type = DragStart;
        pinPos.scriptPos = p1;
        pinPos.canvasPos = fromScriptCoords(p1);
        m_pins.push_back(pinPos);
    }

    if (getLineOrigin(text, org)) {
        Pin pinOrg;
        pinOrg.type = DragOrigin;
        pinOrg.scriptPos = org;
        pinOrg.canvasPos = fromScriptCoords(org);
        m_pins.push_back(pinOrg);
    }
}

Qt::CursorShape VisualToolDrag::cursorShape() const
{
    if (m_dragging) return Qt::ClosedHandCursor;
    for (const auto &pin : m_pins) {
        if (pin.isHovered) return Qt::OpenHandCursor;
    }
    return Qt::ArrowCursor;
}

void VisualToolDrag::mousePressed(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods);
    if (button != Qt::LeftButton) return;

    refreshFeatures();
    m_activePinIndex = -1;

    // Hit-test drag handles within 10px radius
    for (size_t i = 0; i < m_pins.size(); ++i) {
        qreal dx = m_pins[i].canvasPos.x() - pos.x();
        qreal dy = m_pins[i].canvasPos.y() - pos.y();
        if (std::sqrt(dx * dx + dy * dy) <= 10.0) {
            m_activePinIndex = static_cast<int>(i);
            m_dragging = true;
            m_dragStartMousePos = pos;
            m_dragStartScriptPos = m_pins[i].scriptPos;
            break;
        }
    }
}

void VisualToolDrag::mouseMoved(const QPointF &pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers mods)
{
    m_mousePos = pos;

    if (!m_dragging) {
        // Update hover state for pin handles
        for (auto &pin : m_pins) {
            pin.canvasPos = fromScriptCoords(pin.scriptPos);
            qreal dx = pin.canvasPos.x() - pos.x();
            qreal dy = pin.canvasPos.y() - pos.y();
            pin.isHovered = (std::sqrt(dx * dx + dy * dy) <= 10.0);
        }
        return;
    }

    if (m_activePinIndex < 0 || m_activePinIndex >= static_cast<int>(m_pins.size())) return;

    // Compute mouse displacement vector
    qreal deltaCanvasX = pos.x() - m_dragStartMousePos.x();
    qreal deltaCanvasY = pos.y() - m_dragStartMousePos.y();

    // Shift key constrains motion along horizontal or vertical axis
    if (mods.testFlag(Qt::ShiftModifier)) {
        if (std::abs(deltaCanvasX) > std::abs(deltaCanvasY)) {
            deltaCanvasY = 0.0;
        } else {
            deltaCanvasX = 0.0;
        }
    }

    QPointF constrainedCanvasPos = m_dragStartMousePos + QPointF(deltaCanvasX, deltaCanvasY);
    QPointF targetScriptPos = toScriptCoords(constrainedCanvasPos);
    int sx = static_cast<int>(std::round(targetScriptPos.x()));
    int sy = static_cast<int>(std::round(targetScriptPos.y()));

    QString text = m_parent->activeSubtitleText();
    Pin &curPin = m_pins[m_activePinIndex];
    curPin.scriptPos = targetScriptPos;
    curPin.canvasPos = constrainedCanvasPos;

    if (curPin.type == DragStart) {
        if (m_hasMove && m_pins.size() >= 2) {
            int eX = static_cast<int>(std::round(m_pins[1].scriptPos.x()));
            int eY = static_cast<int>(std::round(m_pins[1].scriptPos.y()));
            text = setOverrideTag(text, "\\move", QString("(%1,%2,%3,%4,%5,%6)")
                                     .arg(sx).arg(sy).arg(eX).arg(eY).arg(m_moveT1).arg(m_moveT2));
        } else {
            text = setOverrideTag(text, "\\pos", QString("(%1,%2)").arg(sx).arg(sy));
        }
    } else if (curPin.type == DragEnd && m_pins.size() >= 2) {
        int sX = static_cast<int>(std::round(m_pins[0].scriptPos.x()));
        int sY = static_cast<int>(std::round(m_pins[0].scriptPos.y()));
        text = setOverrideTag(text, "\\move", QString("(%1,%2,%3,%4,%5,%6)")
                                 .arg(sX).arg(sY).arg(sx).arg(sy).arg(m_moveT1).arg(m_moveT2));
    } else if (curPin.type == DragOrigin) {
        text = setOverrideTag(text, "\\org", QString("(%1,%2)").arg(sx).arg(sy));
    }

    m_parent->updateLiveSubtitleText(text);
}

void VisualToolDrag::mouseReleased(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos);
    Q_UNUSED(button);
    Q_UNUSED(mods);

    if (m_dragging) {
        m_dragging = false;
        m_activePinIndex = -1;
        m_parent->commitChanges("drag positioning");
        refreshFeatures();
    }
}

void VisualToolDrag::mouseDoubleClicked(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods);
    if (button != Qt::LeftButton) return;

    // Double-click repositions anchor to clicked point
    QPointF scriptCoord = toScriptCoords(pos);
    int sx = static_cast<int>(std::round(scriptCoord.x()));
    int sy = static_cast<int>(std::round(scriptCoord.y()));

    QString text = m_parent->activeSubtitleText();
    QString newText = setOverrideTag(text, "\\pos", QString("(%1,%2)").arg(sx).arg(sy));
    m_parent->updateLiveSubtitleText(newText);
    m_parent->commitChanges("positioning");
    refreshFeatures();
}

void VisualToolDrag::toggleMoveOrPos()
{
    if (!m_parent) return;
    refreshFeatures();
    QString text = m_parent->activeSubtitleText();

    if (m_hasMove) {
        // Convert \move to static \pos
        QPointF p1 = m_pins.empty() ? QPointF(100, 100) : m_pins[0].scriptPos;
        int sx = static_cast<int>(std::round(p1.x()));
        int sy = static_cast<int>(std::round(p1.y()));
        text = setOverrideTag(text, "\\pos", QString("(%1,%2)").arg(sx).arg(sy));
    } else {
        // Convert \pos to \move trajectory
        QPointF p1 = m_pins.empty() ? QPointF(100, 100) : m_pins[0].scriptPos;
        int sx = static_cast<int>(std::round(p1.x()));
        int sy = static_cast<int>(std::round(p1.y()));
        text = setOverrideTag(text, "\\move", QString("(%1,%2,%3,%4,0,1000)").arg(sx).arg(sy).arg(sx + 50).arg(sy + 50));
    }

    m_parent->updateLiveSubtitleText(text);
    m_parent->commitChanges("toggle move/pos");
    refreshFeatures();
}

QVariantList VisualToolDrag::getPinsData() const
{
    QVariantList list;
    for (const auto &p : m_pins) {
        QVariantMap map;
        map["type"] = static_cast<int>(p.type);
        map["x"] = p.canvasPos.x();
        map["y"] = p.canvasPos.y();
        map["scriptX"] = std::round(p.scriptPos.x());
        map["scriptY"] = std::round(p.scriptPos.y());
        map["isHovered"] = p.isHovered;
        map["isSelected"] = p.isSelected;
        list.append(map);
    }
    return list;
}

QVariantList VisualToolDrag::getConnectingLines() const
{
    QVariantList list;
    if (m_pins.size() >= 2 && m_hasMove) {
        QVariantMap moveLine;
        moveLine["type"] = "arrow";
        moveLine["x1"] = m_pins[0].canvasPos.x();
        moveLine["y1"] = m_pins[0].canvasPos.y();
        moveLine["x2"] = m_pins[1].canvasPos.x();
        moveLine["y2"] = m_pins[1].canvasPos.y();
        list.append(moveLine);
    }
    return list;
}

// VisualToolRotateZ: Z-axis rotation angle manipulator

VisualToolRotateZ::VisualToolRotateZ(VideoDisplayController *parent)
    : VisualToolBase(parent)
{
    refreshFeatures();
}

void VisualToolRotateZ::refreshFeatures()
{
    if (!m_parent) return;
    QString text = m_parent->activeSubtitleText();
    QPointF scriptRes = m_parent->scriptResolution();

    QPointF p1;
    getLinePosition(text, scriptRes, p1);
    m_posCanvasPos = fromScriptCoords(p1);

    QPointF org;
    if (getLineOrigin(text, org)) {
        m_originScriptPos = org;
    } else {
        m_originScriptPos = p1;
    }
    m_originCanvasPos = fromScriptCoords(m_originScriptPos);

    double rx, ry, rz;
    getLineRotation(text, rx, ry, rz);
    m_angle = rz;

    qreal dx = m_posCanvasPos.x() - m_originCanvasPos.x();
    qreal dy = m_posCanvasPos.y() - m_originCanvasPos.y();
    double dist = std::sqrt(dx * dx + dy * dy);
    m_radius = std::max(60.0, dist);
}

Qt::CursorShape VisualToolRotateZ::cursorShape() const
{
    return m_holding ? Qt::ClosedHandCursor : Qt::CrossCursor;
}

void VisualToolRotateZ::mousePressed(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods);
    if (button != Qt::LeftButton) return;

    refreshFeatures();
    m_holding = true;
    m_origAngle = m_angle;

    // Calculate initial mouse angle relative to origin
    qreal dx = pos.x() - m_originCanvasPos.x();
    qreal dy = pos.y() - m_originCanvasPos.y();
    m_startMouseAngle = std::atan2(-dy, dx) * kRad2Deg;
}

void VisualToolRotateZ::mouseMoved(const QPointF &pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers mods)
{
    m_mousePos = pos;
    if (!m_holding) return;

    qreal dx = pos.x() - m_originCanvasPos.x();
    qreal dy = pos.y() - m_originCanvasPos.y();
    double curMouseAngle = std::atan2(-dy, dx) * kRad2Deg;

    double delta = curMouseAngle - m_startMouseAngle;
    double newAngle = m_origAngle - delta;

    // Shift key snaps rotation angle to 15-degree steps
    if (mods.testFlag(Qt::ShiftModifier)) {
        newAngle = std::round(newAngle / 15.0) * 15.0;
    }

    newAngle = std::fmod(newAngle, 360.0);
    if (newAngle < 0.0) newAngle += 360.0;

    m_angle = newAngle;

    QString text = m_parent->activeSubtitleText();
    QString val = QString::number(newAngle, 'f', 1);
    if (val.endsWith(".0")) val.chop(2);
    QString newText = setOverrideTag(text, "\\frz", val);
    m_parent->updateLiveSubtitleText(newText);
}

void VisualToolRotateZ::mouseReleased(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos);
    Q_UNUSED(button);
    Q_UNUSED(mods);

    if (m_holding) {
        m_holding = false;
        m_parent->commitChanges("rotate z");
        refreshFeatures();
    }
}

void VisualToolRotateZ::mouseDoubleClicked(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos);
    Q_UNUSED(mods);
    if (button != Qt::LeftButton) return;

    // Double-click resets rotation angle to 0
    m_angle = 0.0;
    QString text = m_parent->activeSubtitleText();
    QString newText = setOverrideTag(text, "\\frz", "0");
    m_parent->updateLiveSubtitleText(newText);
    m_parent->commitChanges("reset rotate z");
    refreshFeatures();
}

QVariantMap VisualToolRotateZ::getRotationData() const
{
    QVariantMap map;
    map["centerX"] = m_originCanvasPos.x();
    map["centerY"] = m_originCanvasPos.y();
    map["radius"] = m_radius;
    map["angle"] = m_angle;
    map["isHolding"] = m_holding;
    map["mouseX"] = m_mousePos.x();
    map["mouseY"] = m_mousePos.y();
    return map;
}

// VisualToolScale: Subtitle glyph scale manipulator

VisualToolScale::VisualToolScale(VideoDisplayController *parent)
    : VisualToolBase(parent)
{
    refreshFeatures();
}

void VisualToolScale::refreshFeatures()
{
    if (!m_parent) return;
    QString text = m_parent->activeSubtitleText();
    QPointF scriptRes = m_parent->scriptResolution();
    QPointF p1;
    getLinePosition(text, scriptRes, p1);
    m_posCanvas = fromScriptCoords(p1);
    getLineScale(text, m_scaleX, m_scaleY);
}

Qt::CursorShape VisualToolScale::cursorShape() const
{
    return m_holding ? Qt::ClosedHandCursor : Qt::SizeAllCursor;
}

void VisualToolScale::mousePressed(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods);
    if (button != Qt::LeftButton) return;
    refreshFeatures();
    m_holding = true;
    m_dragStartPos = pos;
    m_initScaleX = m_scaleX;
    m_initScaleY = m_scaleY;
}

void VisualToolScale::mouseMoved(const QPointF &pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers mods)
{
    m_mousePos = pos;
    if (!m_holding) return;

    qreal dx = pos.x() - m_dragStartPos.x();
    qreal dy = pos.y() - m_dragStartPos.y();

    if (mods.testFlag(Qt::ShiftModifier)) {
        // Shift key enforces uniform aspect-ratio scaling
        double delta = (dx - dy) * 0.5;
        m_scaleX = std::clamp(m_initScaleX + delta, 0.0, 1000.0);
        m_scaleY = m_scaleX;
    } else {
        m_scaleX = std::clamp(m_initScaleX + dx * 0.5, 0.0, 1000.0);
        m_scaleY = std::clamp(m_initScaleY - dy * 0.5, 0.0, 1000.0);
    }

    QString text = m_parent->activeSubtitleText();
    text = setOverrideTag(text, "\\fscx", QString::number(std::round(m_scaleX)));
    text = setOverrideTag(text, "\\fscy", QString::number(std::round(m_scaleY)));
    m_parent->updateLiveSubtitleText(text);
}

void VisualToolScale::mouseReleased(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos);
    Q_UNUSED(button);
    Q_UNUSED(mods);
    if (m_holding) {
        m_holding = false;
        m_parent->commitChanges("scale");
        refreshFeatures();
    }
}

void VisualToolScale::mouseDoubleClicked(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos);
    Q_UNUSED(mods);
    if (button != Qt::LeftButton) return;
    m_scaleX = 100.0;
    m_scaleY = 100.0;
    QString text = m_parent->activeSubtitleText();
    text = setOverrideTag(text, "\\fscx", "100");
    text = setOverrideTag(text, "\\fscy", "100");
    m_parent->updateLiveSubtitleText(text);
    m_parent->commitChanges("reset scale");
    refreshFeatures();
}

QVariantMap VisualToolScale::getScaleData() const
{
    QVariantMap map;
    map["x"] = m_posCanvas.x();
    map["y"] = m_posCanvas.y();
    map["scaleX"] = m_scaleX;
    map["scaleY"] = m_scaleY;
    map["isHolding"] = m_holding;
    return map;
}

// VisualToolClip: Rectangular clipping mask manipulator

VisualToolClip::VisualToolClip(VideoDisplayController *parent)
    : VisualToolBase(parent)
{
    refreshFeatures();
}

void VisualToolClip::refreshFeatures()
{
    if (!m_parent) return;
    QString text = m_parent->activeSubtitleText();
    m_activeClip = getLineClip(text, m_p1Script, m_p2Script, m_inverse);
    if (m_activeClip) {
        m_p1Canvas = fromScriptCoords(m_p1Script);
        m_p2Canvas = fromScriptCoords(m_p2Script);
    }
}

Qt::CursorShape VisualToolClip::cursorShape() const
{
    return m_dragging ? Qt::CrossCursor : Qt::CrossCursor;
}

void VisualToolClip::mousePressed(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods);
    if (button != Qt::LeftButton) return;
    refreshFeatures();
    m_dragging = true;

    if (m_activeClip) {
        qreal minX = std::min(m_p1Canvas.x(), m_p2Canvas.x());
        qreal maxX = std::max(m_p1Canvas.x(), m_p2Canvas.x());
        qreal minY = std::min(m_p1Canvas.y(), m_p2Canvas.y());
        qreal maxY = std::max(m_p1Canvas.y(), m_p2Canvas.y());

        QPointF corners[4] = {
            QPointF(minX, minY), QPointF(maxX, minY),
            QPointF(minX, maxY), QPointF(maxX, maxY)
        };

        m_activeCorner = -1;
        for (int i = 0; i < 4; ++i) {
            qreal dx = corners[i].x() - pos.x();
            qreal dy = corners[i].y() - pos.y();
            if (std::sqrt(dx * dx + dy * dy) <= 10.0) {
                m_activeCorner = i;
                break;
            }
        }

        if (m_activeCorner < 0) {
            // Click outside corners begins a new clip rectangle
            m_p1Canvas = pos;
            m_p2Canvas = pos;
            m_p1Script = toScriptCoords(pos);
            m_p2Script = m_p1Script;
            m_activeClip = true;
        }
    } else {
        m_p1Canvas = pos;
        m_p2Canvas = pos;
        m_p1Script = toScriptCoords(pos);
        m_p2Script = m_p1Script;
        m_activeClip = true;
        m_activeCorner = -1;
    }
}

void VisualToolClip::mouseMoved(const QPointF &pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods);
    m_mousePos = pos;
    if (!m_dragging) return;

    if (m_activeCorner == 0) {
        m_p1Canvas = pos;
    } else if (m_activeCorner == 1) {
        m_p2Canvas.setX(pos.x());
        m_p1Canvas.setY(pos.y());
    } else if (m_activeCorner == 2) {
        m_p1Canvas.setX(pos.x());
        m_p2Canvas.setY(pos.y());
    } else {
        m_p2Canvas = pos;
    }

    m_p1Script = toScriptCoords(m_p1Canvas);
    m_p2Script = toScriptCoords(m_p2Canvas);

    int x1 = static_cast<int>(std::round(std::min(m_p1Script.x(), m_p2Script.x())));
    int y1 = static_cast<int>(std::round(std::min(m_p1Script.y(), m_p2Script.y())));
    int x2 = static_cast<int>(std::round(std::max(m_p1Script.x(), m_p2Script.x())));
    int y2 = static_cast<int>(std::round(std::max(m_p1Script.y(), m_p2Script.y())));

    QString text = m_parent->activeSubtitleText();
    QString tag = m_inverse ? "\\iclip" : "\\clip";
    text = setOverrideTag(text, tag, QString("(%1,%2,%3,%4)").arg(x1).arg(y1).arg(x2).arg(y2));
    m_parent->updateLiveSubtitleText(text);
}

void VisualToolClip::mouseReleased(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos);
    Q_UNUSED(button);
    Q_UNUSED(mods);
    if (m_dragging) {
        m_dragging = false;
        m_activeCorner = -1;
        m_parent->commitChanges("clip");
        refreshFeatures();
    }
}

void VisualToolClip::mouseDoubleClicked(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos);
    Q_UNUSED(mods);
    if (button != Qt::LeftButton) return;
    // Double-click resets clip rectangle to full script frame
    QPointF res = m_parent->scriptResolution();
    QString text = m_parent->activeSubtitleText();
    text = setOverrideTag(text, "\\clip", QString("(0,0,%1,%2)").arg(int(res.x())).arg(int(res.y())));
    m_parent->updateLiveSubtitleText(text);
    m_parent->commitChanges("reset clip");
    refreshFeatures();
}

QVariantMap VisualToolClip::getClipData() const
{
    QVariantMap map;
    map["active"] = m_activeClip;
    map["x1"] = std::min(m_p1Canvas.x(), m_p2Canvas.x());
    map["y1"] = std::min(m_p1Canvas.y(), m_p2Canvas.y());
    map["x2"] = std::max(m_p1Canvas.x(), m_p2Canvas.x());
    map["y2"] = std::max(m_p1Canvas.y(), m_p2Canvas.y());
    map["inverse"] = m_inverse;
    map["isDragging"] = m_dragging;
    return map;
}
