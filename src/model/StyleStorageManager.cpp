// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

#include "StyleStorageManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <algorithm>

StyleStorageManager::StyleStorageManager(QObject *parent)
    : QObject(parent) {
    load();
    if (m_catalogs.isEmpty()) {
        populateDefaultCatalogs();
        save();
    }
}

StyleStorageManager::~StyleStorageManager() {
    save();
}

QString StyleStorageManager::storageFilePath() const {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty()) {
        configDir = QDir::current().filePath(QStringLiteral("config"));
    }
    QDir().mkpath(configDir);
    return QDir(configDir).filePath(QStringLiteral("style_catalogs.json"));
}

void StyleStorageManager::setCurrentCatalog(const QString &catalog) {
    if (m_currentCatalog != catalog && m_catalogs.contains(catalog)) {
        m_currentCatalog = catalog;
        emit currentCatalogChanged();
        emit currentCatalogStylesChanged();
    }
}

QVariantList StyleStorageManager::getCatalogStyles(const QString &catalogName) const {
    QString cat = catalogName.isEmpty() ? m_currentCatalog : catalogName;
    return m_catalogs.value(cat);
}

QVariantMap StyleStorageManager::getStyle(const QString &catalogName, int index) const {
    QString cat = catalogName.isEmpty() ? m_currentCatalog : catalogName;
    QVariantList list = m_catalogs.value(cat);
    if (index >= 0 && index < list.size()) {
        return list[index].toMap();
    }
    return QVariantMap();
}

void StyleStorageManager::createCatalog(const QString &catalogName) {
    if (catalogName.isEmpty() || m_catalogs.contains(catalogName)) return;

    QVariantList defaultList;
    QVariantMap st;
    st[QStringLiteral("name")] = QStringLiteral("Default");
    st[QStringLiteral("font")] = QStringLiteral("Microsoft YaHei");
    st[QStringLiteral("size")] = 48.0;
    st[QStringLiteral("primary")] = QStringLiteral("&H00FFFFFF");
    st[QStringLiteral("secondary")] = QStringLiteral("&H000000FF");
    st[QStringLiteral("outline")] = QStringLiteral("&H00000000");
    st[QStringLiteral("shadow")] = QStringLiteral("&H00000000");
    st[QStringLiteral("bold")] = false;
    st[QStringLiteral("italic")] = false;
    st[QStringLiteral("underline")] = false;
    st[QStringLiteral("strikeout")] = false;
    st[QStringLiteral("scaleX")] = 100.0;
    st[QStringLiteral("scaleY")] = 100.0;
    st[QStringLiteral("spacing")] = 0.0;
    st[QStringLiteral("angle")] = 0.0;
    st[QStringLiteral("borderStyle")] = 1;
    st[QStringLiteral("outlineWidth")] = 2.0;
    st[QStringLiteral("shadowDepth")] = 2.0;
    st[QStringLiteral("alignment")] = 2;
    st[QStringLiteral("marginL")] = 10;
    st[QStringLiteral("marginR")] = 10;
    st[QStringLiteral("marginV")] = 10;
    st[QStringLiteral("encoding")] = 1;
    defaultList.append(st);

    m_catalogs[catalogName] = defaultList;
    m_currentCatalog = catalogName;
    emit catalogsChanged();
    emit currentCatalogChanged();
    emit currentCatalogStylesChanged();
    save();
}

void StyleStorageManager::deleteCatalog(const QString &catalogName) {
    if (m_catalogs.size() <= 1 || !m_catalogs.contains(catalogName)) return;

    m_catalogs.remove(catalogName);
    if (m_currentCatalog == catalogName) {
        m_currentCatalog = m_catalogs.firstKey();
        emit currentCatalogChanged();
    }
    emit catalogsChanged();
    emit currentCatalogStylesChanged();
    save();
}

void StyleStorageManager::addStyle(const QString &catalogName, const QVariantMap &style) {
    QString cat = catalogName.isEmpty() ? m_currentCatalog : catalogName;
    if (!m_catalogs.contains(cat)) return;

    m_catalogs[cat].append(style);
    if (cat == m_currentCatalog) {
        emit currentCatalogStylesChanged();
    }
    save();
}

