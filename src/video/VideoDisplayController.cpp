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

#include "VideoDisplayController.h"
#include "VisualTools.h"
#include <algorithm>
#include <cmath>

VideoDisplayController::VideoDisplayController(VideoController *video, QObject *parent)
    : QObject(parent)
    , m_videoController(video)
{
    m_toolCross = std::make_unique<VisualToolCross>(this);
    m_toolDrag = std::make_unique<VisualToolDrag>(this);
    m_toolRotateZ = std::make_unique<VisualToolRotateZ>(this);
    m_toolScale = std::make_unique<VisualToolScale>(this);
    m_toolClip = std::make_unique<VisualToolClip>(this);

    if (m_videoController) {
        auto refreshAllTools = [this]() {
            if (m_toolDrag) m_toolDrag->refreshFeatures();
            if (m_toolRotateZ) m_toolRotateZ->refreshFeatures();
            if (m_toolScale) m_toolScale->refreshFeatures();
            if (m_toolClip) m_toolClip->refreshFeatures();
            Q_EMIT visualDataChanged();
        };
        connect(m_videoController, &VideoController::videoInfoChanged, this, [this, refreshAllTools]() {
            recalculateVideoLayout();
            refreshAllTools();
        });
        connect(m_videoController, &VideoController::zoomChanged, this, [this]() {
            if (m_videoController && m_videoController->zoom() != m_zoomText) {
                setZoomText(m_videoController->zoom());
            }
        });
        connect(m_videoController, &VideoController::subtitleSyncChanged, this, refreshAllTools);
        connect(m_videoController, &VideoController::positionChanged, this, refreshAllTools);
    }

    recalculateVideoLayout();
}

VideoDisplayController::~VideoDisplayController() = default;

qreal VideoDisplayController::preferredBoxWidth() const
{
    if (m_windowZoom > 0.001 && m_videoController && m_videoController->videoWidth() > 0) {
        return std::max(380.0, m_videoController->videoWidth() * m_windowZoom + 20.0);
    }
    return 414.0;
}

qreal VideoDisplayController::preferredBoxHeight() const
{
    if (m_windowZoom > 0.001 && m_videoController && m_videoController->videoHeight() > 0) {
        return std::max(220.0, m_videoController->videoHeight() * m_windowZoom + 54.0);
    }
    return 318.0;
}

void VideoDisplayController::setWindowZoom(qreal z)
{
    if (z < 0.001) {
        // "Fit" is a responsive extension mode; upstream Aegisub only supported fixed 12.5%~300% zoom steps.
        m_windowZoom = 0.0;
        m_zoomText = QStringLiteral("Fit");
    } else {
        // Upstream SetWindowZoom: lower bound 12.5%, dropdown menu with 24 discrete steps (12.5% ~ 300%).
        m_windowZoom = std::clamp(z, 0.125, 3.0);
        m_zoomText = QString::number(m_windowZoom * 100.0, 'g', 4) + QStringLiteral("%");
    }
    if (m_videoController && m_videoController->zoom() != m_zoomText) {
        m_videoController->setZoom(m_zoomText);
    }
    recalculateVideoLayout();
    Q_EMIT windowZoomChanged();
}

void VideoDisplayController::setZoomText(const QString &text)
{
    QString trimmed = text.trimmed();
    if (trimmed.compare(QStringLiteral("Fit"), Qt::CaseInsensitive) == 0) {
        setWindowZoom(0.0);
        return;
    }
    QString clean = trimmed;
    if (clean.endsWith(QLatin1Char('%'))) {
        clean.chop(1);
    }
    bool ok = false;
    double val = clean.toDouble(&ok);
    if (ok && val > 0.0) {
        setWindowZoom(val / 100.0);
    }
}

QPointF VideoDisplayController::scriptResolution() const
{
    if (m_videoController && m_videoController->videoWidth() > 0 && m_videoController->videoHeight() > 0) {
        return QPointF(m_videoController->videoWidth(), m_videoController->videoHeight());
    }
    return QPointF(640.0, 480.0);
}

