#include "audio_clip_ui_stubs.h"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QImage>
#include <QtTest/QTest>
#include <cstdio>
#include <memory>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    qmlRegisterType<TestVideoSurface>("Aegisub", 1, 0, "VideoSurface");
    qmlRegisterType<TestVideoSurface>("Aegisub", 1, 0, "SubtitleSurface");
    QQmlEngine engine;
    QQmlComponent mock(&engine);
    mock.setData(R"(import QtQml
QtObject {
    property bool hasVideo: true
    property bool isDummy: true
    property string dummyColor: "#19b35a"
    property int videoWidth: 1280
    property int videoHeight: 720
    property string videoPath: ""
    property real currentTime: 2.5
    property bool isPlaying: false
    signal positionChanged()
    signal playbackStateChanged()
    function seekTime(t) { currentTime = t; }
})", QUrl());
    std::unique_ptr<QObject> controller(mock.create());
    CHECK(controller);
    engine.rootContext()->setContextProperty("videoController", controller.get());
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(QML_SOURCE_DIR) + "/views/DetachedVideoWindow.qml"));
    std::unique_ptr<QObject> object(component.create());
    if (!object) for (const auto &error : component.errors()) fprintf(stderr, "%s\n", qPrintable(error.toString()));
    auto *window = qobject_cast<QQuickWindow *>(object.get());
    CHECK(window);
    auto *frame = object->findChild<QQuickItem *>("detached-video-frame");
    auto *subtitles = object->findChild<QQuickItem *>("detached-subtitles");
    auto *native = object->findChild<QQuickItem *>("detached-native-video");
    CHECK(frame && subtitles && native);
    window->resize(800, 600);
    window->show();
    QTest::qWait(150);
    CHECK(frame->width() == 800 && frame->height() == 450 && frame->y() == 75);
    CHECK(subtitles->size() == frame->size() && subtitles->position() == QPointF(0, 0));
    CHECK(!native->isVisible());
    // Capture the real software-rendered QML window: the dummy fill must cover
    // only the video rectangle, leaving black bars outside it.
    auto image = window->grabWindow();
    CHECK(!image.isNull());
    const double scale = static_cast<double>(image.width()) / window->width();
    CHECK(image.pixelColor(qRound(400 * scale), qRound(300 * scale)) == QColor("#19b35a"));
    CHECK(image.pixelColor(qRound(400 * scale), qRound(20 * scale)) == QColor(Qt::black));
    CHECK(controller->setProperty("videoWidth", 720) && controller->setProperty("videoHeight", 1280));
    QTest::qWait(30);
    CHECK(qAbs(frame->width() - 337.5) < 0.01 && frame->height() == 600);
    CHECK(subtitles->size() == frame->size());
    CHECK(controller->setProperty("isDummy", false));
    QTest::qWait(30);
    CHECK(native->isVisible()); // Native frame fallback before a decoder has video.
    CHECK(controller->setProperty("hasVideo", false));
    CHECK(!frame->isVisible() && !subtitles->isVisible());
    window->hide();
    puts("PASS detached dummy pixels, landscape/portrait letterbox geometry, subtitle geometry, native fallback and unload");
    return 0;
}