void StyleStorageManager::updateStyle(const QString &catalogName, int index, const QVariantMap &style) {
    QString cat = catalogName.isEmpty() ? m_currentCatalog : catalogName;
    if (!m_catalogs.contains(cat)) return;
    auto &list = m_catalogs[cat];
    if (index >= 0 && index < list.size()) {
        list[index] = style;
        if (cat == m_currentCatalog) {
            emit currentCatalogStylesChanged();
        }
        save();
    }
}

void StyleStorageManager::removeStyle(const QString &catalogName, int index) {
    QString cat = catalogName.isEmpty() ? m_currentCatalog : catalogName;
    if (!m_catalogs.contains(cat)) return;
    auto &list = m_catalogs[cat];
    if (index >= 0 && index < list.size() && list.size() > 1) {
        list.removeAt(index);
        if (cat == m_currentCatalog) {
            emit currentCatalogStylesChanged();
        }
        save();
    }
}

void StyleStorageManager::moveStyle(const QString &catalogName, int from, int to) {
    QString cat = catalogName.isEmpty() ? m_currentCatalog : catalogName;
    if (!m_catalogs.contains(cat)) return;
    auto &list = m_catalogs[cat];
    if (from >= 0 && from < list.size() && to >= 0 && to < list.size() && from != to) {
        list.move(from, to);
        if (cat == m_currentCatalog) {
            emit currentCatalogStylesChanged();
        }
        save();
    }
}

void StyleStorageManager::copyStyle(const QString &catalogName, int index) {
    QString cat = catalogName.isEmpty() ? m_currentCatalog : catalogName;
    if (!m_catalogs.contains(cat)) return;
    auto &list = m_catalogs[cat];
    if (index >= 0 && index < list.size()) {
        QVariantMap copy = list[index].toMap();
        copy[QStringLiteral("name")] = copy.value(QStringLiteral("name")).toString() + QStringLiteral(" (copy)");
        list.append(copy);
        if (cat == m_currentCatalog) {
            emit currentCatalogStylesChanged();
        }
        save();
    }
}

void StyleStorageManager::sortStyles(const QString &catalogName) {
    QString cat = catalogName.isEmpty() ? m_currentCatalog : catalogName;
    if (!m_catalogs.contains(cat)) return;
    auto &list = m_catalogs[cat];
    auto comp = [](const QVariant &a, const QVariant &b) {
        QString na = a.toMap().value(QStringLiteral("name")).toString();
        QString nb = b.toMap().value(QStringLiteral("name")).toString();
        return na.compare(nb, Qt::CaseInsensitive) < 0;
    };
    std::stable_sort(list.begin(), list.end(), comp);
    if (cat == m_currentCatalog) {
        emit currentCatalogStylesChanged();
    }
    save();
}

void StyleStorageManager::save() {
    QJsonObject root;
    QJsonObject catObj;

    for (auto it = m_catalogs.begin(); it != m_catalogs.end(); ++it) {
        QJsonArray arr;
        for (const auto &v : it.value()) {
            arr.append(QJsonObject::fromVariantMap(v.toMap()));
        }
        catObj[it.key()] = arr;
    }
    root[QStringLiteral("catalogs")] = catObj;
    root[QStringLiteral("currentCatalog")] = m_currentCatalog;

    QSaveFile file(storageFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.commit();
    }
}