QString VideoDisplayController::activeSubtitleText() const
{
    return m_videoController ? m_videoController->activeSubText() : QString();
}

void VideoDisplayController::setViewportSize(qreal w, qreal h)
{
    if (w <= 0.0 || h <= 0.0) return;
    if (std::abs(m_viewportWidth - w) > 0.5 || std::abs(m_viewportHeight - h) > 0.5) {
        m_viewportWidth = w;
        m_viewportHeight = h;
        recalculateVideoLayout();
    }
}

void VideoDisplayController::setContentZoom(qreal z)
{
    z = std::clamp(z, 0.125, 10.0);
    if (std::abs(m_contentZoom - z) > 0.001) {
        m_contentZoom = z;
        recalculateVideoLayout();
    }
}

void VideoDisplayController::setCurrentTool(int tool)
{
    tool = std::clamp(tool, 0, 7);
    if (m_currentTool != tool) {
        m_currentTool = tool;
        if (m_toolDrag) m_toolDrag->refreshFeatures();
        if (m_toolRotateZ) m_toolRotateZ->refreshFeatures();
        if (m_toolScale) m_toolScale->refreshFeatures();
        if (m_toolClip) m_toolClip->refreshFeatures();
        Q_EMIT toolChanged();
        Q_EMIT visualDataChanged();
    }
}

void VideoDisplayController::resetContentZoom()
{
    m_panX = 0.0;
    m_panY = 0.0;
    m_contentZoom = 1.0;
    recalculateVideoLayout();
}

void VideoDisplayController::toggleMoveOrPos()
{
    if (m_toolDrag) {
        m_toolDrag->toggleMoveOrPos();
    }
}

// Viewport geometry and layout calculations

void VideoDisplayController::setArOverride(qreal ar)
{
    // Sanitize: ratios below 0.05 are treated as invalid; 0 restores the native frame ratio.
    const qreal clamped = (ar > 0.05) ? ar : 0.0;
    if (clamped == m_arOverride) return;
    m_arOverride = clamped;
    recalculateVideoLayout();
}

void VideoDisplayController::recalculateVideoLayout()
{
    int vw = (m_videoController && m_videoController->videoWidth() > 0) ? m_videoController->videoWidth() : 640;
    int vh = (m_videoController && m_videoController->videoHeight() > 0) ? m_videoController->videoHeight() : 480;

    // Effective display aspect: the override (upstream Video > Override Aspect Ratio)
    // takes precedence over the native frame ratio when set.
    const double videoAR = (m_arOverride > 0.05) ? static_cast<double>(m_arOverride)
                                                 : (double(vw) / double(vh));

    double boxW = 0.0;
    double boxH = 0.0;
    double boxLeft = 0.0;
    double boxTop = 0.0;

    if (m_windowZoom <= 0.001) {
        // Fit (adaptive, default): scales content to preserve video aspect ratio centered in the viewport.
        // Matches upstream PositionVideo freeSize branch (video_display.cpp:320-343:
        // compute content_width/height based on aspect ratio, then derive box dimensions).
        const double viewportAR = (m_viewportHeight > 0) ? (m_viewportWidth / m_viewportHeight) : 1.0;
        if (viewportAR > videoAR) {
            boxH = m_viewportHeight;
            boxW = boxH * videoAR;
        } else {
            boxW = m_viewportWidth;
            boxH = boxW / videoAR;
        }
        boxLeft = (m_viewportWidth - boxW) / 2.0;
        boxTop = (m_viewportHeight - boxH) / 2.0;
        // Upstream writes the fitted percentage to zoomBox->ChangeValue; AegisubQT uses
        // discrete dropdown presets so this state is labeled "Fit" while the ratio guides layout.
    } else {
        // Explicit window zoom: video box = video dimensions * zoom factor,
        // anchored to the top-left corner (matching upstream FitClientSizeToVideo).
        // With an AR override the box keeps native width but derives height from the override.
        boxW = vw * m_windowZoom;
        boxH = (m_arOverride > 0.05) ? (boxW / videoAR) : (vh * m_windowZoom);
    }
    m_boxWidth = boxW;
    m_boxHeight = boxH;
    m_boxLeft = boxLeft;
    m_boxTop = boxTop;

    // Content zoom applies inside the video box, centered within the frame (upstream PositionVideo content_left/top).
    const double contentW = boxW * m_contentZoom;
    const double contentH = boxH * m_contentZoom;

    // Restrict pan offset so video bounds cannot be dragged entirely offscreen.
    double maxPanX = 0.5 * contentW + 0.4 * m_viewportWidth;
    double maxPanY = 0.5 * contentH + 0.4 * m_viewportHeight;
    m_panX = std::clamp(m_panX, -maxPanX, maxPanX);
    m_panY = std::clamp(m_panY, -maxPanY, maxPanY);

    double left = boxLeft + m_panX + (boxW - contentW) / 2.0;
    double top = boxTop + m_panY + (boxH - contentH) / 2.0;

    m_videoRect = QRectF(left, top, contentW, contentH);
    Q_EMIT layoutChanged();
    Q_EMIT visualDataChanged();
}

