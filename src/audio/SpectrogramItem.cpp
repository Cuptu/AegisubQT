// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Aegisub Project http://www.aegisub.org/

#include "SpectrogramItem.h"
#include <QPainter>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGSimpleTextureNode>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace {
inline quint64 bitsOf(double v)
{
    quint64 u = 0;
    static_assert(sizeof(u) == sizeof(v), "double must be 64-bit");
    std::memcpy(&u, &v, sizeof(u));
    return u;
}

inline void hashMix(quint64 &h, quint64 v)
{
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
}
} // namespace

SpectrogramItem::SpectrogramItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

SpectrogramItem::~SpectrogramItem()
{
    // QSGTexture must be destroyed while render context is valid.
    // deleteLater defers cleanup to the render thread event loop.
    if (m_stftGpuTexture) m_stftGpuTexture->deleteLater();
    if (m_paletteGpuTexture) m_paletteGpuTexture->deleteLater();
}

void SpectrogramItem::releaseResources()
{
    // Early release of GPU textures: scene graph and render thread remain active, ensuring
    // deleteLater is processed by the render thread event loop.
    if (m_stftGpuTexture) {
        m_stftGpuTexture->deleteLater();
        m_stftGpuTexture = nullptr;
    }
    if (m_paletteGpuTexture) {
        m_paletteGpuTexture->deleteLater();
        m_paletteGpuTexture = nullptr;
    }
}

quint64 SpectrogramItem::rulerStateKey(int width) const
{
    quint64 h = 0xcbf29ce484222325ULL;
    hashMix(h, bitsOf(static_cast<double>(width)));
    if (m_audioController) {
        hashMix(h, bitsOf(m_audioController->msPerPixel()));
        hashMix(h, bitsOf(static_cast<double>(m_audioController->scrollLeft())));
        hashMix(h, bitsOf(m_audioController->duration()));
    }
    return h;
}

quint64 SpectrogramItem::markerStateKey(int width, int height) const
{
    quint64 h = 0xcbf29ce484222325ULL ^ 0x5bf03635ULL;
    hashMix(h, bitsOf(static_cast<double>(width)));
    hashMix(h, bitsOf(static_cast<double>(height)));
    if (m_audioController) {
        hashMix(h, bitsOf(m_audioController->msPerPixel()));
        hashMix(h, bitsOf(static_cast<double>(m_audioController->scrollLeft())));
        hashMix(h, bitsOf(static_cast<double>(m_audioController->selectionStart())));
        hashMix(h, bitsOf(static_cast<double>(m_audioController->selectionEnd())));
        hashMix(h, bitsOf(m_audioController->currentTime()));
        hashMix(h, bitsOf(static_cast<double>(m_audioController->trackCursorMs())));
        // Previous and current video frame bounding boxes depend on frame number and timestamp.
        hashMix(h, bitsOf(static_cast<double>(m_audioController->videoFrame())));
        hashMix(h, bitsOf(m_audioController->frameTimeMs(m_audioController->videoFrame())));
        hashMix(h, bitsOf(m_audioController->frameTimeMs(m_audioController->videoFrame() + 1)));
        hashMix(h, bitsOf(static_cast<double>(m_audioController->keyframes().size())));
        // Overlay content revision (subtitle lines, drawing option toggles).
        hashMix(h, bitsOf(static_cast<double>(m_audioController->overlayRevision())));
    }
    return h;
}

