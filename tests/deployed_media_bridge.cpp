#include "AstraCoreBridge.h"
#include "MediaTools.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <cstdio>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2) return 2;
#ifdef Q_OS_WIN
    const QDir runtime(QCoreApplication::applicationDirPath() + "/assets/bin");
    const QString executable = QStringLiteral("ffmpeg.exe");
#else
    const QDir runtime(QCoreApplication::applicationDirPath() + "/../Resources/astracore");
    const QString executable = QStringLiteral("ffmpeg");
#endif
    const QString media = app.arguments().at(1);
    const QString expected = QFileInfo(runtime.filePath(executable)).canonicalFilePath();
    const QString actual = QFileInfo(MediaTools::ffmpegPath()).canonicalFilePath();
    if (expected.isEmpty() || actual != expected) {
        std::fprintf(stderr, "FFmpeg path mismatch: '%s' != '%s'\n",
            actual.toUtf8().constData(), expected.toUtf8().constData());
        return 3;
    }
    AstraCoreBridge bridge;
    MediaInfo info;
    if (!bridge.isAvailable() || bridge.abiVersion() != 5
        || !bridge.probe(media, info)
        || !info.hasVideo || !info.hasAudio) return 4;
    void *session = bridge.openVideoSession(media);
    if (!session) return 5;
    bool valid = true;
    for (double time : {0.3, 0.3, 0.6, 0.1, 0.9, 0.9}) {
        double actual = -1;
        const QImage frame = bridge.grabSessionFrame(session, time, 48, 32, &actual);
        if (frame.isNull() || frame.width() != 48 || frame.height() != 32
            || actual < time - 0.11 || actual > time + 0.11) valid = false;
    }
    bridge.closeVideoSession(session);
    if (!valid) return 6;
    std::puts("Application bridge resolved the bundled AstraCore and FFmpeg and decoded real session frames.");
    return 0;
}
