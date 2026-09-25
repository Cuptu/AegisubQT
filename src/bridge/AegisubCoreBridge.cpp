// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// Aegisub Project http://www.aegisub.org/

#include "AegisubCoreBridge.h"

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
#include <algorithm>

namespace {

using namespace agi::ass;
namespace dt = DialogueTokenType;

std::vector<KaraokeSyllable> parse_karaoke_syllables_from_tokens(std::string_view text, int line_start) {
    auto tokens = TokenizeDialogueBody(text, false);
    std::vector<KaraokeSyllable> syls;

    KaraokeSyllable syl;
    syl.start_time = line_start;
    syl.duration = 0;
    syl.tag_type = "\\k";

    size_t pos = 0;
    size_t i = 0;
    const size_t n = tokens.size();

    while (i < n) {
        auto const& tok = tokens[i];
        size_t len = tok.length;
        std::string_view tok_str = text.substr(pos, len);

        switch (tok.type) {
            case dt::TEXT:
            case dt::WORD:
            case dt::LINE_BREAK:
                syl.text += tok_str;
                break;
            case dt::COMMENT:
            case dt::DRAWING_FULL:
            case dt::DRAWING_CMD:
            case dt::DRAWING_X:
            case dt::DRAWING_Y:
            case dt::DRAWING_ENDPOINT_X:
            case dt::DRAWING_ENDPOINT_Y:
                syl.ovr_tags[syl.text.size()] += tok_str;
                break;
            case dt::OVR_BEGIN: {
                // Parse override block until OVR_END
                size_t j = i + 1;
                size_t block_pos = pos + len;
                bool in_tag_group = false;

                while (j < n && tokens[j].type != dt::OVR_END) {
                    auto const& sub_tok = tokens[j];
                    size_t sub_len = sub_tok.length;
                    std::string_view sub_str = text.substr(block_pos, sub_len);

                    if (sub_tok.type == dt::TAG_NAME && (sub_str.starts_with("k") || sub_str.starts_with("K"))) {
                        // Found a karaoke tag
                        if (in_tag_group) {
                            syl.ovr_tags[syl.text.size()] += "}";
                            in_tag_group = false;
                        }

                        std::string k_tag_name = std::string(sub_str);
                        if (k_tag_name == "K") k_tag_name = "kf";

                        // Collect argument and convert centiseconds to milliseconds
                        int dur = 0;
                        if (j + 1 < n && tokens[j + 1].type == dt::ARG) {
                            ++j;
                            size_t arg_len = tokens[j].length;
                            std::string_view arg_str = text.substr(block_pos + sub_len, arg_len);
                            block_pos += sub_len + arg_len;
                            dur = std::atoi(std::string(arg_str).c_str()) * 10;
                        } else {
                            block_pos += sub_len;
                        }

                        if (syl.duration > 0 || !syl.text.empty()) {
                            syls.push_back(syl);
                            syl.text.clear();
                            syl.ovr_tags.clear();
                        }

                        syl.tag_type = "\\" + k_tag_name;
                        syl.start_time += syl.duration;
                        syl.duration = dur;
                    } else {
                        // Regular tag or punctuation
                        if (!in_tag_group) {
                            syl.ovr_tags[syl.text.size()] += "{";
                            in_tag_group = true;
                        }
                        syl.ovr_tags[syl.text.size()] += sub_str;
                        block_pos += sub_len;
                    }
                    ++j;
                }

                if (in_tag_group) {
                    syl.ovr_tags[syl.text.size()] += "}";
                }

                // Advance main loop past OVR_END
                while (i < j && i < n) {
                    pos += tokens[i].length;
                    ++i;
                }
                if (i < n && tokens[i].type == dt::OVR_END) {
                    pos += tokens[i].length;
                    ++i;
                }
                continue;
            }
            default:
                break;
        }

        pos += len;
        ++i;
    }

    syls.push_back(syl);
    return syls;
}

} // anonymous namespace

AegisubCoreBridge::AegisubCoreBridge(QObject *parent) : QObject(parent) {}

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

QList<QVariantMap> AegisubCoreBridge::parseKaraokeLine(const QString &text, int startTime, int endTime, bool autoSplit) {
    QList<QVariantMap> result;
    QByteArray utf8 = text.toUtf8();
    std::string_view sv(utf8.constData(), utf8.size());

    auto syls = parse_karaoke_syllables_from_tokens(sv, startTime);

    agi::ass::Karaoke kara;
    kara.SetLine(std::move(syls), autoSplit, endTime > startTime ? std::optional<int>(endTime) : std::nullopt);

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

QString AegisubCoreBridge::readTextFile(const QString &filePath) {
    QString localPath = filePath;
    if (localPath.startsWith("file:///")) {
        localPath = QUrl(filePath).toLocalFile();
    } else if (localPath.startsWith("file://")) {
        localPath = localPath.mid(7);
    }
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    return in.readAll();
}

bool AegisubCoreBridge::writeTextFile(const QString &filePath, const QString &content) {
    QString localPath = filePath;
    if (localPath.startsWith("file:///")) {
        localPath = QUrl(filePath).toLocalFile();
    } else if (localPath.startsWith("file://")) {
        localPath = localPath.mid(7);
    }
    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    return true;
}

