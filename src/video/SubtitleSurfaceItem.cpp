// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT
#include "SubtitleSurfaceItem.h"
#include "VideoController.h"
#include "../model/SubtitleModel.h"
#include <QPainter>
#include <QDebug>
#include <cmath>

SubtitleSurfaceItem::SubtitleSurfaceItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    if (!m_renderer.available()) qWarning() << "[SubtitleRenderer]" << m_renderer.error();
}
void SubtitleSurfaceItem::setController(VideoController *controller) {
    if (m_controller == controller) return;
    if (m_controller) disconnect(m_controller, nullptr, this, nullptr);
    m_controller = controller;
    if (m_controller) {
        connect(m_controller, &VideoController::positionChanged, this, [this] { refresh(); });
        connect(m_controller, &VideoController::videoInfoChanged, this, [this] { refresh(); });
    }
    refresh();
    emit controllerChanged();
}
void SubtitleSurfaceItem::setModel(SubtitleModel *model) {
    if (m_model == model) return;
    if (m_model) disconnect(m_model, nullptr, this, nullptr);
    m_model = model;
    if (m_model) {
        connect(m_model, &SubtitleModel::contentModified, this, [this] { refresh(true); });
        connect(m_model, &SubtitleModel::scriptInfoChanged, this, [this] { refresh(true); });
        connect(m_model, &SubtitleModel::stylesChanged, this, [this] { refresh(true); });
        connect(m_model, &QAbstractItemModel::modelReset, this, [this] { refresh(true); });
    }
    refresh(true);
    emit modelChanged();
}
void SubtitleSurfaceItem::refresh(bool documentChanged) {
    m_documentDirty |= documentChanged;
    if (!m_controller || !m_model || !m_controller->hasVideo()) {
        m_image = {};
        update();
        return;
    }
    m_model->initializeVideoResolution(m_controller->videoWidth(), m_controller->videoHeight());
    if (m_documentDirty) {
        m_documentDirty = false;
        m_renderer.setDocument(m_model->previewAss().toUtf8());
    }
    m_image = m_renderer.render(m_controller->videoWidth(), m_controller->videoHeight(),
                               qRound64(m_controller->currentTime() * 1000.0));
    update();
}
void SubtitleSurfaceItem::paint(QPainter *painter) {
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(boundingRect(), m_image);
}
