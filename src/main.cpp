// Copyright (c) 2005 - 2026, Aegisub Project & Contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

#include <QGuiApplication>
#include <QFileOpenEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QFileInfo>
#include <QSettings>
#include <QDir>
#include <QUrl>
#include <QDebug>
#include "VideoController.h"
#include "VideoFrameImageProvider.h"
#include "VideoSurfaceItem.h"
#include "AudioController.h"
#include "AudioDisplayController.h"
#include "VideoDisplayController.h"
#include "AstraCoreBridge.h"
#include "SpectrogramItem.h"
#include "AutomationManager.h"
#include "AegisubCoreBridge.h"
#include "RecentFilesManager.h"
#include "LanguageManager.h"
#include "model/SubtitleModel.h"
#include "model/StyleStorageManager.h"

#include <QWindow>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QPalette>
#include <QKeyEvent>
#include <QLibraryInfo>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QStringListModel>
#include <climits>

// Thread-safe in-memory log ring buffer powering the in-app Log Window
// (upstream Help > Log Window). Qt log messages are mirrored here in addition
// to stderr so users can inspect runtime diagnostics without a console.
class AppLogBuffer : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList lines READ lines NOTIFY linesChanged)
public:
    explicit AppLogBuffer(QObject *parent = nullptr) : QObject(parent) {}

    void append(QtMsgType type, const QString &msg) {
        static constexpr int kMaxLines = 1000;
        const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
        const QString prefix = (type == QtWarningMsg) ? QStringLiteral("W")
                               : (type == QtCriticalMsg) ? QStringLiteral("C")
                               : (type == QtFatalMsg) ? QStringLiteral("F")
                               : QStringLiteral("I");
        const QString line = QStringLiteral("[%1][%2] %3").arg(stamp, prefix, msg);
        {
            QMutexLocker lock(&m_mutex);
            m_lines.append(line);
            while (m_lines.size() > kMaxLines) m_lines.removeFirst();
            const QStringList snapshot = m_lines;
            QMetaObject::invokeMethod(this, [this, snapshot]() {
                m_model.setStringList(snapshot);
                Q_EMIT linesChanged();
            }, Qt::QueuedConnection);
        }
    }

    QStringList lines() const {
        QMutexLocker lock(&m_mutex);
        return m_lines;
    }

    Q_INVOKABLE void clear() {
        {
            QMutexLocker lock(&m_mutex);
            m_lines.clear();
        }
        QMetaObject::invokeMethod(this, [this]() {
            m_model.setStringList({});
            Q_EMIT linesChanged();
        }, Qt::QueuedConnection);
    }

    QAbstractListModel *model() { return &m_model; }

signals:
    void linesChanged();

private:
    mutable QMutex m_mutex;
    QStringList m_lines;
    QStringListModel m_model;
};

static AppLogBuffer *g_appLog = nullptr;

static void customLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(type);
    Q_UNUSED(context);
    std::fprintf(stderr, "[QT_LOG] %s\n", msg.toLocal8Bit().constData());
    std::fflush(stderr);
    if (g_appLog) {
        g_appLog->append(type, msg);
    }
    if (qEnvironmentVariableIsSet("AEGISUB_DEBUG_LOG")) {
        QFile file("debug_log.txt");
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << "[QT_LOG] " << msg << "\n";
            out.flush();
            file.flush();
        }
    }
}

// Cross-platform UI typography theme exposed to QML via context property 'uiTheme'.
// Replaces hardcoded font families that cause inconsistent fallback across desktop OSs.
// Defaults are aligned with platform UI metrics in main().
class UiTheme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString uiFont READ uiFont CONSTANT)
    Q_PROPERTY(QString monoFont READ monoFont CONSTANT)
public:
    explicit UiTheme(QObject *parent = nullptr) : QObject(parent) {}
    // Application default UI font family configured for Windows/macOS/Linux.
    QString uiFont() const { return QGuiApplication::font().family(); }
    // Platform-native monospace font family resolved via QFontDatabase.
    QString monoFont() const { return QFontDatabase::systemFont(QFontDatabase::FixedFont).family(); }
};

