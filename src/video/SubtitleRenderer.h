// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT
#pragma once
#include <QByteArray>
#include <QImage>
#include <QString>
#include <memory>

// Uses the same ASS renderer as upstream, independent of the video decoder.
class SubtitleRenderer {
public:
    explicit SubtitleRenderer(const QString &libraryPath = {});
    ~SubtitleRenderer();
    bool available() const;
    QString error() const;
    bool setDocument(const QByteArray &ass);
    QImage render(int width, int height, qint64 milliseconds);
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
