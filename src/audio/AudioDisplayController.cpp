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

#include "AudioDisplayController.h"

#include <QGuiApplication>
#include <algorithm>
#include <cmath>

AudioDisplayController::AudioDisplayController(AudioController *audio, QObject *parent)
    : QObject(parent)
    , m_audioController(audio)
{
    // 50ms periodic timer for automatic scrolling during out-of-bounds dragging.
    m_edgeScrollTimer.setSingleShot(false);
    m_edgeScrollTimer.setInterval(50);
    connect(&m_edgeScrollTimer, &QTimer::timeout, this, [this]() {
        if (m_drag == Drag::None || !m_audioController) {
            m_edgeScrollTimer.stop();
            return;
        }
        const qreal step = std::max<qreal>(8.0, m_viewportWidth / 20.0);
        if (m_lastPointerX < 0.0) {
            m_audioController->scrollBy(-int(step));
        } else if (m_lastPointerX > m_viewportWidth) {
            m_audioController->scrollBy(int(step));
        } else {
            m_edgeScrollTimer.stop();
            return;
        }
        updateMarkerDrag(std::clamp(m_lastPointerX, 0.0, m_viewportWidth));
    });

    if (m_audioController) {
        connect(m_audioController, &AudioController::zoomChanged, this, &AudioDisplayController::viewChanged);
        connect(m_audioController, &AudioController::scrollChanged, this, &AudioDisplayController::viewChanged);
    }
}

void AudioDisplayController::setDragSensitivity(int v)
{
    v = std::clamp(v, 1, 15);
    if (m_dragSensitivity != v) { m_dragSensitivity = v; Q_EMIT optionsChanged(); }
}

void AudioDisplayController::setSnapDistance(int v)
{
    v = std::clamp(v, 0, 25);
    if (m_snapDistance != v) { m_snapDistance = v; Q_EMIT optionsChanged(); }
}

void AudioDisplayController::setSnapEnabled(bool v)
{
    if (m_snapEnabled != v) { m_snapEnabled = v; Q_EMIT optionsChanged(); }
}

void AudioDisplayController::setDragTiming(bool v)
{
    if (m_dragTiming != v) { m_dragTiming = v; Q_EMIT optionsChanged(); }
}

void AudioDisplayController::setAutoScroll(bool v)
{
    if (m_autoScroll != v) { m_autoScroll = v; Q_EMIT optionsChanged(); }
}

void AudioDisplayController::setWheelDefaultToZoom(bool v)
{
    if (m_wheelDefaultToZoom != v) { m_wheelDefaultToZoom = v; Q_EMIT optionsChanged(); }
}

void AudioDisplayController::setCursorTimeVisible(bool v)
{
    if (m_cursorTimeVisible != v) { m_cursorTimeVisible = v; Q_EMIT optionsChanged(); }
}

void AudioDisplayController::setLeadInMs(int v)
{
    v = std::clamp(v, 0, 36000);
    if (m_leadIn != v) { m_leadIn = v; Q_EMIT optionsChanged(); }
}

void AudioDisplayController::setLeadOutMs(int v)
{
    v = std::clamp(v, 0, 36000);
    if (m_leadOut != v) { m_leadOut = v; Q_EMIT optionsChanged(); }
}

void AudioDisplayController::setDefaultDuration(int v)
{
    v = std::clamp(v, 0, 36000);
    if (m_defaultDuration != v) { m_defaultDuration = v; Q_EMIT optionsChanged(); }
}

void AudioDisplayController::setExtraSnapPoints(const QVariantList &v)
{
    m_extraSnapPoints = v;
    Q_EMIT optionsChanged();
}

void AudioDisplayController::setViewportSize(qreal w, qreal h)
{
    m_viewportWidth = w;
    m_viewportHeight = h;
    m_scrollbarTop = std::max(kRulerHeight, int(h) - kScrollbarHeight);
    // Forward actual viewport width to AudioController for scroll range bounds and auto-scroll.
    if (m_audioController && w > 0) {
        m_audioController->setViewportWidth(int(w));
    }
}

