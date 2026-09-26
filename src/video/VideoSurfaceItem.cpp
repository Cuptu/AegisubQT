// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Aegisub Project http://www.aegisub.org/

#include "VideoSurfaceItem.h"
#include "VideoController.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QDebug>

VideoSurfaceItem::VideoSurfaceItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
}

void VideoSurfaceItem::setController(VideoController *ctrl)
{
    if (m_videoController == ctrl) return;
    if (m_videoController) {
        disconnect(m_videoController, nullptr, this, nullptr);
    }
    m_videoController = ctrl;
    if (m_videoController) {
        // Frame arrival copies local reference on GUI thread; render thread reads only
        // the local copy under mutex to avoid concurrent access to VideoController's QImage.
        connect(m_videoController, &VideoController::frameImageChanged, this, [this]() {
            {
                QMutexLocker locker(&m_imageMutex);
                m_pendingImage = m_videoController->currentFrameImage();
            }
            m_frameDirty = true;
            update();
        });
        {
            QMutexLocker locker(&m_imageMutex);
            m_pendingImage = m_videoController->currentFrameImage();
        }
    } else {
        {
            QMutexLocker locker(&m_imageMutex);
            m_pendingImage = QImage();
        }
    }
    m_frameDirty = true;
    update();
    Q_EMIT controllerChanged();
}

QSGNode *VideoSurfaceItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    QImage img;
    {
        QMutexLocker locker(&m_imageMutex);
        img = m_pendingImage;
    }

    if (img.isNull() || width() <= 0 || height() <= 0 || !window()) {
        delete oldNode;
        return nullptr;
    }

    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Linear);
        m_frameDirty = true;
    }

    if (m_frameDirty) {
        m_frameDirty = false;
        QSGTexture *tex = window()->createTextureFromImage(img);
        if (tex) {
            tex->setFiltering(QSGTexture::Linear);
            if (node->texture()) {
                delete node->texture();
            }
            node->setTexture(tex);
        } else {
            delete node;
            return nullptr;
        }
    }

    node->setRect(QRectF(0, 0, width(), height()));
    return node;
}

void VideoSurfaceItem::releaseResources()
{
    // Re-upload current frame after Scene Graph reconstruction
    m_frameDirty = true;
}