void StyleStorageManager::load() {
    QFile file(storageFilePath());
    if (!file.open(QIODevice::ReadOnly)) return;

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;

    QJsonObject root = doc.object();
    QJsonObject catObj = root.value(QStringLiteral("catalogs")).toObject();

    m_catalogs.clear();
    for (auto it = catObj.begin(); it != catObj.end(); ++it) {
        QJsonArray arr = it.value().toArray();
        QVariantList list;
        for (const auto &item : arr) {
            list.append(item.toObject().toVariantMap());
        }
        m_catalogs[it.key()] = list;
    }

    if (root.contains(QStringLiteral("currentCatalog"))) {
        QString cur = root.value(QStringLiteral("currentCatalog")).toString();
        if (m_catalogs.contains(cur)) {
            m_currentCatalog = cur;
        }
    }
    if (m_currentCatalog.isEmpty() && !m_catalogs.isEmpty()) {
        m_currentCatalog = m_catalogs.firstKey();
    }

    emit catalogsChanged();
    emit currentCatalogChanged();
    emit currentCatalogStylesChanged();
}

void StyleStorageManager::populateDefaultCatalogs() {
    auto makeStyle = [](const QString &name, const QString &font, double size, const QString &prim, const QString &outl) {
        QVariantMap s;
        s[QStringLiteral("name")] = name;
        s[QStringLiteral("font")] = font;
        s[QStringLiteral("size")] = size;
        s[QStringLiteral("primary")] = prim;
        s[QStringLiteral("secondary")] = QStringLiteral("&H000000FF");
        s[QStringLiteral("outline")] = outl;
        s[QStringLiteral("shadow")] = QStringLiteral("&H00000000");
        s[QStringLiteral("bold")] = false;
        s[QStringLiteral("italic")] = false;
        s[QStringLiteral("underline")] = false;
        s[QStringLiteral("strikeout")] = false;
        s[QStringLiteral("scaleX")] = 100.0;
        s[QStringLiteral("scaleY")] = 100.0;
        s[QStringLiteral("spacing")] = 0.0;
        s[QStringLiteral("angle")] = 0.0;
        s[QStringLiteral("borderStyle")] = 1;
        s[QStringLiteral("outlineWidth")] = 2.0;
        s[QStringLiteral("shadowDepth")] = 2.0;
        s[QStringLiteral("alignment")] = 2;
        s[QStringLiteral("marginL")] = 10;
        s[QStringLiteral("marginR")] = 10;
        s[QStringLiteral("marginV")] = 10;
        s[QStringLiteral("encoding")] = 1;
        return s;
    };

    QVariantList defaultCat;
    defaultCat.append(makeStyle(QStringLiteral("Default"), QStringLiteral("Microsoft YaHei"), 48.0, QStringLiteral("&H00FFFFFF"), QStringLiteral("&H00000000")));
    defaultCat.append(makeStyle(QStringLiteral("Alt"), QStringLiteral("Segoe UI"), 42.0, QStringLiteral("&H00FFFDEA"), QStringLiteral("&H00333333")));
    defaultCat.append(makeStyle(QStringLiteral("Sign"), QStringLiteral("Impact"), 54.0, QStringLiteral("&H00FFCC00"), QStringLiteral("&H00000000")));
    defaultCat.append(makeStyle(QStringLiteral("ED_Roman"), QStringLiteral("Times New Roman"), 36.0, QStringLiteral("&H00E8E8E8"), QStringLiteral("&H00111111")));

    QVariantList animeCat;
    animeCat.append(makeStyle(QStringLiteral("Default"), QStringLiteral("Microsoft YaHei"), 48.0, QStringLiteral("&H00FFFFFF"), QStringLiteral("&H00000000")));
    animeCat.append(makeStyle(QStringLiteral("Title"), QStringLiteral("Microsoft YaHei"), 64.0, QStringLiteral("&H00FFE066"), QStringLiteral("&H001A1A1A")));
    animeCat.append(makeStyle(QStringLiteral("OP"), QStringLiteral("Microsoft YaHei"), 40.0, QStringLiteral("&H00FFAAA5"), QStringLiteral("&H002D4059")));
    animeCat.append(makeStyle(QStringLiteral("ED"), QStringLiteral("Microsoft YaHei"), 40.0, QStringLiteral("&H00FFD3B6"), QStringLiteral("&H002D4059")));

    m_catalogs[QStringLiteral("Default")] = defaultCat;
    m_catalogs[QStringLiteral("Anime Template")] = animeCat;
    m_currentCatalog = QStringLiteral("Default");
}
