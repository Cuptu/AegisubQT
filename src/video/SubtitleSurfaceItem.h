// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT
#pragma once
#include <QQuickPaintedItem>
#include "SubtitleRenderer.h"
class VideoController;
class SubtitleModel;

class SubtitleSurfaceItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(VideoController* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(SubtitleModel* model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString rendererError READ rendererError NOTIFY rendererErrorChanged)
public:
    explicit SubtitleSurfaceItem(QQuickItem *parent = nullptr);
    VideoController *controller() const { return m_controller; }
    SubtitleModel *model() const { return m_model; }
    QString rendererError() const { return m_renderer.error(); }
    void setController(VideoController *controller);
    void setModel(SubtitleModel *model);
    void paint(QPainter *painter) override;
signals:
    void controllerChanged();
    void modelChanged();
    void rendererErrorChanged();
private:
    void refresh(bool documentChanged = false);
    VideoController *m_controller = nullptr;
    SubtitleModel *m_model = nullptr;
    SubtitleRenderer m_renderer;
    QImage m_image;
    bool m_documentDirty = true;
};
