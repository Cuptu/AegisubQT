// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT
#pragma once

#include <QByteArray>
#include <QMap>
#include <QVariantList>
#include <QVariantMap>

// Automation extradata is a map of Lua byte strings, including binary values.
using AssExtraData = QMap<QByteArray, QByteArray>;

inline QVariantList assExtraToVariant(const AssExtraData &extra) {
    QVariantList result;
    for (auto it = extra.cbegin(); it != extra.cend(); ++it)
        result.append(QVariantMap{{"key", it.key()}, {"value", it.value()}});
    return result;
}

inline AssExtraData assExtraFromVariant(const QVariant &value) {
    AssExtraData result;
    if (value.metaType().id() == QMetaType::QVariantMap) {
        const auto map = value.toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            result[it.key().toUtf8()] = it.value().toByteArray();
    } else {
        for (const auto &entry : value.toList()) {
            const auto map = entry.toMap();
            result[map.value("key").toByteArray()] = map.value("value").toByteArray();
        }
    }
    return result;
}

// Use the upstream #HH escaping syntax, also escaping high bytes so binary
// keys/values survive Unicode documents and non-UTF-8 file encodings.
inline QString assExtraEncode(const QByteArray &bytes) {
    QString result;
    for (unsigned char ch : bytes) {
        if (ch <= 0x20 || ch >= 0x7f || ch == '#' || ch == ',' || ch == ':' || ch == '|')
            result += QStringLiteral("#%1").arg(ch, 2, 16, QLatin1Char('0')).toUpper();
        else result += QLatin1Char(ch);
    }
    return result;
}