QPointF VideoDisplayController::getZoomAnchorPoint(const QPointF &pos) const
{
    // Anchor point expressed relative to video box center (upstream video_display.cpp:540-562).
    // Box center = box position + pan + box / 2; content zooming does not alter the box center.
    const QPointF boxCenter(m_boxLeft + m_panX + m_boxWidth / 2.0,
                            m_boxTop + m_panY + m_boxHeight / 2.0);
    return (pos - boxCenter) / m_contentZoom;
}

void VideoDisplayController::zoomAndPan(double newZoomValue, const QPointF &anchorPoint, const QPointF &newPos)
{
    // Upstream ZoomAndPan: newPan = newPosition - viewportCenter - anchorPoint * newZoomValue
    newZoomValue = std::clamp(newZoomValue, 0.125, 10.0);

    const QPointF boxCenterOffset(m_boxLeft + m_boxWidth / 2.0, m_boxTop + m_boxHeight / 2.0);
    const QPointF newPan = newPos - boxCenterOffset - anchorPoint * newZoomValue;

    m_panX = newPan.x();
    m_panY = newPan.y();
    m_contentZoom = newZoomValue;

    recalculateVideoLayout();
}

void VideoDisplayController::pan(double dx, double dy)
{
    m_panX += dx;
    m_panY += dy;
    recalculateVideoLayout();
}

// Mouse and keyboard interaction dispatch

void VideoDisplayController::mousePressed(qreal x, qreal y, int button, int modifiers)
{
    QPointF pos(x, y);
    m_lastMousePos = pos;
    Qt::KeyboardModifiers mods(modifiers);

    if (button == Qt::MiddleButton) {
        // Middle mouse button begins canvas panning.
        m_middlePanning = true;
        return;
    }

    if (m_currentTool == 0 && m_toolCross) {
        m_toolCross->mousePressed(pos, Qt::MouseButton(button), mods);
    } else if (m_currentTool == 1 && m_toolDrag) {
        m_toolDrag->mousePressed(pos, Qt::MouseButton(button), mods);
    } else if (m_currentTool == 2 && m_toolRotateZ) {
        m_toolRotateZ->mousePressed(pos, Qt::MouseButton(button), mods);
    } else if (m_currentTool == 4 && m_toolScale) {
        m_toolScale->mousePressed(pos, Qt::MouseButton(button), mods);
    } else if (m_currentTool == 5 && m_toolClip) {
        m_toolClip->mousePressed(pos, Qt::MouseButton(button), mods);
    }

    Q_EMIT visualDataChanged();
}

