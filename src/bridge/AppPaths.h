#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace AppPaths {
inline bool portable() {
    return QFileInfo(QDir(QCoreApplication::applicationDirPath()).filePath("portable.txt")).isFile();
}
inline QString configDirectory() {
    return portable() ? QCoreApplication::applicationDirPath()
                      : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}
inline QString dataDirectory() {
    return portable() ? QDir(QCoreApplication::applicationDirPath()).filePath("data")
                      : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}
inline QString cacheDirectory() {
    return portable() ? QDir(QCoreApplication::applicationDirPath()).filePath("cache")
                      : QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}
}
