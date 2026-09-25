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
#include <QRectF>
#include <QPointF>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <memory>
#include "VideoController.h"

class VisualToolBase;
class VisualToolCross;
class VisualToolDrag;
class VisualToolRotateZ;
class VisualToolScale;
class VisualToolClip;

/// Manages the video display viewport, aspect ratio scaling, zoom/pan geometry,
/// and delegates mouse interactions to active visual typesetting tools.
class VideoDisplayController : public QObject {
    Q_OBJECT

    // Viewport geometry and transformation properties
    Q_PROPERTY(qreal windowZoom READ windowZoom WRITE setWindowZoom NOTIFY windowZoomChanged)
    Q_PROPERTY(QString zoomText READ zoomText WRITE setZoomText NOTIFY windowZoomChanged)
    Q_PROPERTY(qreal preferredBoxWidth READ preferredBoxWidth NOTIFY layoutChanged)
    Q_PROPERTY(qreal preferredBoxHeight READ preferredBoxHeight NOTIFY layoutChanged)
    Q_PROPERTY(qreal contentZoom READ contentZoom WRITE setContentZoom NOTIFY layoutChanged)
    Q_PROPERTY(qreal arOverride READ arOverride WRITE setArOverride NOTIFY layoutChanged)
    Q_PROPERTY(qreal panX READ panX NOTIFY layoutChanged)
    Q_PROPERTY(qreal panY READ panY NOTIFY layoutChanged)
    Q_PROPERTY(qreal videoLeft READ videoLeft NOTIFY layoutChanged)
    Q_PROPERTY(qreal videoTop READ videoTop NOTIFY layoutChanged)
    Q_PROPERTY(qreal videoWidth READ videoWidth NOTIFY layoutChanged)
    Q_PROPERTY(qreal videoHeight READ videoHeight NOTIFY layoutChanged)
    Q_PROPERTY(int   currentTool READ currentTool WRITE setCurrentTool NOTIFY toolChanged)
    Q_PROPERTY(int   cursorShape READ cursorShape NOTIFY visualDataChanged)

    // Visual typesetting tool overlay metadata exported to QML
    Q_PROPERTY(qreal crosshairX READ crosshairX NOTIFY visualDataChanged)
    Q_PROPERTY(qreal crosshairY READ crosshairY NOTIFY visualDataChanged)
    Q_PROPERTY(bool  crosshairVisible READ crosshairVisible NOTIFY visualDataChanged)
    Q_PROPERTY(QString scriptCoordText READ scriptCoordText NOTIFY visualDataChanged)
    Q_PROPERTY(QVariantList dragPins READ dragPins NOTIFY visualDataChanged)
    Q_PROPERTY(QVariantList connectingLines READ connectingLines NOTIFY visualDataChanged)
    Q_PROPERTY(QVariantMap rotationData READ rotationData NOTIFY visualDataChanged)
    Q_PROPERTY(QVariantMap scaleData READ scaleData NOTIFY visualDataChanged)
    Q_PROPERTY(QVariantMap clipData READ clipData NOTIFY visualDataChanged)

public:
    explicit VideoDisplayController(VideoController *video, QObject *parent = nullptr);
    ~VideoDisplayController() override;

    qreal windowZoom() const { return m_windowZoom; }
    QString zoomText() const { return m_zoomText; }
    qreal preferredBoxWidth() const;
    qreal preferredBoxHeight() const;
    qreal contentZoom() const { return m_contentZoom; }
    // Aspect ratio override (upstream Video > Override Aspect Ratio):
    // 0 keeps the native frame ratio; >0 forces display to the given w/h ratio.
    qreal arOverride() const { return m_arOverride; }
    qreal panX() const { return m_panX; }
    qreal panY() const { return m_panY; }
    qreal videoLeft() const { return m_videoRect.left(); }
    qreal videoTop() const { return m_videoRect.top(); }
    qreal videoWidth() const { return m_videoRect.width(); }
    qreal videoHeight() const { return m_videoRect.height(); }
    int   currentTool() const { return m_currentTool; }
    int   cursorShape() const;

    QRectF videoRect() const { return m_videoRect; }
    QPointF scriptResolution() const;
    QString activeSubtitleText() const;

    // Visual tool data accessors
    qreal crosshairX() const;
    qreal crosshairY() const;
    bool  crosshairVisible() const;
    QString scriptCoordText() const;
    QVariantList dragPins() const;
    QVariantList connectingLines() const;
    QVariantMap rotationData() const;
    QVariantMap scaleData() const;
    QVariantMap clipData() const;

public Q_SLOTS:
    void setWindowZoom(qreal z);
    void setZoomText(const QString &text);
    void setViewportSize(qreal w, qreal h);
    void setContentZoom(qreal z);
    void setArOverride(qreal ar);
    void setCurrentTool(int tool);
    void resetContentZoom();
    void toggleMoveOrPos();
    void pan(double dx, double dy);

    // Mouse and wheel event handlers
    void mousePressed(qreal x, qreal y, int button, int modifiers);
    void mouseMoved(qreal x, qreal y, int buttons, int modifiers);
    void mouseReleased(qreal x, qreal y, int button, int modifiers);
    void mouseDoubleClicked(qreal x, qreal y, int button, int modifiers);
    void mouseLeft();
    void wheel(qreal angleDeltaY, qreal x, qreal y, int modifiers);

    // Live subtitle text updates from active visual tool
    void updateLiveSubtitleText(const QString &text);
    void commitChanges(const QString &description);

Q_SIGNALS:
    void windowZoomChanged();
    void layoutChanged();
    void toolChanged();
    void visualDataChanged();
    void subtitleTextChanged(const QString &newText);
    void commitRequested(const QString &description);

private:
    void recalculateVideoLayout();
    void zoomAndPan(double newZoom, const QPointF &anchorPoint, const QPointF &newPos);
    QPointF getZoomAnchorPoint(const QPointF &pos) const;

    VideoController *m_videoController = nullptr;

    // Window zoom and display text. Defaults to 0.0 ("Fit" viewport); explicit zoom scales by video size.
    qreal m_windowZoom = 0.0;
    QString m_zoomText = QStringLiteral("Fit");

    // Viewport dimensions (logical pixels)
    qreal m_viewportWidth = 400.0;
    qreal m_viewportHeight = 280.0;

    // Video display box geometry (centered in Fit mode, anchored to top-left in explicit zoom mode)
    qreal m_boxWidth = 0.0;
    qreal m_boxHeight = 0.0;
    qreal m_boxLeft = 0.0;
    qreal m_boxTop = 0.0;

    // Display zoom, pan offsets, and letterboxed video bounds
    qreal m_contentZoom = 1.0;
    // 0 = native aspect; positive values force the display ratio (upstream Override AR).
    qreal m_arOverride = 0.0;
    qreal m_panX = 0.0;
    qreal m_panY = 0.0;
    QRectF m_videoRect;

    // Middle-button panning state
    bool m_middlePanning = false;
    QPointF m_lastMousePos;

    // Visual typesetting tool instances
    int m_currentTool = 0; // 0: Crosshair, 1: Drag, 2: RotateZ, 4: Scale, 5: Clip
    std::unique_ptr<VisualToolCross> m_toolCross;
    std::unique_ptr<VisualToolDrag> m_toolDrag;
    std::unique_ptr<VisualToolRotateZ> m_toolRotateZ;
    std::unique_ptr<VisualToolScale> m_toolScale;
    std::unique_ptr<VisualToolClip> m_toolClip;
};
