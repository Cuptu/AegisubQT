// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Aegisub Project http://www.aegisub.org/

#pragma once

#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>

class SpectrogramShaderMaterial : public QSGMaterial {
public:
    SpectrogramShaderMaterial();
    ~SpectrogramShaderMaterial() override = default;

    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode renderMode) const override;
    int compare(const QSGMaterial *other) const override;

    // Texture samplers.
    void setStftTexture(QSGTexture *tex) { m_stftTexture = tex; }
    QSGTexture* stftTexture() const { return m_stftTexture; }

    void setPaletteTexture(QSGTexture *tex) { m_paletteTexture = tex; }
    QSGTexture* paletteTexture() const { return m_paletteTexture; }

    // Shader uniform parameters.
    float viewportWidth = 800.0f;
    float viewportHeight = 160.0f;
    float scrollLeft = 0.0f;
    float msPerPixel = 10.0f;

    float sampleRate = 16000.0f;
    float hopSamples = 256.0f; // Synchronized with AegisubStftCore hop size constant (1 << derivationDist = 256)
    float totalFrames = 100.0f;
    float nbrBins = 512.0f;

    float maxFreq = 20000.0f;
    float freqRef = 1000.0f;
    float posFref = 0.001f;
    float amplitudeScale = 1.0f;

    float selStartMs = 0.0f;
    float selEndMs = 0.0f;

    float windowStartFrame = 0.0f;
    float windowFrameCount = 1.0f;
    float windowFrameStep = 1.0f;
    float viewportDeviceHeight = 1.0f;

private:
    QSGTexture *m_stftTexture = nullptr;
    QSGTexture *m_paletteTexture = nullptr;
};
