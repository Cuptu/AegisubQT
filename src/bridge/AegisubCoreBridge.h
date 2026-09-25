// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Aegisub Project http://www.aegisub.org/

#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include <QList>
#include <QVariantMap>

namespace agi::ass {
    struct KaraokeSyllable;
}

/// Token representation exported to QML from native libaegisub dialogue tokenizer.
struct CoreDialogueToken {
    Q_GADGET
    Q_PROPERTY(int type MEMBER type)
    Q_PROPERTY(int length MEMBER length)
public:
    int type = 0;
    int length = 0;
};

/// Karaoke syllable representation exported to QML with timing and override tags.
struct CoreKaraokeSyllable {
    Q_GADGET
    Q_PROPERTY(int startTime MEMBER startTime)
    Q_PROPERTY(int duration MEMBER duration)
    Q_PROPERTY(QString text MEMBER text)
    Q_PROPERTY(QString tagType MEMBER tagType)
    Q_PROPERTY(QString textWithTags MEMBER textWithTags)
public:
    int startTime = 0;
    int duration = 0;
    QString text;
    QString tagType;
    QString textWithTags;
};

/// Bridge exposing native libaegisub algorithms (dialogue lexing, karaoke parsing,
/// time/color codecs, keyframe I/O) to the Qt/QML runtime.
class AegisubCoreBridge : public QObject {
    Q_OBJECT
public:
    explicit AegisubCoreBridge(QObject *parent = nullptr);

    /// Tokenize dialogue text into syntax tokens (text, override tags, arguments, drawings).
    Q_INVOKABLE static QList<QVariantMap> tokenizeLine(const QString &text, bool karaokeTemplater = false);

    /// Parse dialogue text into structured karaoke syllables with timing in milliseconds.
    Q_INVOKABLE static QList<QVariantMap> parseKaraokeLine(const QString &text, int startTime = 0, int endTime = 0, bool autoSplit = false);

    /// Load video keyframe indices from file.
    Q_INVOKABLE static QList<int> loadKeyframes(const QString &filePath);

    /// Save video keyframe indices to file.
    Q_INVOKABLE static bool saveKeyframes(const QString &filePath, const QList<int> &keyframes);

    /// Parse ASS hex color (&HAABBGGRR& / &HBBGGRR&) or CSS name into QColor.
    /// Note: ASS alpha (0=opaque, 255=transparent) is inverted to Qt alpha (255=opaque, 0=transparent).
    Q_INVOKABLE static QColor parseColor(const QString &colorStr);

    /// Format QColor as ASS style color string (&HAABBGGRR).
    Q_INVOKABLE static QString formatAssStyleColor(const QColor &color);

    /// Format QColor as ASS inline override color tag argument (&HBBGGRR&).
    Q_INVOKABLE static QString formatAssOverrideColor(const QColor &color);

    /// Format millisecond timestamp as ASS time string (H:MM:SS.CS).
    Q_INVOKABLE static QString formatAssTime(int ms, bool msPrecision = false);

    /// Format millisecond timestamp as SRT time string (HH:MM:SS,mmm).
    Q_INVOKABLE static QString formatSrtTime(int ms);

    /// Parse ASS time string (H:MM:SS.CS) into milliseconds.
    Q_INVOKABLE static int parseAssTime(const QString &timeStr);

    /// Inline string codec for escaping characters in configuration and project stores.
    Q_INVOKABLE static QString inlineStringEncode(const QString &input);
    Q_INVOKABLE static QString inlineStringDecode(const QString &input);

    /// Read UTF-8 text file content from local filesystem or file URI.
    Q_INVOKABLE static QString readTextFile(const QString &filePath);

    /// Write UTF-8 text content to local filesystem or file URI.
    Q_INVOKABLE static bool writeTextFile(const QString &filePath, const QString &content);
};

