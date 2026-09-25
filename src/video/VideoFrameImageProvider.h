// Copyright (c) 2005 - 2026, Aegisub Project & Contributors
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QQuickImageProvider>
#include <QMutex>
#include <QMutexLocker>
#include <QImage>

/// Zero-copy in-memory QQuickImageProvider for video frames.
/// Eliminates intermediate disk JPEG serialization and file reloading,
/// directly passing decoded QImage buffers to the Qt Quick SceneGraph.
class VideoFrameImageProvider : public QQuickImageProvider {
public:
    VideoFrameImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {
        s_instance = this;
    }

    ~VideoFrameImageProvider() override {
        if (s_instance == this) {
            s_instance = nullptr;
        }
    }

    static VideoFrameImageProvider* instance() {
        return s_instance;
    }

    void setFrame(const QImage &image) {
        QMutexLocker locker(&m_mutex);
        m_frame = image;
    }

    QImage currentFrame() const {
        QMutexLocker locker(&m_mutex);
        return m_frame;
    }

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override {
        Q_UNUSED(id);
        QMutexLocker locker(&m_mutex);
        if (size) {
            *size = m_frame.size();
        }
        if (requestedSize.isValid() && requestedSize != m_frame.size()) {
            return m_frame.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        return m_frame;
    }

private:
    static inline VideoFrameImageProvider *s_instance = nullptr;
    mutable QMutex m_mutex;
    QImage m_frame;
};
