// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

#include "RecentFilesManager.h"

#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QUrl>

RecentFilesManager::RecentFilesManager(QObject *parent)
    : QObject(parent)
{
}

QString RecentFilesManager::groupFor(const QString &type) const
{
    return QStringLiteral("recent/") + type;
}

QStringList RecentFilesManager::entries(const QString &type) const
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Aegisub"), QStringLiteral("Aegisub"));
    return settings.value(groupFor(type)).toStringList();
}

void RecentFilesManager::add(const QString &type, const QString &path)
{
    if (type.isEmpty() || path.isEmpty()) return;

    QString clean = path;
    if (clean.startsWith(QStringLiteral("file:"))) {
        const QUrl url(clean);
        if (url.isLocalFile()) clean = url.toLocalFile();
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Aegisub"), QStringLiteral("Aegisub"));
    QStringList list = settings.value(groupFor(type)).toStringList();

    // Normalize separators and deduplicate, newest first.
    const QString normalized = QDir::cleanPath(clean);
    list.removeAll(normalized);
    list.prepend(normalized);
    while (list.size() > kMaxEntries) list.removeLast();

    settings.setValue(groupFor(type), list);
    emit entriesChanged();
}

void RecentFilesManager::clear(const QString &type)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Aegisub"), QStringLiteral("Aegisub"));
    settings.remove(groupFor(type));
    emit entriesChanged();
}

bool RecentFilesManager::exists(const QString &path) const
{
    QString clean = path;
    if (clean.startsWith(QStringLiteral("file:"))) {
        const QUrl url(clean);
        if (url.isLocalFile()) clean = url.toLocalFile();
    }
    return QFileInfo::exists(clean);
}