QString AudioDisplayController::zoomDescription() const
{
    const double msp = msPerPixel();
    if (msp <= 0.0) return QString();
    const int pxPerSec = int(std::lround(1000.0 / msp));
    return QStringLiteral("%1%, %2 pixel/second").arg(pxPerSec).arg(pxPerSec);
}

double AudioDisplayController::msPerPixel() const
{
    return m_audioController ? m_audioController->msPerPixel() : 1.0;
}

int AudioDisplayController::timeAt(qreal x) const
{
    if (!m_audioController) return 0;
    // AudioDisplay::TimeFromRelativeX: int((scroll_left + x) * ms_per_pixel).
    return int((m_audioController->scrollLeft() + x) * msPerPixel());
}

int AudioDisplayController::snapRangeMs() const
{
    // XOR toggle: snapping enabled when Audio/Snap/Enable != Shift modifier.
    const bool shift = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier);
    if (m_snapEnabled == shift) return 0;
    return int(m_snapDistance * msPerPixel());
}

int AudioDisplayController::snapShift(int anchorA, int anchorB, int otherEnd, int snapRange) const
{
    if (snapRange <= 0 || !m_audioController) return 0;

    int best = INT_MAX;
    auto scanAnchor = [&](int anchor) {
        auto consider = [&](int candidate) {
            const int d = candidate - anchor;
            if (std::abs(d) < std::abs(best)) best = d;
        };
        // Video keyframes
        const QVariantList kfs = m_audioController->keyframes();
        for (const QVariant &v : kfs) consider(v.toInt());
        // Neighboring subtitle line boundaries
        for (const QVariant &v : m_extraSnapPoints) consider(v.toInt());
        // Inactive boundary of the current active line
        if (otherEnd != INT_MIN) consider(otherEnd);
    };

    scanAnchor(anchorA);
    if (anchorB != INT_MIN) scanAnchor(anchorB);

    if (best == INT_MAX || std::abs(best) > snapRange) return 0;
    return best;
}

void AudioDisplayController::scrollbarJumpTo(qreal x)
{
    if (!m_audioController || m_viewportWidth <= 0.0) return;
    const double w = m_viewportWidth;
    const double totalPx = (m_audioController->duration() * 1000.0) / msPerPixel();
    if (totalPx <= w) return;
    const double thumbW = std::max(10.0, w * w / totalPx);
    if (w - thumbW <= 0.0) return;
    // Scrollbar thumb proportional mapping: (total - viewport) * (x - thumb / 2) / (viewport - thumb).
    const double pos = (totalPx - w) * (x - thumbW / 2.0) / (w - thumbW);
    m_audioController->scrollTo(int(std::lround(pos)));
}