// Medusa mode (upstream "Toggle global hotkey overrides (Medusa Mode)"):
// Audio playback hotkeys take precedence application-wide. The filter intercepts
// key presses before focused controls, consuming audio navigation shortcuts.
class MedusaKeyFilter : public QObject {
public:
    MedusaKeyFilter(AudioController *audio, AudioDisplayController *display, QObject *parent = nullptr)
        : QObject(parent), m_audio(audio), m_display(display) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::KeyPress && m_audio && m_display && m_audio->medusaMode()) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (m_display->keyPressed(keyEvent->key(), static_cast<int>(keyEvent->modifiers()))) {
                return true; // Audio hotkey consumed with global priority
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    AudioController *m_audio = nullptr;
    AudioDisplayController *m_display = nullptr;
};

namespace {
// Recursively copy every key/group from one QSettings store into another,
// preserving QVariant types (QStringList MRU entries survive round-trips).
void copySettingsGroup(QSettings &src, QSettings &dst, const QString &prefix)
{
    for (const QString &key : src.childKeys())
        dst.setValue(prefix + key, src.value(key));
    for (const QString &group : src.childGroups()) {
        src.beginGroup(group);
        copySettingsGroup(src, dst, prefix + group + QLatin1Char('/'));
        src.endGroup();
    }
}
} // namespace

// One-time migration from the legacy NativeFormat store (registry on Windows,
// a .conf file on Linux) to the unified IniFormat store, so existing users
// keep their preferences and MRU lists across the upgrade. The marker lives
// in the new store; portable mode starts from a clean slate and never migrates.
static void migrateLegacySettings()
{
    QSettings ini(QSettings::IniFormat, QSettings::UserScope,
                  QStringLiteral("Aegisub"), QStringLiteral("Aegisub"));
    if (ini.value(QStringLiteral("SettingsMigrated"), false).toBool())
        return;

    QSettings legacy(QSettings::NativeFormat, QSettings::UserScope,
                     QStringLiteral("Aegisub"), QStringLiteral("Aegisub"));
    copySettingsGroup(legacy, ini, QString());
    ini.setValue(QStringLiteral("SettingsMigrated"), true);
}

class AegisubApplication : public QGuiApplication {
public:
    AegisubApplication(int &argc, char **argv) : QGuiApplication(argc, argv) {}

    void setSubtitleModel(SubtitleModel *model) {
        m_model = model;
        if (m_model && !m_pendingFile.isEmpty()) {
            m_model->loadFromFile(m_pendingFile);
            m_pendingFile.clear();
        }
    }

protected:
    bool event(QEvent *event) override {
        if (event->type() == QEvent::FileOpen) {
            auto *fe = static_cast<QFileOpenEvent*>(event);
            if (m_model) {
                m_model->loadFromFile(fe->file());
            } else {
                m_pendingFile = fe->file();
            }
            return true;
        }
        return QGuiApplication::event(event);
    }

private:
    SubtitleModel *m_model = nullptr;
    QString m_pendingFile;
};

