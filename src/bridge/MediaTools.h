#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace MediaTools {
inline QString ffmpegPath() {
#ifdef Q_OS_WIN
    const QString name = QStringLiteral("ffmpeg.exe");
#else
    const QString name = QStringLiteral("ffmpeg");
#endif
    const QDir app(QCoreApplication::applicationDirPath());
#ifdef Q_OS_MACOS
    if (app.dirName() == "MacOS") {
        const QFileInfo bundled(app.filePath("../Frameworks/astracore/" + name));
        if (bundled.isFile() && bundled.isExecutable()) return bundled.absoluteFilePath();
    }
#endif
    const QStringList locations = {app.filePath(name), app.filePath("assets/bin/" + name),
                                   app.filePath("tools/" + name),
                                   app.filePath("../libexec/AegisubQT/" + name)};
    for (const auto &path : locations) {
        const QFileInfo file(path);
        if (file.isFile() && file.isExecutable()) return file.absoluteFilePath();
    }
    return QStandardPaths::findExecutable(name);
}
}
