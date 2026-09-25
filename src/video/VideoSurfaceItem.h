// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Aegisub Project http://www.aegisub.org/

#pragma once

#include <QQuickItem>
#include <QSGNode>
#include <QSGTexture>
#include <QImage>
#include <QAtomicInt>
#include <QMutex>
#include <QMutexLocker>

class VideoController;

/// High-performance video frame rendering item: uploads current frame directly as a Scene Graph texture,
/// bypassing QQuickImageProvider and per-frame QML Image URL reassignment overhead on the GUI thread.
///
/// Mirrors upstream Aegisub's approach (video_display.cpp pending_frame -> VideoOutGL::UploadFrameData):
/// GPU texture creation occurs inside updatePaintNode on the scene graph render thread without GUI thread copies.
class VideoSurfaceItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(VideoController* controller READ controller
               WRITE setController NOTIFY controllerChanged)

public:
    explicit VideoSurfaceItem(QQuickItem *parent = nullptr);

    // Property named 'controller' to avoid shadowing the root context property 'videoController' in QML.
    VideoController* controller() const { return m_videoController; }
    void setController(VideoController *ctrl);

Q_SIGNALS:
    void controllerChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void releaseResources() override;

private:
    VideoController *m_videoController = nullptr;
    mutable QMutex m_imageMutex;
    QImage m_pendingImage;
    QAtomicInt m_frameDirty{1};
};