int main(int argc, char *argv[])
{
#if defined(Q_OS_MACOS)
    // GUI applications launched from Finder/Dock inherit a minimal PATH that excludes
    // Homebrew or MacPorts prefixes. Prepend them so tools like ffmpeg are discovered.
    QByteArray pathEnv = qgetenv("PATH");
    QStringList paths = QString::fromLocal8Bit(pathEnv).split(':', Qt::SkipEmptyParts);
    const QStringList extraPaths = {
        QStringLiteral("/opt/homebrew/bin"),
        QStringLiteral("/usr/local/bin"),
        QStringLiteral("/usr/bin"),
        QStringLiteral("/bin")
    };
    bool pathChanged = false;
    for (const QString &p : extraPaths) {
        if (!paths.contains(p) && QDir(p).exists()) {
            paths.prepend(p);
            pathChanged = true;
        }
    }
    if (pathChanged) {
        qputenv("PATH", paths.join(':').toLocal8Bit());
    }
#endif

    if (qEnvironmentVariableIsSet("AEGISUB_DEBUG_LOG")) {
        QFile file("debug_log.txt");
        (void)file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    }
    AppLogBuffer appLog;
    g_appLog = &appLog;
    qInstallMessageHandler(customLogHandler);
    QQuickStyle::setStyle("Aegisub");
    QQuickStyle::setFallbackStyle("Fusion");
#if !defined(Q_OS_MACOS)
    // Prevent non-macOS platforms from attempting to delegate in-scene QML MenuBar to native menu bars
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
#endif
    AegisubApplication app(argc, argv);
#if defined(Q_OS_MACOS)
    QCoreApplication::addLibraryPath(app.applicationDirPath() + "/../PlugIns");
#endif
    QCoreApplication::addLibraryPath(app.applicationDirPath() + "/plugins");
    QCoreApplication::addLibraryPath(app.applicationDirPath());
    app.setApplicationName("AegisubQT");
    app.setApplicationVersion("4.0.2");
    app.setOrganizationName("AegisubQT");

    // Portable mode: a "portable.txt" marker next to the executable keeps all
    // settings in an ini file beside the app instead of the platform store.
    // All QSettings call sites use the explicit IniFormat, which honours the
    // path set here (matching upstream Aegisub's file-based config).
    if (QFileInfo::exists(app.applicationDirPath() + "/portable.txt")) {
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, app.applicationDirPath());
    } else {
        migrateLegacySettings();
    }

    // Align default system font metrics with platform conventions.
    QFont defaultFont;
#if defined(Q_OS_WIN)
    defaultFont.setFamilies({"Segoe UI", "Microsoft YaHei UI", "Yu Gothic UI", "Malgun Gothic", "Tahoma"});
    defaultFont.setPointSize(9);
#elif defined(Q_OS_MACOS)
    defaultFont.setFamilies({".AppleSystemUIFont", "PingFang SC", "Hiragino Sans", "Apple SD Gothic Neo", "Helvetica Neue"});
    defaultFont.setPointSize(12);
#else
    defaultFont.setFamilies({"Cantarell", "Ubuntu", "Noto Sans", "sans-serif"});
    defaultFont.setPointSize(10);