void SpectrogramItem::setAudioController(AudioController *ctrl)
{
    if (m_audioController != ctrl) {
        if (m_audioController) {
            disconnect(m_audioController, nullptr, this, nullptr);
        }

        m_audioController = ctrl;

        if (m_audioController) {
            connect(m_audioController, &AudioController::scrollChanged, this, [this]() { update(); });
            connect(m_audioController, &AudioController::zoomChanged, this, [this]() { update(); });
            connect(m_audioController, &AudioController::selectionChanged, this, [this]() { update(); });
            connect(m_audioController, &AudioController::verticalZoomChanged, this, [this]() { update(); });
            connect(m_audioController, &AudioController::spectrumModeChanged, this, [this]() { update(); });
            connect(m_audioController, &AudioController::audioInfoChanged, this, [this]() {
                // Invalidate textures when audio source or spectrum settings change.
                m_lastStftRevision = ~quint64(0);
                m_rulerKey = ~quint64(0);
                m_markerKey = ~quint64(0);
                update();
            });
            connect(m_audioController, &AudioController::keyframesChanged, this, [this]() {
                m_markerKey = ~quint64(0); // Invalidate marker overlay on keyframe updates.
                update();
            });
            connect(m_audioController, &AudioController::positionChanged, this, [this]() { update(); });
            connect(m_audioController, &AudioController::cursorChanged, this, [this]() { update(); });
        }

        Q_EMIT audioControllerChanged();
        update();
    }
}

void SpectrogramItem::renderTimelineRulerImage(int width)
{
    if (width <= 0 || !m_audioController) return;

    const int bottom = 17;
    if (m_rulerImage.size() != QSize(width, bottom)) {
        m_rulerImage = QImage(width, bottom, QImage::Format_ARGB32_Premultiplied);
    }

    QPainter painter(&m_rulerImage);
    painter.setRenderHint(QPainter::Antialiasing, false);

    double msPerPx = m_audioController->msPerPixel();
    int scrollLeft = m_audioController->scrollLeft();
    if (msPerPx <= 0) return;

    QColor darkCol(8, 4, 13);
    QColor lightCol(89, 145, 220);

    // 1. Ruler background.
    painter.fillRect(0, 0, width, bottom, darkCol);

    // 2. Baseline separator line.
    painter.setPen(QPen(lightCol, 1));
    painter.drawLine(0, bottom - 1, width, bottom - 1);

    // 3. Time division scale matching Aegisub AudioDisplayTimeline.
    double pxSec = 1000.0 / msPerPx;
    double scaleMinorDivisor = 1000.0;
    int scaleMajorModulo = 10;
    enum Scale { Sc_Milli, Sc_Centi, Sc_Deci, Sc_Sec, Sc_DecaSec, Sc_Min, Sc_DecaMin, Sc_Hour };
    Scale scaleMinor = Sc_Sec;

    if (pxSec > 3000) {
        scaleMinor = Sc_Milli;
        scaleMinorDivisor = 1.0;
        scaleMajorModulo = 10;
    } else if (pxSec > 300) {
        scaleMinor = Sc_Centi;
        scaleMinorDivisor = 10.0;
        scaleMajorModulo = 10;
    } else if (pxSec > 30) {
        scaleMinor = Sc_Deci;
        scaleMinorDivisor = 100.0;
        scaleMajorModulo = 10;
    } else if (pxSec > 3) {
        scaleMinor = Sc_Sec;
        scaleMinorDivisor = 1000.0;
        scaleMajorModulo = 10;
    } else if (pxSec > 1.0 / 3.0) {
        scaleMinor = Sc_DecaSec;
        scaleMinorDivisor = 10000.0;
        scaleMajorModulo = 6;
    } else if (pxSec > 1.0 / 9.0) {
        scaleMinor = Sc_Min;
        scaleMinorDivisor = 60000.0;
        scaleMajorModulo = 10;
    } else if (pxSec > 1.0 / 90.0) {
        scaleMinor = Sc_DecaMin;
        scaleMinorDivisor = 600000.0;
        scaleMajorModulo = 6;
    } else {
        scaleMinor = Sc_Hour;
        scaleMinorDivisor = 3600000.0;
        scaleMajorModulo = 10;
    }

    int msLeft = static_cast<int>(scrollLeft * msPerPx);
    int nextScaleMark = static_cast<int>(msLeft / scaleMinorDivisor);
    if (nextScaleMark * scaleMinorDivisor < msLeft)
        nextScaleMark += 1;

    QFont font("Segoe UI", 8);
    painter.setFont(font);
    QFontMetrics fm(font);

    int nextScaleMarkPos = 0;
    int lastTextRight = -1;
    const double totalDur = m_audioController ? m_audioController->duration() : 0.0;
    int lastHour = (totalDur < 3600.0) ? 0 : -1;
    int lastMinute = -1;

    do {
        nextScaleMarkPos = static_cast<int>(std::round((nextScaleMark * scaleMinorDivisor / msPerPx) - scrollLeft));
        bool markIsMajor = (nextScaleMark % scaleMajorModulo == 0);

        painter.setPen(QPen(lightCol, 1));
        if (markIsMajor) {
            painter.drawLine(nextScaleMarkPos, bottom - 6, nextScaleMarkPos, bottom - 1);
        } else {
            painter.drawLine(nextScaleMarkPos, bottom - 4, nextScaleMarkPos, bottom - 1);
        }

        if (markIsMajor && nextScaleMarkPos > lastTextRight) {
            double markTime = nextScaleMark * scaleMinorDivisor / 1000.0;
            int markHour = static_cast<int>(markTime / 3600.0);
            int markMinute = static_cast<int>(markTime / 60.0) % 60;
            double markSecond = markTime - markHour * 3600.0 - markMinute * 60.0;

            QString timeString;
            bool changedHour = (markHour != lastHour);
            bool changedMinute = (markMinute != lastMinute);

            if (changedHour) {
                timeString = QString("%1:%2:").arg(markHour).arg(markMinute, 2, 10, QChar('0'));
                lastHour = markHour;
                lastMinute = markMinute;
            } else if (changedMinute) {
                timeString = QString("%1:").arg(markMinute);
                lastMinute = markMinute;
            }

            if (scaleMinor >= Sc_Deci) {
                timeString += QString("%1").arg(static_cast<int>(markSecond), 2, 10, QChar('0'));
            } else if (scaleMinor == Sc_Centi) {
                timeString += QString::asprintf("%04.1f", markSecond);
            } else {
                timeString += QString::asprintf("%05.2f", markSecond);
            }

            int tw = fm.horizontalAdvance(timeString);
            lastTextRight = nextScaleMarkPos + tw + 4;
            painter.drawText(nextScaleMarkPos, fm.ascent() + 1, timeString);
        }

        nextScaleMark += 1;
    } while (nextScaleMarkPos < width);
}

