#include "AstraCoreBridge.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
static QString dllDirectory() {
    wchar_t buffer[32768] = {};
    const DWORD length = GetDllDirectoryW(32768, buffer);
    if (length >= 32768) throw std::runtime_error("DLL directory too long");
    return QString::fromWCharArray(buffer, int(length));
}
#endif
static void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        if (argc == 4 && QString::fromLocal8Bit(argv[1]) == "child") {
            const bool expected = QString::fromLocal8Bit(argv[2]) == "available";
            const QString sentinel = QString::fromLocal8Bit(argv[3]);
#ifdef Q_OS_WIN
            SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, nullptr);
            require(SetDllDirectoryW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(sentinel).utf16())), "sentinel DLL directory failed");
            const QString before = dllDirectory();
            const DWORD errorMode = GetThreadErrorMode();
#endif
            {
                AstraCoreBridge bridge;
                require(bridge.isAvailable() == expected, "wrong native library availability");
#ifdef Q_OS_WIN
                require(dllDirectory() == before, "bridge changed global DLL directory while alive");
                require(GetThreadErrorMode() == errorMode, "bridge changed thread error mode");
#endif
            }
#ifdef Q_OS_WIN
            require(dllDirectory() == before, "bridge changed global DLL directory after destruction");
#endif
            std::cout << "child path and loader state passed\n";
            return 0;
        }
        require(argc == 4, "expected SCENARIO NATIVE_FIXTURE DEPENDENCY");
        const QString scenario = QString::fromLocal8Bit(argv[1]);
        QTemporaryDir temporary;
        require(temporary.isValid(), "temporary tree failed");
        const QString root = temporary.path();
        const QString executableDir = root + "/legit/bin";
        const QString assets = executableDir + "/assets/bin";
        const QString rogue = root + "/untrusted";
        const QString sentinel = root + "/sentinel";
        require(QDir().mkpath(assets) && QDir().mkpath(rogue) && QDir().mkpath(sentinel), "directory setup failed");
        const QString nativeSource = QString::fromLocal8Bit(argv[2]);
        const QString dependencySource = QString::fromLocal8Bit(argv[3]);
        const QString nativeName = QFileInfo(nativeSource).fileName();
        const QString dependencyName = QFileInfo(dependencySource).fileName();
        const QString child = executableDir + "/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName();
        require(QFile::copy(QCoreApplication::applicationFilePath(), child), "child executable copy failed");
        bool available = scenario == "trusted";
        if (scenario == "cwd" || scenario == "escape") {
            require(QFile::copy(nativeSource, rogue + "/" + nativeName)
                && QFile::copy(dependencySource, rogue + "/" + dependencyName), "rogue fixture copy failed");
            if (scenario == "escape") {
                require(QDir().rmdir(assets), "empty assets removal failed");
#ifdef Q_OS_WIN
                QProcess junction;
                junction.start(qEnvironmentVariable("SystemRoot") + "/System32/cmd.exe",
                    {"/c", "mklink", "/J", QDir::toNativeSeparators(assets), QDir::toNativeSeparators(rogue)});
                require(junction.waitForFinished(5000) && junction.exitCode() == 0, "owned test junction failed");
#else
                require(QFile::link(rogue, assets), "owned test symlink failed");
#endif
            }
        } else if (scenario == "trusted" || scenario == "dependency") {
            require(QFile::copy(nativeSource, assets + "/" + nativeName), "native copy failed");
            require(QFile::copy(dependencySource, (available ? assets : rogue) + "/" + dependencyName), "dependency copy failed");
        } else if (scenario == "broken") {
            QFile bad(assets + "/" + nativeName);
            require(bad.open(QIODevice::WriteOnly) && bad.write("invalid native binary") > 0, "broken fixture failed");
        } else require(false, "unknown scenario");
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("PATH", rogue + QDir::listSeparator() + environment.value("PATH"));
        process.setProcessEnvironment(environment);
        process.setWorkingDirectory(rogue);
        process.start(child, {"child", available ? "available" : "unavailable", sentinel});
        require(process.waitForFinished(10000), "child timed out");
        std::cout << process.readAllStandardOutput().constData();
        std::cerr << process.readAllStandardError().constData();
        require(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0, "child path contract failed");
        std::cout << scenario.toStdString() << " passed\n";
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
