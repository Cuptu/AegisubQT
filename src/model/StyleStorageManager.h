// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

// StyleStorageManager: Persistent storage manager for subtitle style catalogs.
// Aligns with Aegisub's ass_style_storage subsystem, providing disk persistence,
// catalog organization, and cross-catalog synchronization.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>

class StyleStorageManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList catalogNames READ catalogNames NOTIFY catalogsChanged)
    Q_PROPERTY(QString currentCatalog READ currentCatalog WRITE setCurrentCatalog NOTIFY currentCatalogChanged)
    Q_PROPERTY(QVariantList currentCatalogStyles READ currentCatalogStyles NOTIFY currentCatalogStylesChanged)

public:
    explicit StyleStorageManager(QObject *parent = nullptr);
    ~StyleStorageManager() override;

    QStringList catalogNames() const { return m_catalogs.keys(); }
    QString currentCatalog() const { return m_currentCatalog; }
    void setCurrentCatalog(const QString &catalog);

    QVariantList currentCatalogStyles() const { return getCatalogStyles(m_currentCatalog); }

    Q_INVOKABLE QVariantList getCatalogStyles(const QString &catalogName) const;
    Q_INVOKABLE QVariantMap getStyle(const QString &catalogName, int index) const;

    Q_INVOKABLE void createCatalog(const QString &catalogName);
    Q_INVOKABLE void deleteCatalog(const QString &catalogName);

    Q_INVOKABLE void addStyle(const QString &catalogName, const QVariantMap &style);
    Q_INVOKABLE void updateStyle(const QString &catalogName, int index, const QVariantMap &style);
    Q_INVOKABLE void removeStyle(const QString &catalogName, int index);
    Q_INVOKABLE void moveStyle(const QString &catalogName, int from, int to);
    Q_INVOKABLE void copyStyle(const QString &catalogName, int index);
    Q_INVOKABLE void sortStyles(const QString &catalogName);

    Q_INVOKABLE void save();
    Q_INVOKABLE void load();

signals:
    void catalogsChanged();
    void currentCatalogChanged();
    void currentCatalogStylesChanged();

private:
    QString storageFilePath() const;
    void populateDefaultCatalogs();

    QMap<QString, QVariantList> m_catalogs;
    QString m_currentCatalog = QStringLiteral("Default");
};