void SpectrogramItem::renderMarkersOverlayImage(int width, int height)
{
    if (width <= 0 || height <= 17 || !m_audioController) return;

    if (m_markerImage.size() != QSize(width, height)) {
        m_markerImage = QImage(width, height, QImage::Format_ARGB32_Premultiplied);
    }
    m_markerImage.fill(Qt::transparent);

    QPainter painter(&m_markerImage);
    painter.setRenderHint(QPainter::Antialiasing, false);

    double msPerPx = m_audioController->msPerPixel();
    int scrollLeft = m_audioController->scrollLeft();
    if (msPerPx <= 0) return;

    int audioTop = 17;
    int audioBottom = height - 1;

    // Palette colors and stroke widths matching native Aegisub default_config.json:
    //   Keyframe:            rgb(255,0,255) 1px solid (only rendered when authentic keyframes exist)
    //   Seconds Line:        rgb(0,100,255) 1px dotted
    //   Line Boundary Start: rgb(216,0,0)   solid Audio/Line Boundaries Thickness (2px) with rightward foot
    //   Line Boundary End:   rgb(0,0,216)   solid 2px with leftward foot
    //   Inactive Line:       rgb(190,190,190) solid 2px
    //   Play Cursor:         rgb(255,255,255) solid 1px
    //   Prev Frame Range:    rgba(255,255,255,200) filled rect
    //   Curr Frame Range:    rgba(255,255,255,160) filled rect
    //   Foot size:           AudioDisplay::foot_size = 6
    const QColor kSecondsLine(0, 100, 255);
    const QColor kKeyframe(255, 0, 255);
    const QColor kLineBoundaryStart(216, 0, 0);
    const QColor kLineBoundaryEnd(0, 0, 216);
    const QColor kLineBoundaryInactive(190, 190, 190);
    const QColor kSyllableBoundary(255, 255, 0);
    const QColor kPlayCursor(255, 255, 255);
    const QColor kPrevFrameRange(255, 255, 255, 200);
    const QColor kCurrFrameRange(255, 255, 255, 160);
    const int kLineBoundaryThickness = m_audioController->lineBoundaryThickness();
    const int footSize = 6;                 // AudioDisplay::foot_size

    // Projects time in milliseconds to screen pixels (matches native RelativeXFromTime).
    auto relX = [scrollLeft, msPerPx](double ms) {
        return static_cast<int>(std::floor(ms / msPerPx)) - scrollLeft;
    };

    // 1. Video frame range rectangles (VideoPositionRange, controlled by Audio/Display/Draw/Video Position).
    if (m_audioController->hasVideo() && m_audioController->drawVideoPosition()) {
        const int frame = m_audioController->videoFrame();
        // range1 = previous frame [TimeAtFrame(n-1), TimeAtFrame(n)], range2 = current frame.
        const double tPrev = m_audioController->frameTimeMs(frame - 1);
        const double tCurr = m_audioController->frameTimeMs(frame);
        const double tNext = m_audioController->frameTimeMs(frame + 1);
        const double ranges[2][2] = { { tPrev + 1.0, tCurr - tPrev }, { tCurr + 1.0, tNext - tCurr } };
        const QColor rangeColors[2] = { kPrevFrameRange, kCurrFrameRange };
        for (int i = 0; i < 2; ++i) {
            const double pos = ranges[i][0];
            const double w = std::max(1.0, ranges[i][1]);
            const int x1 = relX(pos);
            const int x2 = relX(pos + w - 1.0);
            if (x2 < 0 || x1 >= width) continue;
            painter.setPen(Qt::NoPen);
            painter.setBrush(rangeColors[i]);
            painter.drawRect(QRect(x1, audioTop, std::max(1, x2 - x1 + 1), audioBottom - audioTop + 1));
        }
        painter.setBrush(Qt::NoBrush);
    }

    // 2. Seconds grid lines (Audio/Display/Draw/Seconds): 1px dotted line at every 1000ms boundary.
    if (m_audioController->drawSeconds()) {
        const int startSec = static_cast<int>((scrollLeft * msPerPx + 999.0) / 1000.0);
        const int endSec = static_cast<int>(((scrollLeft + width) * msPerPx) / 1000.0) + 1;
        painter.setPen(QPen(kSecondsLine, 1, Qt::DotLine));
        for (int s = startSec; s <= endSec; ++s) {
            const int px = relX(s * 1000.0);
            if (px >= 0 && px < width) {
                painter.drawLine(px, audioTop, px, audioBottom);
            }
        }
    }

    // 3. Inactive line boundaries (Audio/Inactive Lines Display Mode, gray lines without feet).
    if (m_audioController->drawInactiveLines()) {
        painter.setPen(QPen(kLineBoundaryInactive, kLineBoundaryThickness));
        const QVector<AudioController::LineBoundaryMark> inactive =
            m_audioController->inactiveLineBoundaries();
        for (const auto &mark : inactive) {
            const int x1 = relX(mark.startMs);
            const int x2 = relX(mark.endMs);
            if (x1 >= 0 && x1 < width) painter.drawLine(x1, audioTop, x1, audioBottom);
            if (x2 >= 0 && x2 < width) painter.drawLine(x2, audioTop, x2, audioBottom);
        }
    }

    // 4. Karaoke syllable boundaries (Colour/Audio Display/Syllable Boundaries, 1 boundary per syllable after first).
    const QVector<AudioController::SyllableMark> syllables = m_audioController->syllableMarks();
    if (!syllables.isEmpty()) {
        painter.setPen(QPen(kSyllableBoundary, kLineBoundaryThickness));
        for (const auto &syl : syllables) {
            if (syl.isFirst) continue;
            const int px = relX(syl.startMs);
            if (px >= 0 && px < width) painter.drawLine(px, audioTop, px, audioBottom);
        }
    }

    // 5. Video keyframe markers (magenta, 1px solid, drawn only when keyframes exist).
    if (m_audioController->drawKeyframes()) {
        const QVariantList kfs = m_audioController->keyframes();
        painter.setPen(QPen(kKeyframe, 1));
        for (const auto &val : kfs) {
            const int px = relX(val.toDouble());
            if (px >= 0 && px < width) {
                painter.drawLine(px, audioTop, px, audioBottom);
            }
        }
    }

    // 6. Active line boundary markers with directional feet.
    const int selStart = m_audioController->selectionStart();
    const int selEnd = m_audioController->selectionEnd();
    const int selStartPx = relX(selStart);
    const int selEndPx = relX(selEnd);

    auto paintBoundary = [&](int px, const QColor &color, int dir) {
        if (px < -footSize || px > width + footSize) return;
        painter.setPen(QPen(color, kLineBoundaryThickness));
        painter.drawLine(px, audioTop, px, audioBottom);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        // Matches native PaintFoot: right triangle spanning footSize in direction dir.
        QPoint topFoot[3] = { QPoint(px + footSize * dir, audioTop), QPoint(px, audioTop), QPoint(px, audioTop + footSize) };
        QPoint botFoot[3] = { QPoint(px + footSize * dir, audioBottom), QPoint(px, audioBottom - footSize), QPoint(px, audioBottom) };
        painter.drawPolygon(topFoot, 3);
        painter.drawPolygon(botFoot, 3);
        painter.setBrush(Qt::NoBrush);
    };
    paintBoundary(selStartPx, kLineBoundaryStart, 1);
    paintBoundary(selEndPx, kLineBoundaryEnd, -1);

    // 7. Playback cursor (Play Cursor, controlled by Audio/Display/Draw/Video Position).
    if (m_audioController->drawVideoPosition()) {
        const int playPx = relX(m_audioController->currentTime() * 1000.0);
        if (playPx >= 0 && playPx < width) {
            painter.setPen(QPen(kPlayCursor, 1));
            painter.drawLine(playPx, audioTop, playPx, audioBottom);
        }
    }

    // 8. Karaoke syllable text labels (centered in syllable interval, left-aligned clipped on overflow).
    if (!syllables.isEmpty()) {
        QFont font(QStringLiteral("Verdana"));   // Audio/Karaoke/Font Face default
        font.setPointSize(9);                    // Audio/Karaoke/Font Size default
        font.setBold(true);
        painter.setFont(font);
        QFontMetrics fm(font);
        painter.setPen(QColor(255, 255, 255));
        for (const auto &syl : syllables) {
            if (syl.text.trimmed().isEmpty()) continue;
            const int left = relX(syl.startMs);
            const int right = relX(syl.endMs);
            const int span = right - left;
            if (right < 0 || left >= width) continue;
            const int tw = fm.horizontalAdvance(syl.text);
            if (span < tw) {
                painter.save();
                painter.setClipRect(left, audioTop + 4, std::max(1, span), fm.height());
                painter.drawText(left, audioTop + 4 + fm.ascent(), syl.text);
                painter.restore();
            } else {
                painter.drawText(left + (span - tw) / 2, audioTop + 4 + fm.ascent(), syl.text);
            }
        }
    }

    // 9. Hover tracking cursor: white solid line; timestamp label controlled by Audio/Display/Draw/Cursor Time.
    const int cursorMs = m_audioController->trackCursorMs();
    if (cursorMs >= 0) {
        const int cursorPx = relX(cursorMs);
        if (cursorPx >= 0 && cursorPx < width) {
            painter.setPen(QPen(QColor(255, 255, 255), 1));
            painter.drawLine(cursorPx, audioTop, cursorPx, audioBottom);

            const QString label = m_audioController->trackCursorText();
            if (m_audioController->drawCursorTime() && !label.isEmpty()) {
                QFont font("Segoe UI", 9, QFont::Bold);
                painter.setFont(font);
                QFontMetrics fm(font);
                const int tw = fm.horizontalAdvance(label);
                const int lx = std::clamp(cursorPx - tw / 2, 2, width - tw - 2);
                const int ly = audioTop + 2 + fm.ascent();

                // 1px outline: 4-directional offsets matching Aegisub wxColour(64,64,64).
                painter.setPen(QColor(64, 64, 64));
                painter.drawText(lx + 1, ly + 1, label);
                painter.drawText(lx + 1, ly - 1, label);
                painter.drawText(lx - 1, ly + 1, label);
                painter.drawText(lx - 1, ly - 1, label);

                // Primary label glyphs.
                painter.setPen(QColor(255, 255, 255));
                painter.drawText(lx, ly, label);
            }
        }
    }
}