void VideoDisplayController::mouseMoved(qreal x, qreal y, int buttons, int modifiers)
{
    QPointF pos(x, y);
    Qt::KeyboardModifiers mods(modifiers);

    if (m_middlePanning && (buttons & Qt::MiddleButton)) {
        // Drag canvas while middle button is held.
        pan(pos.x() - m_lastMousePos.x(), pos.y() - m_lastMousePos.y());
        m_lastMousePos = pos;
        return;
    }

    m_lastMousePos = pos;

    if (m_currentTool == 0 && m_toolCross) {
        m_toolCross->mouseMoved(pos, Qt::MouseButtons(buttons), mods);
    } else if (m_currentTool == 1 && m_toolDrag) {
        m_toolDrag->mouseMoved(pos, Qt::MouseButtons(buttons), mods);
    } else if (m_currentTool == 2 && m_toolRotateZ) {
        m_toolRotateZ->mouseMoved(pos, Qt::MouseButtons(buttons), mods);
    } else if (m_currentTool == 4 && m_toolScale) {
        m_toolScale->mouseMoved(pos, Qt::MouseButtons(buttons), mods);
    } else if (m_currentTool == 5 && m_toolClip) {
        m_toolClip->mouseMoved(pos, Qt::MouseButtons(buttons), mods);
    }

    Q_EMIT visualDataChanged();
}

void VideoDisplayController::mouseReleased(qreal x, qreal y, int button, int modifiers)
{
    QPointF pos(x, y);
    Qt::KeyboardModifiers mods(modifiers);

    if (button == Qt::MiddleButton) {
        m_middlePanning = false;
        return;
    }

    if (m_currentTool == 0 && m_toolCross) {
        m_toolCross->mouseReleased(pos, Qt::MouseButton(button), mods);
    } else if (m_currentTool == 1 && m_toolDrag) {
        m_toolDrag->mouseReleased(pos, Qt::MouseButton(button), mods);
    } else if (m_currentTool == 2 && m_toolRotateZ) {
        m_toolRotateZ->mouseReleased(pos, Qt::MouseButton(button), mods);
    } else if (m_currentTool == 4 && m_toolScale) {
        m_toolScale->mouseReleased(pos, Qt::MouseButton(button), mods);
    } else if (m_currentTool == 5 && m_toolClip) {
        m_toolClip->mouseReleased(pos, Qt::MouseButton(button), mods);
    }

    Q_EMIT visualDataChanged();
}

void VideoDisplayController::mouseDoubleClicked(qreal x, qreal y, int button, int modifiers)
{
    QPointF pos(x, y);
    Qt::KeyboardModifiers mods(modifiers);

    if (button == Qt::MiddleButton) {
        // Double-clicking middle button resets pan and zoom to default fit.
        resetContentZoom();
        return;
    }

    if (m_currentTool == 0 && m_toolCross) {
        m_toolCross->mouseDoubleClicked(pos, Qt::MouseButton(button), mods);
    } else if (m_currentTool == 1 && m_toolDrag) {
        m_toolDrag->mouseDoubleClicked(pos, Qt::MouseButton(button), mods);
    } else if (m_currentTool == 2 && m_toolRotateZ) {
        m_toolRotateZ->mouseDoubleClicked(pos, Qt::MouseButton(button), mods);
    } else if (m_currentTool == 4 && m_toolScale) {
        m_toolScale->mouseDoubleClicked(pos, Qt::MouseButton(button), mods);
    } else if (m_currentTool == 5 && m_toolClip) {
        m_toolClip->mouseDoubleClicked(pos, Qt::MouseButton(button), mods);
    }

    Q_EMIT visualDataChanged();
}

void VideoDisplayController::mouseLeft()
{
    m_middlePanning = false;
    if (m_toolCross) m_toolCross->mouseLeft();
    if (m_toolDrag) m_toolDrag->mouseLeft();
    if (m_toolRotateZ) m_toolRotateZ->mouseLeft();
    if (m_toolScale) m_toolScale->mouseLeft();
    if (m_toolClip) m_toolClip->mouseLeft();
    Q_EMIT visualDataChanged();
}

