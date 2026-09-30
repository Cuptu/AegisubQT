// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Aegisub Project http://www.aegisub.org/

#include "AegisubCoreBridge.h"
#include "MediaTools.h"
#include "AppPaths.h"
#include "KaraokeParser.h"
#include "ScreenColorPicker.h"
#include <QScreen>
#include <QPixmap>

#include <libaegisub/ass/dialogue_parser.h>
#include <libaegisub/ass/karaoke.h>
#include <libaegisub/ass/string_codec.h>
#include <libaegisub/ass/time.h>
#include <libaegisub/color.h>
#include <libaegisub/keyframe.h>
#include <libaegisub/vfr.h>

#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QStringConverter>
#include <QGuiApplication>
#include <QClipboard>
#include <QImage>
#include <QProcess>
#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <algorithm>

AegisubCoreBridge::AegisubCoreBridge(QObject *parent) : QObject(parent) {}

void AegisubCoreBridge::cancelScreenColorPick() {
    if (!screenPickSession_) return;
    auto *session = screenPickSession_.data();
    screenPickSession_.clear();
    for (auto *window : session->findChildren<QWindow *>()) window->hide();
    session->deleteLater();
    emit screenColorPickCancelled();
}

bool AegisubCoreBridge::beginScreenColorPick() {
    cancelScreenColorPick();
    // Capture every screen before creating any overlay, including negative origins.
    QList<QPair<QScreen *, QImage>> screens;
    for (auto *screen : QGuiApplication::screens()) {
        auto pixels = screen->grabWindow(0).toImage();
        if (pixels.isNull()) {
            emit screenColorPickFailed(tr("Screen capture is unavailable. Check screen recording permissions."));
            return false;
        }
        screens.append({screen, pixels});
    }
    if (screens.isEmpty()) {
        emit screenColorPickFailed(tr("No screen is available for colour picking."));
        return false;
    }
    auto *session = new QObject(this);
    screenPickSession_ = session;
    const QPointer<QObject> guard(session);
    auto finish = [this, guard](QColor color) {
        if (!guard || screenPickSession_ != guard) return;
        screenPickSession_.clear();
        for (auto *window : guard->findChildren<QWindow *>()) window->hide();
        guard->deleteLater();
        if (color.isValid()) emit screenColorPicked(color);
        else emit screenColorPickCancelled();
    };
    for (const auto &screen : screens) {
        auto *window = new ScreenPickWindow(screen.first, screen.first->geometry(), screen.second, finish);
        window->QObject::setParent(session);
        connect(screen.first, &QObject::destroyed, session, [finish] { finish({}); });
        window->show();
        window->requestActivate();
    }
    return true;
}

QList<QVariantMap> AegisubCoreBridge::tokenizeLine(const QString &text, bool karaokeTemplater) {
    QList<QVariantMap> result;
    QByteArray utf8 = text.toUtf8();
    auto tokens = agi::ass::TokenizeDialogueBody(std::string_view(utf8.constData(), utf8.size()), karaokeTemplater);

    result.reserve(static_cast<qsizetype>(tokens.size()));
    for (auto const& tok : tokens) {
        QVariantMap map;
        map[QStringLiteral("type")] = tok.type;
        map[QStringLiteral("length")] = static_cast<int>(tok.length);
        result.append(map);
    }
    return result;
}

QList<QVariantMap> AegisubCoreBridge::parseKaraokeLine(const QString &text, int startTime, int endTime, bool autoSplit, bool normalize) {
    QList<QVariantMap> result;
    std::vector<agi::ass::KaraokeSyllable> syls;
    try { syls = KaraokeParser::parse(text, startTime); }
    catch (const std::exception &) { return {}; }

    agi::ass::Karaoke kara;
    kara.SetLine(std::move(syls), autoSplit, normalize ? std::optional<int>(endTime) : std::nullopt);

    result.reserve(static_cast<qsizetype>(kara.size()));
    for (auto const& syl : kara) {
        QVariantMap map;
        map[QStringLiteral("startTime")] = syl.start_time;
        map[QStringLiteral("duration")] = syl.duration;
        map[QStringLiteral("endTime")] = syl.start_time + syl.duration;
        map[QStringLiteral("text")] = QString::fromStdString(syl.text);
        map[QStringLiteral("tagType")] = QString::fromStdString(syl.tag_type);
        map[QStringLiteral("textWithTags")] = QString::fromStdString(syl.GetText(false));
        result.append(map);
    }

    return result;
}