void AudioDisplayController::mousePressed(qreal x, qreal y, int button, int modifiers)
{
    if (!m_audioController) return;
    m_lastPointerX = x;

    // 1) Scrollbar hit: takes precedence, absorbs event.
    if (y >= m_scrollbarTop) {
        m_drag = Drag::Scrollbar;
        m_dragAnchorX = x;
        m_dragAnchorScroll = m_audioController->scrollLeft();
        scrollbarJumpTo(x);
        Q_EMIT interactionChanged();
        return;
    }

    // 2) Timeline ruler: drag to scroll horizontally.
    if (y < kRulerHeight) {
        m_drag = Drag::Timeline;
        m_dragAnchorX = x;
        m_dragAnchorScroll = m_audioController->scrollLeft();
        if (!m_audioController->isPlaying()) m_audioController->clearCursor();
        Q_EMIT interactionChanged();
        return;
    }

    // 3) Audio display body.
    if (button == Qt::MiddleButton) {
        // Middle click: seek immediately and start scrubbing on drag.
        m_drag = Drag::Scrub;
        m_audioController->jumpToTime(timeAt(x));
        Q_EMIT interactionChanged();
        return;
    }

    const Qt::KeyboardModifiers mods = Qt::KeyboardModifiers(modifiers);
    const bool alt = mods.testFlag(Qt::AltModifier);
    const int timepos = timeAt(x);
    const int dragSens = int(m_dragSensitivity * msPerPixel());
    const int snapRange = snapRangeMs();
    const int oldScroll = m_audioController->scrollLeft();

    if (button == Qt::RightButton) {
        // Right click: reposition end marker and initiate drag.
        const int start = m_audioController->selectionStart();
        const int end = timepos + snapShift(timepos, INT_MIN, start, snapRange);
        m_audioController->setSelectionEnd(end);
        m_drag = Drag::MarkerEnd;
    } else if (button == Qt::LeftButton) {
        if (alt) {
            // Alt: translate entire selection range.
            m_anchorStart = m_audioController->selectionStart();
            m_anchorEnd = m_audioController->selectionEnd();
            m_clickedMs = timepos;
            m_drag = Drag::MarkersAll;
        } else {
            const int start = m_audioController->selectionStart();
            const int end = m_audioController->selectionEnd();
            const int distL = std::abs(start - timepos);
            const int distR = std::abs(end - timepos);

            if (distL > dragSens && distR > dragSens) {
                // Unanchored click: move start marker; Audio/Drag Timing selects drag target.
                m_audioController->setSelectionStart(timepos + snapShift(timepos, INT_MIN, end, snapRange));
                m_drag = m_dragTiming ? Drag::MarkerEnd : Drag::MarkerStart;
            } else if (distL <= distR) {
                m_audioController->setSelectionStart(timepos + snapShift(timepos, INT_MIN, end, snapRange));
                m_drag = Drag::MarkerStart;
            } else {
                m_audioController->setSelectionEnd(timepos + snapShift(timepos, INT_MIN, start, snapRange));
                m_drag = Drag::MarkerEnd;
            }
        }
    } else {
        return;
    }

    // Invariant: clicking must not trigger scroll shifts.
    m_audioController->scrollTo(oldScroll);
    if (!m_audioController->isPlaying()) m_audioController->clearCursor();
    Q_EMIT interactionChanged();
}

void AudioDisplayController::mouseMoved(qreal x, qreal y, int buttons)
{
    Q_UNUSED(y);
    Q_UNUSED(buttons);
    if (!m_audioController) return;
    m_lastPointerX = x;

    switch (m_drag) {
    case Drag::Scrollbar:
        scrollbarJumpTo(x);
        return;
    case Drag::Timeline:
        m_audioController->scrollTo(m_dragAnchorScroll + int(std::lround(m_dragAnchorX - x)));
        return;
    case Drag::Scrub:
        m_audioController->jumpToTime(timeAt(x));
        return;
    case Drag::MarkerStart:
    case Drag::MarkerEnd:
    case Drag::MarkersAll:
        updateMarkerDrag(x);
        // Trigger periodic edge scrolling when dragged beyond viewport boundaries.
        if (x < 0.0 || x > m_viewportWidth) {
            if (!m_edgeScrollTimer.isActive()) m_edgeScrollTimer.start();
        } else {
            m_edgeScrollTimer.stop();
        }
        return;
    case Drag::None:
        break;
    }

    // Update hover tracking cursor and marker boundary states.
    if (!m_audioController->isPlaying()) {
        m_audioController->updateCursor(int(std::lround(x)));
    }
    updateHoverCursor(x);
}

void AudioDisplayController::updateMarkerDrag(qreal x)
{
    if (!m_audioController) return;
    const int timepos = timeAt(x);
    const int snapRange = snapRangeMs();

    if (m_drag == Drag::MarkersAll) {
        const int shift = timepos - m_clickedMs;
        const int ns = m_anchorStart + shift;
        const int ne = m_anchorEnd + shift;
        const int d = snapShift(ns, ne, INT_MIN, snapRange);
        m_audioController->setSelection(ns + d, ne + d);
    } else if (m_drag == Drag::MarkerStart) {
        const int end = m_audioController->selectionEnd();
        m_audioController->setSelectionStart(timepos + snapShift(timepos, INT_MIN, end, snapRange));
    } else if (m_drag == Drag::MarkerEnd) {
        const int start = m_audioController->selectionStart();
        m_audioController->setSelectionEnd(timepos + snapShift(timepos, INT_MIN, start, snapRange));
    }
}

