// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Aegisub Project http://www.aegisub.org/

#pragma once

#include <QQuickItem>
#include <QSGNode>
#include <QSGGeometryNode>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QImage>
#include "AudioController.h"
#include "SpectrogramShaderMaterial.h"

class SpectrogramItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(AudioController* audioController READ audioController WRITE setAudioController NOTIFY audioControllerChanged)
    Q_PROPERTY(AudioController* controller READ audioController WRITE setAudioController NOTIFY audioControllerChanged)

public:
    explicit SpectrogramItem(QQuickItem *parent = nullptr);
    ~SpectrogramItem() override;

    AudioController* audioController() const { return m_audioController; }
    void setAudioController(AudioController *ctrl);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    // Releases GPU textures prior to window destruction while the scene graph and render thread
    // are still active. Relying solely on destructor cleanup risks leaks if the render thread stopped.
    void releaseResources() override;

Q_SIGNALS:
    void audioControllerChanged();

private:
    void renderTimelineRulerImage(int width);
    void renderMarkersOverlayImage(int width, int height);
    quint64 rulerStateKey(int width) const;
    quint64 markerStateKey(int width, int height) const;

    AudioController *m_audioController = nullptr;

    // GPU texture caches.
    QSGTexture *m_stftGpuTexture = nullptr;
    QSGTexture *m_paletteGpuTexture = nullptr;
    // STFT texture is re-uploaded only when AegisubStftCore revision changes.
    quint64 m_lastStftRevision = ~quint64(0);
    // State hashes for caching ruler and marker overlay textures.
    quint64 m_rulerKey = ~quint64(0);
    quint64 m_markerKey = ~quint64(0);

    QImage m_rulerImage;
    QImage m_markerImage;
};