QList<int> AegisubCoreBridge::loadKeyframes(const QString &filePath) {
    QList<int> list;
    try {
        auto vec = agi::keyframe::Load(filePath.toStdString());
        list.reserve(static_cast<qsizetype>(vec.size()));
        for (int kf : vec) {
            list.append(kf);
        }
    } catch (...) {
        // Return empty list on failure
    }
    return list;
}

bool AegisubCoreBridge::saveKeyframes(const QString &filePath, const QList<int> &keyframes) {
    try {
        std::vector<int> vec(keyframes.begin(), keyframes.end());
        agi::keyframe::Save(filePath.toStdString(), vec);
        return true;
    } catch (...) {
        return false;
    }
}

QColor AegisubCoreBridge::parseColor(const QString &colorStr) {
    try {
        agi::Color col(colorStr.toStdString());
        // In ASS alpha, 0 is fully opaque and 255 is fully transparent.
        // In Qt QColor, 255 is fully opaque and 0 is fully transparent.
        int qAlpha = 255 - col.a;
        return QColor(col.r, col.g, col.b, qAlpha);
    } catch (...) {
        return QColor();
    }
}

QString AegisubCoreBridge::formatAssStyleColor(const QColor &color) {
    int assAlpha = 255 - color.alpha();
    agi::Color col(color.red(), color.green(), color.blue(), assAlpha);
    return QString::fromStdString(col.GetAssStyleFormatted());
}

QString AegisubCoreBridge::formatAssOverrideColor(const QColor &color) {
    agi::Color col(color.red(), color.green(), color.blue(), 0);
    return QString::fromStdString(col.GetAssOverrideFormatted());
}

QString AegisubCoreBridge::formatAssTime(int ms, bool msPrecision) {
    agi::Time t(ms);
    return QString::fromStdString(t.GetAssFormatted(msPrecision));
}

QString AegisubCoreBridge::formatSrtTime(int ms) {
    agi::Time t(ms);
    return QString::fromStdString(t.GetSrtFormatted());
}

int AegisubCoreBridge::parseAssTime(const QString &timeStr) {
    agi::Time t(timeStr.toStdString());
    return static_cast<int>(t);
}

QString AegisubCoreBridge::inlineStringEncode(const QString &input) {
    return QString::fromStdString(agi::ass::inline_string_encode(input.toStdString()));
}

QString AegisubCoreBridge::inlineStringDecode(const QString &input) {
    return QString::fromStdString(agi::ass::inline_string_decode(input.toStdString()));
}

static QString toLocalPath(const QString &path) {
    if (path.startsWith(QStringLiteral("file:"))) {
        const QUrl url(path);
        if (url.isLocalFile()) return url.toLocalFile();
    }
    return path;
}

QString AegisubCoreBridge::readTextFile(const QString &filePath) {
    const QString localPath = toLocalPath(filePath);
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QByteArray raw = file.readAll();
    return QString::fromUtf8(raw);
}

bool AegisubCoreBridge::writeTextFile(const QString &filePath, const QString &content) {
    const QString localPath = toLocalPath(filePath);
    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray utf8 = content.toUtf8();
    return file.write(utf8) == utf8.size();
}

void AegisubCoreBridge::launchNewInstance() {
#if defined(Q_OS_MACOS)
    const QString bundlePath = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../..");
    if (bundlePath.endsWith(QStringLiteral(".app"), Qt::CaseInsensitive)) {
        QProcess::startDetached(QStringLiteral("/usr/bin/open"), QStringList{QStringLiteral("-n"), QStringLiteral("-a"), bundlePath});
        return;
    }
#endif
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
}

bool AegisubCoreBridge::setClipboardText(const QString &text) {
    auto *clipboard = QGuiApplication::clipboard();
    clipboard->setText(text);
    return clipboard->text().replace(QStringLiteral("\r\n"), QStringLiteral("\n")) ==
           QString(text).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
}

QString AegisubCoreBridge::clipboardText() {
    return QGuiApplication::clipboard()->text();
}