#endif
    QGuiApplication::setFont(defaultFont);

    // Set Windows 10 tooltip palette defaults
    QPalette appPal = app.palette();
    appPal.setColor(QPalette::ToolTipBase, Qt::white);
    appPal.setColor(QPalette::ToolTipText, QColor("#575757"));
    app.setPalette(appPal);

    // Set official application window and taskbar icon
    QIcon appIcon = QIcon::fromTheme(QStringLiteral("AegisubQT"));
    if (appIcon.isNull()) {
        const QStringList iconCandidates = {
            app.applicationDirPath() + "/assets/icons_native/icon.ico",
            app.applicationDirPath() + "/assets/branding/icon_64.png",
            app.applicationDirPath() + "/assets/icons_native/icon.icns",
            app.applicationDirPath() + "/../Resources/assets/branding/icon_64.png",
            app.applicationDirPath() + "/../Resources/icon.icns",
            app.applicationDirPath() + "/../share/AegisubQT/assets/branding/icon_64.png",
            app.applicationDirPath() + "/../assets/icons_native/icon.ico",
            QDir::current().filePath("assets/branding/icon_64.png"),
            QDir::current().filePath("assets/icons_native/icon.ico")
        };
        for (const QString &p : iconCandidates) {
            if (QFileInfo::exists(p)) {
                appIcon.addFile(p);
                break;
            }
        }
    }
    if (!appIcon.isNull()) {
        QGuiApplication::setWindowIcon(appIcon);
    }

    qmlRegisterType<SpectrogramItem>("Aegisub", 1, 0, "SpectrogramView");
    qmlRegisterType<VideoSurfaceItem>("Aegisub", 1, 0, "VideoSurface");
    qmlRegisterType<AegisubCoreBridge>("Aegisub", 1, 0, "AegisubCoreBridge");

    // Command-line options for media playback and rendering configuration.
    QString assPath;
    bool karaokeMode = false;
    QString audioPath;
    QString videoPath;
    QString initialLang;
    int zoomLevel = INT_MIN;
    int scrollPx = -1;
    int selStart = -1;
    int selEnd = -1;
    int freqCurve = -1;
    int sampleRate = 0;
    bool waveformMode = true;

    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString &a = args[i];
        auto value = [&args, &i](const char *opt) -> QString {
            if (i + 1 < args.size()) return args[++i];
            qWarning() << "Missing value for" << opt;
            return QString();
        };
        if (a == "--waveform") {
            waveformMode = false;
        } else if (a == "--ass") {
            assPath = value("--ass");
        } else if (a == "--karaoke") {
            karaokeMode = true;
        } else if (a == "--audio") {
            audioPath = value("--audio");
        } else if (a == "--video") {
            videoPath = value("--video");
        } else if (a == "--zoom") {
            zoomLevel = value("--zoom").toInt();
        } else if (a == "--scroll") {
            scrollPx = value("--scroll").toInt();
        } else if (a == "--freq-curve") {
            freqCurve = value("--freq-curve").toInt();
        } else if (a == "--sample-rate") {
            sampleRate = value("--sample-rate").toInt();
        } else if (a == "--sel") {
            const QStringList parts = value("--sel").split(',');
            if (parts.size() == 2) {
                selStart = parts[0].toInt();
                selEnd = parts[1].toInt();
            }
        } else if (a == "--lang" || a == "--language") {
            initialLang = value(a.toUtf8().constData());
        } else if (!a.startsWith("--")) {
            // Bare path argument: open as subtitle file (shell file association).
            if (assPath.isEmpty()) {
                assPath = a;
                if (assPath.startsWith(QStringLiteral("file:"))) {
                    const QUrl url(assPath);
                    if (url.isLocalFile()) assPath = url.toLocalFile();
                }
            }
        }
    }

    VideoController videoController;
    AudioController audioController(&videoController);
    // AudioDisplayController manages hit testing and drag state transitions.
    // Must outlive QQmlApplicationEngine to ensure teardown order safety.
    AudioDisplayController displayController(&audioController);
    VideoDisplayController videoDisplayController(&videoController);

    if (!videoPath.isEmpty()) {
        videoController.openVideoFile(videoPath);
        if (audioPath.isEmpty()) {
            audioPath = videoPath;
        }
    }

    if (!audioPath.isEmpty()) {
        audioController.loadAudio(audioPath, sampleRate);
    }

    // Default spectrum frequency curve mapping (Audio/Renderer/Spectrum/FreqCurve = 0).
    audioController.applyFreqCurve(freqCurve >= 0 ? freqCurve : 0);
    if (zoomLevel != INT_MIN) audioController.setZoomLevel(zoomLevel);
    if (selStart >= 0 && selEnd > selStart) audioController.setSelection(selStart, selEnd);
    if (scrollPx >= 0) audioController.scrollTo(scrollPx);

    if (!waveformMode) {
        audioController.setSpectrumMode(false);
    }

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("videoframe"), new VideoFrameImageProvider());
    engine.rootContext()->setContextProperty("videoController", &videoController);
    engine.rootContext()->setContextProperty("videoDisplayController", &videoDisplayController);
    engine.rootContext()->setContextProperty("audioController", &audioController);
    engine.rootContext()->setContextProperty("displayController", &displayController);
    engine.rootContext()->setContextProperty("astracore", AstraCoreBridge::instance());
    AegisubCoreBridge aegisubCoreBridge;
    engine.rootContext()->setContextProperty("aegisubCore", &aegisubCoreBridge);
    engine.rootContext()->setContextProperty("automationManager", Automation::AutomationManager::instance());
    // Unified platform font theme for all QML views
    UiTheme uiTheme;
    engine.rootContext()->setContextProperty("uiTheme", &uiTheme);
    // In-app Log Window model and persistent MRU lists (upstream Log Window / Recent Files).
    engine.rootContext()->setContextProperty("appLog", &appLog);
    engine.rootContext()->setContextProperty("appLogModel", appLog.model());
    RecentFilesManager recentFiles;
    engine.rootContext()->setContextProperty("recentFiles", &recentFiles);
    LanguageManager languageManager(&engine);
    if (!initialLang.isEmpty()) {
        languageManager.setLanguage(initialLang);
    }
    engine.rootContext()->setContextProperty("languageManager", &languageManager);

    SubtitleModel subtitleModel;
    // newDocument() initializes default 0:00:00.00-0:00:05.00 dialogue line
    // and resets modification tracking to pristine state.
    subtitleModel.newDocument();
    app.setSubtitleModel(&subtitleModel);
    engine.rootContext()->setContextProperty("nativeSubtitleModel", &subtitleModel);
    Automation::AutomationManager::instance()->setSubtitleModel(&subtitleModel);

    StyleStorageManager styleStorageManager;
    engine.rootContext()->setContextProperty("styleStorageManager", &styleStorageManager);
    // Inactive line boundaries and karaoke syllable marks require subtitle line data
    audioController.setSubtitleModel(&subtitleModel);

    if (!assPath.isEmpty() && QFileInfo::exists(assPath)) {
        subtitleModel.loadFromFile(assPath);
    }
    if (karaokeMode) {
        audioController.setKaraokeMode(true);
    }

    QStringList candidates = {
        app.applicationDirPath() + "/../qml/Main.qml",
        // Layout produced by `cmake --install`: exe in <prefix>/bin, QML in <prefix>/share/AegisubQT.
        app.applicationDirPath() + "/../share/AegisubQT/qml/Main.qml",
        // macOS .app bundle: resources live in Contents/Resources.
        app.applicationDirPath() + "/../Resources/qml/Main.qml",
        QDir::current().filePath("qml/Main.qml"),
        app.applicationDirPath() + "/qml/Main.qml",
        QDir::current().filePath("Main.qml"),
        app.applicationDirPath() + "/Main.qml",
        app.applicationDirPath() + "/../Main.qml"
    };

    QString mainQml;
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            mainQml = QDir::cleanPath(path);
            break;
        }
    }

    if (mainQml.isEmpty()) {
        qCritical() << "Fatal: Main.qml not found!";
        return -1;
    }

    QString qmlDir = QFileInfo(mainQml).dir().absolutePath();
    QStringList paths = engine.importPathList();
    paths.prepend(qmlDir);
    engine.setImportPathList(paths);
    engine.addImportPath(qmlDir + "/views");
    engine.addImportPath(qmlDir + "/dialogs");
    engine.addImportPath(qmlDir + "/project");
    engine.addImportPath(qmlDir + "/controls");
    // Qt built-in QML module path: prioritize QLibraryInfo (relocatable deployment),
    // with build-time qmake query fallback for in-source developer trees.
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
#ifdef QT_INSTALL_QML_PATH
    engine.addImportPath(QStringLiteral(QT_INSTALL_QML_PATH));
#endif
    qInfo() << "[Aegisub-QtQuick] Import paths:" << engine.importPathList();

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, [](const QUrl &url) {
        qCritical() << "[QML Error] Object creation failed for URL:" << url;
    });
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const auto &w : warnings) {
            qWarning() << "[QML Warning]" << w.toString();
        }
    });

    qInfo() << "[Aegisub-QtQuick] Loading QML from:" << mainQml;
    engine.load(QUrl::fromLocalFile(mainQml));

    qInfo() << "[Aegisub-QtQuick] QML loaded. Root objects count:" << engine.rootObjects().size();
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Fatal: Failed to load QML root object!";
        return -1;
    }

    // Medusa mode: promote audio hotkeys to window-level global overrides
    MedusaKeyFilter medusaFilter(&audioController, &displayController);
    if (auto *rootWindow = qobject_cast<QWindow *>(engine.rootObjects().first())) {
        rootWindow->installEventFilter(&medusaFilter);
    }

    return app.exec();
}

#include "main.moc"
