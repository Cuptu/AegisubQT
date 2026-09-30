// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT
#pragma once

#include <QWindow>
#include <QBackingStore>
#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <functional>

// A frozen screen avoids sampling the picker itself or a changing video frame.
// Geometry is in logical desktop coordinates; captured pixels may be HiDPI.
class ScreenPickWindow final : public QWindow {
public:
    ScreenPickWindow(QScreen *screen, const QRect &geometry, QImage pixels,
                     std::function<void(QColor)> finish)
        : pixels_(std::move(pixels)), finish_(std::move(finish)), backing_(this) {
        setScreen(screen);
        setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setGeometry(geometry);
        setCursor(Qt::CrossCursor);
    }

    QColor sample(const QPointF &position) const {
        if (pixels_.isNull() || width() <= 0 || height() <= 0 ||
            position.x() < 0 || position.y() < 0 ||
            position.x() >= width() || position.y() >= height()) return {};
        return pixels_.pixelColor(qMin(pixels_.width() - 1,
            int(position.x() * pixels_.width() / width())),
            qMin(pixels_.height() - 1, int(position.y() * pixels_.height() / height())));
    }

protected:
    bool event(QEvent *event) override {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) finish_(sample(mouse->position()));
            else if (mouse->button() == Qt::RightButton) finish_({});
            return true;
        }
        if (event->type() == QEvent::KeyPress &&
            static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            finish_({});
            return true;
        }
        if (event->type() == QEvent::Close) {
            finish_({});
            return true;
        }
        return QWindow::event(event);
    }
    void exposeEvent(QExposeEvent *) override {
        if (!isExposed()) return;
        backing_.resize(size());
        backing_.beginPaint(QRect(QPoint(), size()));
        QPainter painter(backing_.paintDevice());
        painter.drawImage(QRect(QPoint(), size()), pixels_);
        painter.end();
        backing_.endPaint();
        backing_.flush(QRect(QPoint(), size()));
    }

private:
    QImage pixels_;
    std::function<void(QColor)> finish_;
    QBackingStore backing_;
};
