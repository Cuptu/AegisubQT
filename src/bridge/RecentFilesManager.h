// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// RecentFilesManager: persistent most-recently-used file lists per media type.
// Mirrors upstream Aegisub's MRU subsystem (MRUStub / recent/<type> entries),
// backed by QSettings so entries survive restarts on every platform.

#pragma once

#include <QObject>
#include <QStringList>

class RecentFilesManager : public QObject
{
    Q_OBJECT
public:
    explicit RecentFilesManager(QObject *parent = nullptr);

    // Returns the MRU list for a type ("subtitles"|"video"|"audio"|"keyframes"|"timecodes"),
    // most recent first. Missing files are kept and flagged by exists() in QML.
    Q_INVOKABLE QStringList entries(const QString &type) const;

    // Pushes a path to the top of the given type's list (deduplicated, capped at 10).
    Q_INVOKABLE void add(const QString &type, const QString &path);

    // Clears the MRU list for a type.
    Q_INVOKABLE void clear(const QString &type);

    // True when the file still exists on disk (used to dim stale entries in menus).
    Q_INVOKABLE bool exists(const QString &path) const;

signals:
    void entriesChanged();

private:
    static constexpr int kMaxEntries = 10;
    QString groupFor(const QString &type) const;
};
