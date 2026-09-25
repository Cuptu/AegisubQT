// Copyright (c) 2026, Cuptu
// SPDX-License-Identifier: MIT

#include "LanguageManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <QVariantMap>
#include <QDebug>

QString FallbackTranslator::translate(const char *context, const char *sourceText,
                                      const char *disambiguation, int n) const {
    // 1. Attempt lookup with the specific component context
    QString result = QTranslator::translate(context, sourceText, disambiguation, n);
    if (!result.isEmpty()) {
        return result;
    }

    // 2. Fall back to empty/global context ("") if specific context missed
    if (context && *context) {
        result = QTranslator::translate("", sourceText, disambiguation, n);
        if (!result.isEmpty()) {
            return result;
        }
    }

    return QString();
}

LanguageManager::LanguageManager(QQmlApplicationEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
    initLocale();
}

LanguageManager::~LanguageManager() {
    if (m_installed) {
        QCoreApplication::removeTranslator(&m_translator);
        m_installed = false;
    }
}

QVariantList LanguageManager::availableLanguages() const {
    return {
        QVariantMap{{"code", "en_US"}, {"name", "English"},              {"nativeName", "English"}},
        QVariantMap{{"code", "zh_CN"}, {"name", "Simplified Chinese"},   {"nativeName", "简体中文"}},
        QVariantMap{{"code", "zh_TW"}, {"name", "Traditional Chinese"},  {"nativeName", "繁體中文"}},
        QVariantMap{{"code", "ja"},    {"name", "Japanese"},              {"nativeName", "日本語"}},
        QVariantMap{{"code", "ko"},    {"name", "Korean"},                {"nativeName", "한국어"}},
        QVariantMap{{"code", "fr_FR"}, {"name", "French"},                {"nativeName", "Français"}},
        QVariantMap{{"code", "de"},    {"name", "German"},                {"nativeName", "Deutsch"}},
        QVariantMap{{"code", "ru"},    {"name", "Russian"},               {"nativeName", "Русский"}},
        QVariantMap{{"code", "es"},    {"name", "Spanish"},               {"nativeName", "Español"}},
    };
}

QString LanguageManager::getDisplayName(const QString &langCode) const {
    const auto list = availableLanguages();
    for (const auto &item : list) {
        const auto map = item.toMap();
        if (map.value("code").toString() == langCode) {
            return map.value("nativeName").toString();
        }
    }
    return langCode;
}

QString LanguageManager::findQmPath(const QString &langCode) const {
    const QString fileName = QString("aegisub_%1.qm").arg(langCode);
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/locale/" + fileName,
        QCoreApplication::applicationDirPath() + "/../locale/" + fileName,
        QCoreApplication::applicationDirPath() + "/../share/AegisubQT/locale/" + fileName,
        QCoreApplication::applicationDirPath() + "/../Resources/locale/" + fileName,
        QDir::current().filePath("locale/" + fileName),
        ":/locale/" + fileName
    };

    for (const auto &path : candidates) {
        if (QFileInfo::exists(path)) {
            return QDir::cleanPath(path);
        }
    }
    return QString();
}

void LanguageManager::initLocale() {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Aegisub", "Aegisub");
    QString savedLang = settings.value("App/Language").toString();

    if (savedLang.isEmpty()) {
        const QString sysName = QLocale::system().name();
        if (sysName.startsWith("zh_CN", Qt::CaseInsensitive) || sysName.startsWith("zh_Hans", Qt::CaseInsensitive)) {
            savedLang = "zh_CN";
        } else if (sysName.startsWith("zh_TW", Qt::CaseInsensitive) || sysName.startsWith("zh_HK", Qt::CaseInsensitive) || sysName.startsWith("zh_Hant", Qt::CaseInsensitive)) {
            savedLang = "zh_TW";
        } else if (sysName.startsWith("ja", Qt::CaseInsensitive)) {
            savedLang = "ja";
        } else if (sysName.startsWith("ko", Qt::CaseInsensitive)) {
            savedLang = "ko";
        } else if (sysName.startsWith("fr", Qt::CaseInsensitive)) {
            savedLang = "fr_FR";
        } else if (sysName.startsWith("de", Qt::CaseInsensitive)) {
            savedLang = "de";
        } else if (sysName.startsWith("ru", Qt::CaseInsensitive)) {
            savedLang = "ru";
        } else if (sysName.startsWith("es", Qt::CaseInsensitive)) {
            savedLang = "es";
        } else {
            savedLang = "en_US";
        }
    }

    setLanguage(savedLang);
}

void LanguageManager::setLanguage(const QString &langCode) {
    if (m_installed) {
        QCoreApplication::removeTranslator(&m_translator);
        m_installed = false;
    }

    m_currentLanguage = langCode.trimmed();

    if (m_currentLanguage != "en_US" && m_currentLanguage != "en") {
        const QString qmPath = findQmPath(m_currentLanguage);
        if (!qmPath.isEmpty() && m_translator.load(qmPath)) {
            QCoreApplication::installTranslator(&m_translator);
            m_installed = true;
            qDebug() << "[LanguageManager] Successfully loaded translation:" << qmPath;
        } else {
            qWarning() << "[LanguageManager] Failed to load translation for" << m_currentLanguage
                       << "path:" << qmPath;
        }
    } else {
        qDebug() << "[LanguageManager] Switched to default English (no translator installed).";
    }

    // Persist selection
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Aegisub", "Aegisub");
    settings.setValue("App/Language", m_currentLanguage);

    // Retranslate all QML bindings live
    if (m_engine) {
        m_engine->retranslate();
    }

    emit languageChanged(m_currentLanguage);
}