void AudioDisplayController::updateHoverCursor(qreal x)
{
    if (!m_audioController) return;
    const int timepos = timeAt(x);
    const int sens = int(m_dragSensitivity * msPerPixel());
    const bool alt = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier);

    int newHover = 0;
    const int startDist = std::abs(m_audioController->selectionStart() - timepos);
    const int endDist = std::abs(m_audioController->selectionEnd() - timepos);

    if (startDist <= sens) {
        newHover = 1;
    } else if (endDist <= sens) {
        newHover = 2;
    } else if (alt) {
        // Holding Alt enables whole-selection shift cursor mode.
        newHover = 3;
    }

    const bool near = alt || (newHover != 0);
    if (near != m_horizontalCursor || newHover != m_hoveredMarker) {
        m_horizontalCursor = near;
        m_hoveredMarker = newHover;
        Q_EMIT cursorChanged();
    }
}

void AudioDisplayController::stopDrag()
{
    m_edgeScrollTimer.stop();
    m_drag = Drag::None;
    m_clickedMs = INT_MIN;
    Q_EMIT interactionChanged();
}

void AudioDisplayController::mouseDoubleClicked(qreal x, qreal y, int button)
{
    Q_UNUSED(x);
    Q_UNUSED(y);
    if (!m_audioController) return;
    if (button == Qt::LeftButton) {
        if (m_audioController->isPlaying()) {
            m_audioController->stop();
        } else {
            m_audioController->playSelection();
        }
    }
}

void AudioDisplayController::mouseReleased(qreal x, qreal y, int button, int modifiers)
{
    Q_UNUSED(y);
    Q_UNUSED(button);
    Q_UNUSED(modifiers);
    if (!m_audioController) return;

    stopDrag();

    // Margin clicks within 5% of left/right border scroll viewport by one third.
    if (m_autoScroll && m_viewportWidth > 0.0) {
        const qreal w = m_viewportWidth;
        if (x < w / 20.0) {
            m_audioController->scrollBy(-int(w / 3));
        } else if (w - x < w / 20.0) {
            m_audioController->scrollBy(int(w / 3));
        }
    }
}

void AudioDisplayController::mouseLeft()
{
    if (m_audioController && !m_audioController->isPlaying()) {
        m_audioController->clearCursor();
    }
    if (m_horizontalCursor || m_hoveredMarker != 0) {
        m_horizontalCursor = false;
        m_hoveredMarker = 0;
        Q_EMIT cursorChanged();
    }
}

void AudioDisplayController::wheel(qreal angleX, qreal angleY, int modifiers)
{
    const Qt::KeyboardModifiers mods = Qt::KeyboardModifiers(modifiers);
#if defined(Q_OS_MACOS)
    const bool ctrlOrCmd = mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::MetaModifier);
#else
    const bool ctrlOrCmd = mods.testFlag(Qt::ControlModifier);
#endif
    // Ctrl/Cmd toggles between scrolling and zooming unless overridden by wheelDefaultToZoom.
    const bool zoom = ctrlOrCmd != m_wheelDefaultToZoom;

    if (!zoom) {
        m_zoomAccum = 0;
        // Vertical axis takes precedence over horizontal axis unless horizontal delta is dominant.
        int amount = -int(std::lround(angleY));
        if (std::abs(angleX) > std::abs(angleY) && std::abs(angleX) >= 1.0) amount = -int(std::lround(angleX));
        if (amount != 0) m_audioController->scrollBy(amount);
        return;
    }

    // Accumulate wheel delta across events to handle notched detents.
    m_zoomAccum += int(std::lround(angleY));
    const int delta = m_zoomAccum / 120;
    m_zoomAccum %= 120;
    if (delta != 0) {
        m_audioController->setZoomLevel(m_audioController->zoomLevel() + delta);
    }
}

