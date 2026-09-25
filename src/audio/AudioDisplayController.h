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

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QString>
#include <climits>
#include "AudioController.h"

/// Timeline interaction controller ported from Aegisub AudioDisplay.
///
/// Invariants and dispatch hierarchy:
///   - Hit dispatch precedence: Scrollbar > Timeline Ruler > Audio Waveform/Spectrum.
///   - Drag operations retain mouse capture across viewport boundaries.
///   - Dynamic keyboard modifier evaluation via QGuiApplication::queryKeyboardModifiers().
///   - Snap predicate: snapEnabled != Shift (XOR toggle).
///   - Invariant: clicking without dragging preserves scroll position.
class AudioDisplayController : public QObject {
    Q_OBJECT

    // Configuration properties matching Aegisub Audio/* settings.
    Q_PROPERTY(int  dragSensitivity READ dragSensitivity WRITE setDragSensitivity NOTIFY optionsChanged) // Audio/Start Drag Sensitivity
    Q_PROPERTY(int  snapDistance    READ snapDistance    WRITE setSnapDistance    NOTIFY optionsChanged) // Audio/Snap/Distance
    Q_PROPERTY(bool snapEnabled     READ snapEnabled     WRITE setSnapEnabled     NOTIFY optionsChanged) // Audio/Snap/Enable
    Q_PROPERTY(bool dragTiming      READ dragTiming      WRITE setDragTiming      NOTIFY optionsChanged) // Audio/Drag Timing
    Q_PROPERTY(bool autoScroll      READ autoScroll      WRITE setAutoScroll      NOTIFY optionsChanged) // Audio/Auto/Scroll
    Q_PROPERTY(bool wheelDefaultToZoom READ wheelDefaultToZoom WRITE setWheelDefaultToZoom NOTIFY optionsChanged)
    Q_PROPERTY(bool cursorTimeVisible  READ cursorTimeVisible  WRITE setCursorTimeVisible  NOTIFY optionsChanged) // Audio/Display/Draw/Cursor Time
    Q_PROPERTY(int  leadInMs  READ leadInMs  WRITE setLeadInMs  NOTIFY optionsChanged)   // Audio/Lead/IN
    Q_PROPERTY(int  leadOutMs READ leadOutMs WRITE setLeadOutMs NOTIFY optionsChanged)   // Audio/Lead/OUT
    Q_PROPERTY(int  defaultDuration READ defaultDuration WRITE setDefaultDuration NOTIFY optionsChanged) // Timing/Default Duration

    /// Boundary timestamps (ms) of other subtitle lines used for inter-line snapping.
    Q_PROPERTY(QVariantList extraSnapPoints READ extraSnapPoints WRITE setExtraSnapPoints NOTIFY optionsChanged)

    // Interaction state exposed to QML.
    Q_PROPERTY(bool    horizontalCursor READ horizontalCursor NOTIFY cursorChanged)
    Q_PROPERTY(int     hoveredMarker    READ hoveredMarker    NOTIFY cursorChanged)
    Q_PROPERTY(bool    dragging         READ dragging         NOTIFY interactionChanged)
    Q_PROPERTY(QString zoomDescription  READ zoomDescription  NOTIFY viewChanged)

public:
    explicit AudioDisplayController(AudioController *audio, QObject *parent = nullptr);

    int dragSensitivity() const { return m_dragSensitivity; }
    int snapDistance() const { return m_snapDistance; }
    bool snapEnabled() const { return m_snapEnabled; }
    bool dragTiming() const { return m_dragTiming; }
    bool autoScroll() const { return m_autoScroll; }
    bool wheelDefaultToZoom() const { return m_wheelDefaultToZoom; }
    bool cursorTimeVisible() const { return m_cursorTimeVisible; }
    int leadInMs() const { return m_leadIn; }
    int leadOutMs() const { return m_leadOut; }
    int defaultDuration() const { return m_defaultDuration; }
    QVariantList extraSnapPoints() const { return m_extraSnapPoints; }
    bool horizontalCursor() const { return m_horizontalCursor; }
    int  hoveredMarker() const { return m_hoveredMarker; }
    bool dragging() const { return m_drag != Drag::None; }
    QString zoomDescription() const;

public Q_SLOTS:
    void setDragSensitivity(int v);
    void setSnapDistance(int v);
    void setSnapEnabled(bool v);
    void setDragTiming(bool v);
    void setAutoScroll(bool v);
    void setWheelDefaultToZoom(bool v);
    void setCursorTimeVisible(bool v);
    void setLeadInMs(int v);
    void setLeadOutMs(int v);
    void setDefaultDuration(int v);
    void setExtraSnapPoints(const QVariantList &v);

    /// Viewport dimensions in logical pixels.
    void setViewportSize(qreal w, qreal h);

    // QML event forwarding.
    void mousePressed(qreal x, qreal y, int button, int modifiers);
    void mouseMoved(qreal x, qreal y, int buttons);
    void mouseReleased(qreal x, qreal y, int button, int modifiers);
    void mouseDoubleClicked(qreal x, qreal y, int button);
    void mouseLeft();
    void wheel(qreal angleX, qreal angleY, int modifiers);
    /// @return True if the key was handled and should be consumed by the UI.
    bool keyPressed(int key, int modifiers);

Q_SIGNALS:
    void optionsChanged();
    void cursorChanged();
    void interactionChanged();
    void viewChanged();
    void nextLineRequested();
    void prevLineRequested();

private:
    enum class Drag { None, Scrollbar, Timeline, MarkerStart, MarkerEnd, MarkersAll, Scrub };

    void updateMarkerDrag(qreal x);
    void updateHoverCursor(qreal x);
    void stopDrag();
    void scrollbarJumpTo(qreal x);

    int  timeAt(qreal x) const;
    double msPerPixel() const;
    int  snapRangeMs() const;
    /// Finds nearest snap boundary within +/- snapRange of anchors.
    int  snapShift(int anchorA, int anchorB, int otherEnd, int snapRange) const;

    AudioController *m_audioController = nullptr;

    // Viewport layout (logical pixels, audioViewContainer coordinate space).
    qreal m_viewportWidth = 0.0;
    qreal m_viewportHeight = 0.0;
    static constexpr int kRulerHeight = 17;
    static constexpr int kScrollbarHeight = 15;
    int m_scrollbarTop = 0;

    // Default configuration matching Aegisub default_config.json.
    int  m_dragSensitivity = 8;
    int  m_snapDistance = 8;
    bool m_snapEnabled = true;
    bool m_dragTiming = true;
    bool m_autoScroll = true;
    bool m_wheelDefaultToZoom = false;
    bool m_cursorTimeVisible = true;
    int  m_leadIn = 100;
    int  m_leadOut = 350;
    int  m_defaultDuration = 3000;
    QVariantList m_extraSnapPoints;

    // Interaction state.
    Drag m_drag = Drag::None;
    qreal m_dragAnchorX = 0.0;
    int   m_dragAnchorScroll = 0;
    int   m_anchorStart = 0;
    int   m_anchorEnd = 0;
    int   m_clickedMs = INT_MIN;  // INT_MIN indicates no translation in progress
    qreal m_lastPointerX = 0.0;
    bool  m_horizontalCursor = false;
    int   m_hoveredMarker = 0;

    // Zoom wheel accumulator preserving sub-notch increments across wheel events.
    int m_zoomAccum = 0;

    // Periodic edge scroll timer (50ms interval) for drags exceeding viewport boundaries.
    QTimer m_edgeScrollTimer;
};
