// Copyright (c) 2005 - 2026, Aegisub Project & Contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

#include "LuaTextExtents.h"

#ifdef _WIN32
// Prevent windows.h min/max macros from polluting identifiers in this TU.
#define NOMINMAX
#include <windows.h>
#else
#include <QFont>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <cmath>
#endif

namespace Automation {

bool CalculateTextExtents(const AssStyleExtents &style, const QString &text, double &width, double &height, double &descent, double &extlead)
{
    width = height = descent = extlead = 0;

    // Scale font size and letter spacing by 64 to preserve 26.6 fixed-point fractional
    // precision when measuring through integer GDI metrics, matching VSFilter behavior.
    double fontsize = style.fontsize * 64.0;
    double spacing = style.spacing * 64.0;

#ifdef _WIN32
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return false;

    SetMapMode(dc, MM_TEXT);

    LOGFONTW lf = {0};
    lf.lfHeight = (LONG)fontsize;
    lf.lfWeight = style.bold ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = style.italic ? TRUE : FALSE;
    lf.lfUnderline = style.underline ? TRUE : FALSE;
    lf.lfStrikeOut = style.strikeout ? TRUE : FALSE;
    lf.lfCharSet = (BYTE)style.encoding;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

    std::wstring wfont = style.font.toStdWString();
    wcsncpy(lf.lfFaceName, wfont.c_str(), 31);
    lf.lfFaceName[31] = L'\0';

    HFONT font = CreateFontIndirectW(&lf);
    if (!font) {
        DeleteDC(dc);
        return false;
    }

    HGDIOBJ old_font = SelectObject(dc, font);

    std::wstring wtext = text.toStdWString();
    if (spacing != 0.0) {
        width = 0;
        for (wchar_t c : wtext) {
            SIZE sz = {0, 0};
            GetTextExtentPoint32W(dc, &c, 1, &sz);
            width += sz.cx + spacing;
            height = sz.cy;
        }
    } else {
        SIZE sz = {0, 0};
        GetTextExtentPoint32W(dc, wtext.c_str(), (int)wtext.size(), &sz);
        width = sz.cx;
        height = sz.cy;
    }

    TEXTMETRICW tm;
    GetTextMetricsW(dc, &tm);
    descent = tm.tmDescent;
    extlead = tm.tmExternalLeading;

    SelectObject(dc, old_font);
    DeleteObject(font);
    DeleteDC(dc);
#else
    // Non-Windows POSIX fallback: use QFontMetricsF for equivalent text extents.
    // Fails explicitly if no GUI application instance exists to prevent silent 0x0 metrics.
    if (!QGuiApplication::instance()) return false;

    QFont font(style.font);
    font.setBold(style.bold);
    font.setItalic(style.italic);
    font.setUnderline(style.underline);
    font.setStrikeOut(style.strikeout);
    font.setPixelSize(static_cast<int>(std::round(fontsize)));
    if (spacing != 0.0) {
        // Equivalent to GDI branch per-character spacing accumulation
        font.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
    }

    QFontMetricsF fm(font);
    width = fm.horizontalAdvance(text);
    height = fm.height();
    descent = fm.descent();
    extlead = fm.leading();
#endif

    // Unscale the 64x fixed-point multiplier and apply ASS style ScaleX/ScaleY percentages.
    width = (style.scalex / 100.0) * width / 64.0;
    height = (style.scaley / 100.0) * height / 64.0;
    descent = (style.scaley / 100.0) * descent / 64.0;
    extlead = (style.scaley / 100.0) * extlead / 64.0;

    return true;
}

} // namespace Automation