bool AudioDisplayController::keyPressed(int key, int modifiers)
{
    if (!m_audioController) return false;

    const Qt::KeyboardModifiers mods = Qt::KeyboardModifiers(modifiers);
#if defined(Q_OS_MACOS)
    if (mods.testFlag(Qt::MetaModifier) || mods.testFlag(Qt::ControlModifier)) return false;
#else
    if (mods.testFlag(Qt::ControlModifier)) return false;
#endif

    const bool shift = mods.testFlag(Qt::ShiftModifier);
    const bool keypad = mods.testFlag(Qt::KeypadModifier);

    auto modifyLength = [this](int cs) {
        const int end = std::max(m_audioController->selectionStart(),
                                 m_audioController->selectionEnd() + cs * 10);
        m_audioController->setSelectionEnd(end);
    };
    auto modifyStart = [this](int cs) {
        const int start = std::min(m_audioController->selectionEnd(),
                                   m_audioController->selectionStart() + cs * 10);
        m_audioController->setSelectionStart(start);
    };

    // Numpad hotkeys ("Always" context).
    if (keypad) {
        switch (key) {
        case Qt::Key_Enter: m_audioController->commit(); return true;
        case Qt::Key_5:     m_audioController->playSelection(); return true;
        case Qt::Key_3:     m_audioController->play500msAfter(); return true;
        case Qt::Key_1:     m_audioController->play500msBefore(); return true;
        case Qt::Key_8:     m_audioController->stop(); return true;
        case Qt::Key_2:     Q_EMIT nextLineRequested(); return true;
        case Qt::Key_0:     Q_EMIT prevLineRequested(); return true;
        case Qt::Key_7:     modifyLength(-1); return true;
        case Qt::Key_9:     modifyLength(+1); return true;
        case Qt::Key_4:     modifyStart(-1); return true;
        case Qt::Key_6:     modifyStart(+1); return true;
        default: break;
        }
    }

    // Audio hotkeys ("Audio" context).
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_G:
        if (key == Qt::Key_G && shift) {
            // Shift-G: reset line to default duration and commit.
            const int start = m_audioController->selectionStart();
            m_audioController->setSelection(start, start + m_defaultDuration);
            m_audioController->commit();
        } else {
            m_audioController->commit();
        }
        return true;
    case Qt::Key_R:     m_audioController->playCurrentLine(); return true;
    case Qt::Key_S:
    case Qt::Key_Space: m_audioController->playSelection(); return true;
    case Qt::Key_W:     m_audioController->play500msAfter(); return true;
    case Qt::Key_Q:     m_audioController->play500msBefore(); return true;
    case Qt::Key_E:     m_audioController->playFirst500ms(); return true;
    case Qt::Key_D:     m_audioController->playLast500ms(); return true;
    case Qt::Key_T:     m_audioController->playToEnd(); return true;
    case Qt::Key_B:
        if (m_audioController->isPlaying()) m_audioController->stop();
        else m_audioController->playSelection();
        return true;
    case Qt::Key_H:     m_audioController->stop(); return true;
    case Qt::Key_C:     m_audioController->leadIn(m_leadIn); return true;   // Audio/Lead/IN
    case Qt::Key_V:     m_audioController->leadOut(m_leadOut); return true; // Audio/Lead/OUT
    case Qt::Key_A:     m_audioController->scrollBy(-std::max(1, int(m_viewportWidth / 3))); return true;
    case Qt::Key_F:     m_audioController->scrollBy(std::max(1, int(m_viewportWidth / 3))); return true;
    case Qt::Key_X:
    case Qt::Key_Right: Q_EMIT nextLineRequested(); return true;
    case Qt::Key_Z:
    case Qt::Key_Left:  Q_EMIT prevLineRequested(); return true;
    // Length adjustments matching KP_Add / KP_Subtract
    case Qt::Key_Plus:  modifyLength(shift ? +5 : +1); return true;
    case Qt::Key_Minus: modifyLength(shift ? -5 : -1); return true;
    default: break;
    }
    return false;
}
