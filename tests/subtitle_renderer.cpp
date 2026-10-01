// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT
#include "SubtitleRenderer.h"
#include <QGuiApplication>
#include <QRect>
#include <cstdio>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static QRect bounds(const QImage &image) {
    QRect result;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (qAlpha(image.pixel(x,y))) result |= QRect(x,y,1,1);
    return result;
}
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    SubtitleRenderer renderer(argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString());
    if (!renderer.available()) { fprintf(stderr, "%s\n", qPrintable(renderer.error())); return argc > 1 ? 1 : 77; }
    const QByteArray header = "[Script Info]\nScriptType: v4.00+\nPlayResX: 1280\nPlayResY: 720\nScaledBorderAndShadow: yes\n"
        "[V4+ Styles]\nFormat: Name,Fontname,Fontsize,PrimaryColour,SecondaryColour,OutlineColour,BackColour,Bold,Italic,Underline,StrikeOut,ScaleX,ScaleY,Spacing,Angle,BorderStyle,Outline,Shadow,Alignment,MarginL,MarginR,MarginV,Encoding\n"
        "Style: Default,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1\n"
        "[Events]\nFormat: Layer,Start,End,Style,Name,MarginL,MarginR,MarginV,Effect,Text\n";
    CHECK(renderer.setDocument(header + "Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,First line\\NSecond line\n"
        "Dialogue: 0,0:00:03.00,0:00:04.00,Default,,0,0,0,,{\\an7\\pos(100,100)}Positioned\n"));
    CHECK(bounds(renderer.render(1280,720,999)).isEmpty());
    const QRect bottom = bounds(renderer.render(1280,720,1000));
    CHECK(!bottom.isEmpty() && bottom.top() > 500 && bottom.bottom() < 720);
    CHECK(std::abs(bottom.center().x() - 640) < 10);
    CHECK(bounds(renderer.render(1280,720,2000)).isEmpty());
    const QRect positioned = bounds(renderer.render(1280,720,3000));
    CHECK(positioned.left() >= 95 && positioned.left() < 120 && positioned.top() >= 95 && positioned.top() < 125);
    const QRect scaled = bounds(renderer.render(640,360,1000));
    CHECK(!scaled.isEmpty() && std::abs(scaled.center().x()-320) < 6 && scaled.bottom() < 360);
    CHECK(std::abs(scaled.height()*2-bottom.height()) < 6);
    CHECK(renderer.setDocument(header + "Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,edited\n"));
    CHECK(bounds(renderer.render(1280,720,1000)).height() < bottom.height());
    CHECK(renderer.render(0,720,1000).isNull());
    puts("PASS real libass: multiline bottom alignment, margins, timed cues, ASS position, viewport scaling and edit refresh");
    return 0;
}