QSGNode *SpectrogramItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (width() <= 0 || height() <= 0 || !m_audioController || !m_audioController->hasAudio() || !window()) {
        delete oldNode;
        return nullptr;
    }

    QSGNode *root = oldNode;
    QSGGeometryNode *audioNode = nullptr;
    QSGSimpleTextureNode *rulerNode = nullptr;
    QSGSimpleTextureNode *markerNode = nullptr;

    if (!root) {
        root = new QSGNode();

        // 1. Spectrogram scene graph node.
        audioNode = new QSGGeometryNode();
        QSGGeometry *geom = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
        geom->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        audioNode->setGeometry(geom);
        audioNode->setFlag(QSGNode::OwnsGeometry);

        auto *mat = new SpectrogramShaderMaterial();
        audioNode->setMaterial(mat);
        audioNode->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(audioNode);

        // 2. Timeline ruler node (top 17px).
        rulerNode = new QSGSimpleTextureNode();
        root->appendChildNode(rulerNode);

        // 3. Marker and selection feet overlay node.
        markerNode = new QSGSimpleTextureNode();
        root->appendChildNode(markerNode);
    } else {
        audioNode = static_cast<QSGGeometryNode*>(root->childAtIndex(0));
        rulerNode = static_cast<QSGSimpleTextureNode*>(root->childAtIndex(1));
        markerNode = (root->childCount() > 2) ? static_cast<QSGSimpleTextureNode*>(root->childAtIndex(2)) : nullptr;
    }

    const int w = static_cast<int>(width());
    const int h = static_cast<int>(height());
    const int audioH = std::max(1, h - 17);
    const qreal dpr = window()->devicePixelRatio();

    auto &stftCore = const_cast<AegisubStftCore&>(m_audioController->stftCore());
    const int scrollLeft = m_audioController->scrollLeft();
    const double msPerPx = m_audioController->msPerPixel();
    const int sr = stftCore.sampleRate();
    const int hop = stftCore.hopSamples();
    if (sr <= 0 || hop <= 0) {
        return root;
    }

    const int visCenterMs = static_cast<int>((scrollLeft + w * 0.5) * msPerPx);
    const int visCenterFrame = (visCenterMs * sr / 1000) / hop;
    const int visSpanFrames = static_cast<int>((w * msPerPx * sr / 1000) / hop);

    // Maintain sliding window across visible span; recomputation occurs on boundary crossing.
    stftCore.ensureWindow(m_audioController->pcmProvider(), visCenterFrame, visSpanFrames);

    // Re-upload STFT power texture when the underlying buffer revision changes
    // or the GPU texture was released (e.g. via releaseResources) and needs recreation.
    if ((m_lastStftRevision != stftCore.revision() || !m_stftGpuTexture) && !stftCore.stftTexture().isNull()) {
        m_lastStftRevision = stftCore.revision();
        if (m_stftGpuTexture) {
            m_stftGpuTexture->deleteLater();
            m_stftGpuTexture = nullptr;
        }
        m_stftGpuTexture = window()->createTextureFromImage(stftCore.stftTexture(), QQuickWindow::TextureIsOpaque);
        // Nearest-neighbor horizontal sampling matches Aegisub column-wise STFT indexing.
        // Vertical interpolation or interval maximum evaluation is performed in shader.
        m_stftGpuTexture->setFiltering(QSGTexture::Nearest);
        m_stftGpuTexture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
        m_stftGpuTexture->setVerticalWrapMode(QSGTexture::ClampToEdge);
    }

    if (!stftCore.paletteTexture().isNull() && !m_paletteGpuTexture) {
        m_paletteGpuTexture = window()->createTextureFromImage(stftCore.paletteTexture());
        m_paletteGpuTexture->setFiltering(QSGTexture::Linear);
        m_paletteGpuTexture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
        m_paletteGpuTexture->setVerticalWrapMode(QSGTexture::ClampToEdge);
    }

    if (!m_stftGpuTexture || !m_paletteGpuTexture) {
        delete root;
        return nullptr; // Return early if texture buffers are not ready.
    }

    auto *mat = static_cast<SpectrogramShaderMaterial*>(audioNode->material());
    mat->setStftTexture(m_stftGpuTexture);
    mat->setPaletteTexture(m_paletteGpuTexture);

    QRectF audioRect(0, 17, w, audioH);
    QSGGeometry::updateTexturedRectGeometry(audioNode->geometry(), audioRect, QRectF(0, 0, 1, 1));
    audioNode->markDirty(QSGNode::DirtyGeometry);

    mat->viewportWidth = static_cast<float>(w);
    mat->viewportHeight = static_cast<float>(audioH);
    mat->scrollLeft = static_cast<float>(scrollLeft);
    mat->msPerPixel = static_cast<float>(msPerPx);
    mat->sampleRate = static_cast<float>(sr);
    mat->hopSamples = static_cast<float>(hop);
    mat->totalFrames = static_cast<float>(stftCore.totalFrames());
    mat->nbrBins = static_cast<float>(stftCore.nbrBins());
    mat->maxFreq = stftCore.maxFreq;
    mat->freqRef = stftCore.freqRef;
    mat->posFref = stftCore.posFref;
    mat->amplitudeScale = static_cast<float>(m_audioController->amplitudeScale());
    mat->selStartMs = static_cast<float>(m_audioController->selectionStart());
    mat->selEndMs = static_cast<float>(m_audioController->selectionEnd());
    mat->windowStartFrame = static_cast<float>(stftCore.windowStartFrame());
    mat->windowFrameCount = static_cast<float>(stftCore.windowFrameCount());
    mat->windowFrameStep = static_cast<float>(stftCore.windowFrameStep());
    // Vertical sampling branch depends on physical device pixels matching Aegisub viewport.
    mat->viewportDeviceHeight = static_cast<float>(audioH * dpr);
    audioNode->markDirty(QSGNode::DirtyMaterial);

    // Reuse cached ruler texture if layout and timing parameters remain unchanged.
    const quint64 rKey = rulerStateKey(w);
    if (rKey != m_rulerKey || rulerNode->texture() == nullptr) {
        m_rulerKey = rKey;
        renderTimelineRulerImage(w);
        QSGTexture *rulerTex = window()->createTextureFromImage(m_rulerImage);
        rulerNode->setTexture(rulerTex);
        rulerNode->setOwnsTexture(true);
        rulerNode->setRect(0, 0, w, 17);
        rulerNode->markDirty(QSGNode::DirtyMaterial | QSGNode::DirtyGeometry);
    }

    // Reuse cached marker overlay texture if boundaries remain unchanged.
    if (markerNode) {
        const quint64 mKey = markerStateKey(w, h);
        if (mKey != m_markerKey || markerNode->texture() == nullptr) {
            m_markerKey = mKey;
            renderMarkersOverlayImage(w, h);
            QSGTexture *markerTex = window()->createTextureFromImage(m_markerImage, QQuickWindow::TextureHasAlphaChannel);
            markerNode->setTexture(markerTex);
            markerNode->setOwnsTexture(true);
            markerNode->setRect(0, 0, w, h);
            markerNode->markDirty(QSGNode::DirtyMaterial | QSGNode::DirtyGeometry);
        }
    }

    return root;
}
