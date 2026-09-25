// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Aegisub Project http://www.aegisub.org/

#include "SpectrogramShaderMaterial.h"
#include <cstring>
#include <QFile>
#include <QTextStream>

class SpectrogramRhiShader : public QSGMaterialShader {
public:
    SpectrogramRhiShader() {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/spectrogram.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/spectrogram.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override {
        Q_UNUSED(oldMaterial);
        QByteArray *buf = state.uniformData();
        if (!buf) return false;
        if (buf->size() < 144) buf->resize(144);

        auto *mat = static_cast<SpectrogramShaderMaterial*>(newMaterial);
        float *ptr = reinterpret_cast<float*>(buf->data());

        // Offset 0 (0..15): mat4 qt_Matrix
        const QMatrix4x4 m = state.combinedMatrix();
        std::memcpy(ptr, m.constData(), 16 * sizeof(float));

        // Offset 64 (16..19): vec4 u_viewParams (w, h, scrollLeft, msPerPixel)
        ptr[16] = mat->viewportWidth;
        ptr[17] = mat->viewportHeight;
        ptr[18] = mat->scrollLeft;
        ptr[19] = mat->msPerPixel;

        // Offset 80 (20..23): vec4 u_audioParams (sampleRate, hopSamples, totalFrames, nbrBins)
        ptr[20] = mat->sampleRate;
        ptr[21] = mat->hopSamples;
        ptr[22] = mat->totalFrames;
        ptr[23] = mat->nbrBins;

        // Offset 96 (24..27): vec4 u_curveParams (maxFreq, freqRef, posFref, amplitudeScale)
        ptr[24] = mat->maxFreq;
        ptr[25] = mat->freqRef;
        ptr[26] = mat->posFref;
        ptr[27] = mat->amplitudeScale;

        // Offset 112 (28..31): vec4 u_selParams (selStartMs, selEndMs, qt_Opacity, 0.0)
        ptr[28] = mat->selStartMs;
        ptr[29] = mat->selEndMs;
        ptr[30] = state.opacity();
        ptr[31] = 0.0f;

        // Offset 128 (32..35): vec4 u_windowParams
        //   (windowStartFrame, windowFrameCount, windowFrameStep, viewportDeviceHeight)
        ptr[32] = mat->windowStartFrame;
        ptr[33] = mat->windowFrameCount;
        ptr[34] = mat->windowFrameStep;
        ptr[35] = mat->viewportDeviceHeight;

        return true;
    }

    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                            QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override {
        Q_UNUSED(state);
        Q_UNUSED(oldMaterial);
        auto *mat = static_cast<SpectrogramShaderMaterial*>(newMaterial);
        if (binding == 1) {
            *texture = mat->stftTexture();
        } else if (binding == 2) {
            *texture = mat->paletteTexture();
        }

        if (*texture) {
            (*texture)->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
        }
    }
};

SpectrogramShaderMaterial::SpectrogramShaderMaterial()
{
    setFlag(Blending, false);
}

QSGMaterialType *SpectrogramShaderMaterial::type() const
{
    static QSGMaterialType s_type;
    return &s_type;
}

QSGMaterialShader *SpectrogramShaderMaterial::createShader(QSGRendererInterface::RenderMode renderMode) const
{
    Q_UNUSED(renderMode);
    return new SpectrogramRhiShader();
}

int SpectrogramShaderMaterial::compare(const QSGMaterial *other) const
{
    // Distinct SpectrogramItem instances hold separate textures and uniforms. Returning 0
    // would mislead the scene graph into invalid batching, blending textures or selections.
    // Sort by type -> texture pointers -> key uniforms so only identical materials batch together.
    if (other->type() != type())
        return (type() < other->type()) ? -1 : 1;
    const auto *o = static_cast<const SpectrogramShaderMaterial *>(other);
    if (m_stftTexture != o->m_stftTexture)
        return (m_stftTexture < o->m_stftTexture) ? -1 : 1;
    if (m_paletteTexture != o->m_paletteTexture)
        return (m_paletteTexture < o->m_paletteTexture) ? -1 : 1;
    if (scrollLeft != o->scrollLeft)
        return (scrollLeft < o->scrollLeft) ? -1 : 1;
    if (msPerPixel != o->msPerPixel)
        return (msPerPixel < o->msPerPixel) ? -1 : 1;
    if (amplitudeScale != o->amplitudeScale)
        return (amplitudeScale < o->amplitudeScale) ? -1 : 1;
    if (selStartMs != o->selStartMs)
        return (selStartMs < o->selStartMs) ? -1 : 1;
    if (selEndMs != o->selEndMs)
        return (selEndMs < o->selEndMs) ? -1 : 1;
    if (windowStartFrame != o->windowStartFrame)
        return (windowStartFrame < o->windowStartFrame) ? -1 : 1;
    return 0;
}