bool AegisubCoreBridge::copyImageFileToClipboard(const QString &imagePath) {
    const QString localPath = toLocalPath(imagePath);
    const QImage image(localPath);
    if (image.isNull()) return false;
    QGuiApplication::clipboard()->setImage(image);
    return true;
}

void AegisubCoreBridge::setSetting(const QString &key, const QVariant &value) {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Aegisub"), QStringLiteral("Aegisub"));
    settings.setValue(key, value);
}

QVariant AegisubCoreBridge::getSetting(const QString &key, const QVariant &defaultValue) {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Aegisub"), QStringLiteral("Aegisub"));
    return settings.value(key, defaultValue);
}

QString AegisubCoreBridge::localFilePath(const QUrl &url) {
    return url.isLocalFile() ? url.toLocalFile() : QString();
}

QString AegisubCoreBridge::resolveUserPath(const QString &path) {
    if (path.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive))
        return localFilePath(QUrl(path));
    if (path.startsWith(QStringLiteral("?user"), Qt::CaseInsensitive)) {
        const QString userDir = AppPaths::dataDirectory();
        return QDir::cleanPath(userDir + path.mid(QStringLiteral("?user").size()));
    }
    return path;
}

QVariantList AegisubCoreBridge::listBackupFiles() {
    QVariantList result;

    struct SourceDir {
        QString settingsKey;
        QString fallback;
        QString label;
    };
    const SourceDir sources[] = {
        { QStringLiteral("Autosave/Path"), QStringLiteral("?user/autosave"), QStringLiteral("autosave") },
        { QStringLiteral("Backup/Path"), QStringLiteral("?user/autobackup"), QStringLiteral("autobackup") },
    };

    for (const auto &src : sources) {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Aegisub"), QStringLiteral("Aegisub"));
        QString dirPath = settings.value(src.settingsKey, src.fallback).toString();
        dirPath = resolveUserPath(dirPath);

        const QDir dir(dirPath);
        if (!dir.exists()) continue;

        const QFileInfoList files = dir.entryInfoList({ QStringLiteral("*.ass") }, QDir::Files, QDir::Time);
        for (const QFileInfo &fi : files) {
            QVariantMap entry;
            entry.insert(QStringLiteral("time"), fi.lastModified().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
            entry.insert(QStringLiteral("path"), fi.absoluteFilePath());
            entry.insert(QStringLiteral("kind"), src.label);
            result.append(entry);
        }
    }
    return result;
}

QString AegisubCoreBridge::extractSubtitlesFromVideo(const QString &videoPath)
{
    // Lazily purge extraction temp files older than a day; successful extractions
    // enter the MRU flow and are expected to be saved elsewhere by the user.
    const qint64 staleCutoff = QDateTime::currentDateTime().addDays(-1).toMSecsSinceEpoch();
    for (const QFileInfo &stale : QDir(QDir::temp()).entryInfoList(
             { QStringLiteral("aegisubqt_subs_*.ass") }, QDir::Files)) {
        if (stale.lastModified().toMSecsSinceEpoch() < staleCutoff)
            QFile::remove(stale.absoluteFilePath());
    }

    const QString localPath = toLocalPath(videoPath);
    if (localPath.isEmpty() || !QFileInfo::exists(localPath)) return QString();

    const QString stem = QFileInfo(localPath).completeBaseName();
    const QString outPath = QDir(QDir::temp()).filePath(
        QStringLiteral("aegisubqt_subs_%1_%2.ass").arg(stem).arg(QDateTime::currentMSecsSinceEpoch()));
    QFile::remove(outPath);

    const QString ffmpeg = MediaTools::ffmpegPath();
    if (ffmpeg.isEmpty()) {
        qWarning() << "Cannot extract container subtitles: ffmpeg is missing";
        return QString();
    }
    const int code = QProcess::execute(ffmpeg,
        { QStringLiteral("-y"), QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
          QStringLiteral("-i"), localPath, QStringLiteral("-map"), QStringLiteral("0:s:0"), outPath });
    if (code != 0 || !QFileInfo::exists(outPath) || QFileInfo(outPath).size() == 0) {
        QFile::remove(outPath);
        return QString();
    }
    return outPath;
}

