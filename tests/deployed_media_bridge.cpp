#include "AstraCoreBridge.h"
#include "MediaTools.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <cstdio>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2) return 2;
    const QDir runtime(QCoreApplication::applicationDirPath() + "/../Resources/astracore");
    if (QFileInfo(MediaTools::ffmpegPath()).canonicalFilePath()
        != QFileInfo(runtime.filePath("ffmpeg")).canonicalFilePath()) return 3;
    AstraCoreBridge bridge;
    MediaInfo info;
    if (!bridge.isAvailable() || bridge.abiVersion() != 5
        || !bridge.probe(QString::fromLocal8Bit(argv[1]), info)
        || !info.hasVideo || !info.hasAudio) return 4;
    void *session = bridge.openVideoSession(QString::fromLocal8Bit(argv[1]));
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