void VideoDisplayController::wheel(qreal angleDeltaY, qreal x, qreal y, int modifiers)
{
    if (std::abs(angleDeltaY) < 0.1) return;

    const double dir = (angleDeltaY > 0) ? 1.0 : -1.0;
    const Qt::KeyboardModifiers mods(modifiers);

    // Aligns with upstream Video/{Scroll,Ctrl Scroll,Shift Scroll} Actions (0/2/4):
    //   No modifier = Resizes the video box (12.5% step)
    //   Ctrl        = Zooms the video (content zoom anchored to cursor)
    //   Shift       = Pans the video
    const bool isZoom = mods.testFlag(Qt::ControlModifier)
#if defined(Q_OS_MACOS)
        || mods.testFlag(Qt::MetaModifier)
#endif
    ;
    if (mods.testFlag(Qt::ShiftModifier)) {
        const double distance = 5.0 * dir;
        pan(0.0, distance);
    } else if (isZoom) {
        const QPointF mousePos(x, y);
        const QPointF anchor = getZoomAnchorPoint(mousePos);
        zoomAndPan(m_contentZoom * (1.0 + dir * 0.125), anchor, mousePos);
    } else {
        setWindowZoom(m_windowZoom + dir * 0.125);
    }
}

// Subtitle text synchronization and undo management

void VideoDisplayController::updateLiveSubtitleText(const QString &text)
{
    if (m_videoController) {
        m_videoController->setActiveSubtitle(m_videoController->activeSubStart(),
                                            m_videoController->activeSubEnd(),
                                            text);
    }
    Q_EMIT subtitleTextChanged(text);
    Q_EMIT visualDataChanged();
}

void VideoDisplayController::commitChanges(const QString &description)
{
    Q_EMIT commitRequested(description);
}

// Visual tool overlay data accessors for QML rendering

int VideoDisplayController::cursorShape() const
{
    if (m_middlePanning) return Qt::ClosedHandCursor;
    if (m_currentTool == 0 && m_toolCross) return m_toolCross->cursorShape();
    if (m_currentTool == 1 && m_toolDrag) return m_toolDrag->cursorShape();
    if (m_currentTool == 2 && m_toolRotateZ) return m_toolRotateZ->cursorShape();
    if (m_currentTool == 4 && m_toolScale) return m_toolScale->cursorShape();
    if (m_currentTool == 5 && m_toolClip) return m_toolClip->cursorShape();
    return Qt::ArrowCursor;
}

qreal VideoDisplayController::crosshairX() const
{
    return m_toolCross ? m_toolCross->mousePos().x() : -1.0;
}

qreal VideoDisplayController::crosshairY() const
{
    return m_toolCross ? m_toolCross->mousePos().y() : -1.0;
}

bool VideoDisplayController::crosshairVisible() const
{
    return (m_currentTool == 0) && m_toolCross && (m_toolCross->mousePos().x() >= 0 && m_toolCross->mousePos().y() >= 0);
}

QString VideoDisplayController::scriptCoordText() const
{
    return (m_currentTool == 0 && m_toolCross) ? m_toolCross->scriptCoordText() : QString();
}

QVariantList VideoDisplayController::dragPins() const
{
    return (m_currentTool == 1 && m_toolDrag) ? m_toolDrag->getPinsData() : QVariantList();
}

QVariantList VideoDisplayController::connectingLines() const
{
    return (m_currentTool == 1 && m_toolDrag) ? m_toolDrag->getConnectingLines() : QVariantList();
}

QVariantMap VideoDisplayController::rotationData() const
{
    return (m_currentTool == 2 && m_toolRotateZ) ? m_toolRotateZ->getRotationData() : QVariantMap();
}

QVariantMap VideoDisplayController::scaleData() const
{
    return (m_currentTool == 4 && m_toolScale) ? m_toolScale->getScaleData() : QVariantMap();
}

QVariantMap VideoDisplayController::clipData() const
{
    return (m_currentTool == 5 && m_toolClip) ? m_toolClip->getClipData() : QVariantMap();
}
