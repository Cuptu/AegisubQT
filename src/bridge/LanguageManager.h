// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>
#include <QVariantList>
#include <QQmlApplicationEngine>

/// Custom translator that falls back to the empty global context ("") when
/// a QML component-specific context lookup yields no translation.
/// This matches GNU gettext PO dictionary imports seamlessly with Qt Quick.
class FallbackTranslator : public QTranslator {
    Q_OBJECT
public:
    using QTranslator::QTranslator;

    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override;
};

/// Language manager providing dynamic runtime internationalization (i18n)
/// and live, non-destructive QML retranslation via QQmlApplicationEngine::retranslate().
class LanguageManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentLanguage READ currentLanguage WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages NOTIFY availableLanguagesChanged)

public:
    explicit LanguageManager(QQmlApplicationEngine *engine, QObject *parent = nullptr);
    ~LanguageManager() override;

    QString currentLanguage() const { return m_currentLanguage; }
    QVariantList availableLanguages() const;

    Q_INVOKABLE void setLanguage(const QString &langCode);
    Q_INVOKABLE QString getDisplayName(const QString &langCode) const;

signals:
    void languageChanged(const QString &langCode);
    void availableLanguagesChanged();

private:
    void initLocale();
    QString findQmPath(const QString &langCode) const;

    QQmlApplicationEngine *m_engine = nullptr;
    FallbackTranslator m_translator;
    QString m_currentLanguage = "en_US";
    bool m_installed = false;
};
