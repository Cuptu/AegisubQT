#include "AstraVideoProvider.h"
#include "AstraCoreBridge.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QLibrary>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTimer>
#include <chrono>
#include <future>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Predicate> bool waitFor(Predicate predicate) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < 2000) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return predicate();
}
struct Control {
    QLibrary library;
    void (*arm)();
    void (*armClose)();
    int (*waitEntered)(int);
    void (*release)();
    int (*calls)();
    int (*unsafeCloses)();
    int (*liveSessions)();
    int (*isBlocked)();
    Control() : library(QCoreApplication::applicationDirPath() + "/assets/bin/" +
#ifdef _WIN32
        "AstraCore.Native.dll"
#elif defined(__APPLE__)
        "libAstraCore.Native.dylib"
#else
        "libAstraCore.Native.so"
#endif
    ) {
        require(library.load(), "fixture library failed to load");
        arm = reinterpret_cast<decltype(arm)>(library.resolve("fixture_arm"));
        armClose = reinterpret_cast<decltype(armClose)>(library.resolve("fixture_arm_close"));
        waitEntered = reinterpret_cast<decltype(waitEntered)>(library.resolve("fixture_wait_entered"));
        release = reinterpret_cast<decltype(release)>(library.resolve("fixture_release"));
        calls = reinterpret_cast<decltype(calls)>(library.resolve("fixture_calls"));
        unsafeCloses = reinterpret_cast<decltype(unsafeCloses)>(library.resolve("fixture_unsafe_closes"));
        liveSessions = reinterpret_cast<decltype(liveSessions)>(library.resolve("fixture_live_sessions"));
        isBlocked = reinterpret_cast<decltype(isBlocked)>(library.resolve("fixture_is_blocked"));
        require(arm && armClose && waitEntered && release && calls && unsafeCloses && liveSessions && isBlocked,
            "fixture controls missing");
    }
};
struct ReleaseGuard { Control &control; bool armed = true; ~ReleaseGuard() { if (armed) control.release(); } };
struct Frame { int number; int width; double pts; };
int (*shutdownLiveSessions)() = nullptr;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        Control control;
        QTemporaryDir directory;
        require(directory.isValid(), "temporary directory failed");
        const QString source = directory.filePath("fixture.media");
        QFile file(source);
        require(file.open(QIODevice::WriteOnly), "fixture media file failed");
        file.write("fixture");
        file.close();
        AstraVideoProvider provider;
        ReleaseGuard releaseGuard{control};
        QVector<Frame> delivered;
        QObject::connect(&provider, &VideoProvider::frameReady, &app,
            [&](int number, const QImage &image, double pts) { delivered.push_back({number, image.width(), pts}); });
        require(provider.open(source), "provider open failed");
        const QString mode = argc > 1 ? QString::fromLocal8Bit(argv[1]) : "stale";

        if (mode == "queued-reopen") {
            // Reserve a pool thread without running the decode job. close()
            // sets stopping before that job can enter its while loop.
            auto *pool = QThreadPool::globalInstance();
            pool->setMaxThreadCount(1);
            std::promise<void> entered, release;
            auto releaseFuture = release.get_future().share();
            pool->start([&] { entered.set_value(); releaseFuture.wait(); });
            entered.get_future().wait();
            provider.requestFrameAsync(1, 1);
            std::jthread unblock([&] {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                release.set_value();
            });
            provider.close();
            unblock.join();
            require(provider.open(source), "reopen after queued decode failed");
            provider.requestFrameAsync(2, 2);
            require(waitFor([&] { return !delivered.isEmpty(); }), "queued close/reopen lost future decode requests");
            require(delivered.size() == 1 && delivered.back().number == 2, "old queued decode leaked across reopen");
        } else if (mode == "native-reopen") {
            for (int iteration = 0; iteration < 10; ++iteration) {
                delivered.clear();
                static_cast<VideoProvider &>(provider).setScrubMode(true);
                control.arm();
                provider.requestFrameAsync(1, 1);
                require(control.waitEntered(2000), "native decode did not start");
                std::jthread unblock([&] {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    control.release();
                });
                provider.close();
                unblock.join();
                require(!provider.isLoaded(), "close left provider loaded");
                provider.close();
                require(provider.open(source), "reopen after native decode failed");
                provider.requestFrameAsync(2, 2);
                require(waitFor([&] { return !delivered.isEmpty(); }), "native close/reopen lost future decode requests");
                require(delivered.size() == 1 && delivered.back().number == 2 && delivered.back().width == 960,
                    "old native decode or scrub state leaked across reopen");
            }
            delivered.clear();
            require(!provider.open(directory.filePath("missing.media")), "missing source must fail to open");
            require(!provider.isLoaded(), "failed open left provider loaded");
            require(provider.open(source), "valid open after failed open failed");
            provider.requestFrameAsync(3, 3);
            require(waitFor([&] { return !delivered.isEmpty(); }) && delivered.back().number == 3,
                "failed open prevented a later successful decode");
        } else if (mode == "metadata") {
            int keySignals = 0, timeSignals = 0;
            QObject::connect(&provider, &VideoProvider::keyframesReady, &app, [&] { ++keySignals; });
            QObject::connect(&provider, &VideoProvider::timecodesReady, &app, [&] { ++timeSignals; });
            provider.extractKeyframesAsync();
            provider.extractTimecodesAsync();
            require(QThreadPool::globalInstance()->waitForDone(2000), "metadata extraction did not finish");
            QCoreApplication::processEvents();
            require(keySignals == 1 && timeSignals == 1 && provider.getKeyframes() == QVector<int64_t>({0, 24}) &&
                provider.getTimecodes() == QVector<double>({0, 1.0 / 24}), "normal metadata delivery or cache changed");
            const auto frame = provider.getFrame(5, 5);
            require(frame.width() == 960 && frame.height() == 540, "synchronous native frame access changed");
            provider.close();
            require(provider.getKeyframes().isEmpty() && provider.getTimecodes().isEmpty() && provider.getFrame(0, 0).isNull(),
                "closed provider retained cached metadata or frame access");
        } else if (mode == "close-active") {
            // The native gate remains closed while close returns, GUI events
            // run, and a fresh generation decodes. A watchdog only releases
            // an old broken implementation so it fails instead of hanging.
            for (int operation = 0; operation < 3; ++operation) {
                int metadataSignals = 0;
                const auto keys = QObject::connect(&provider, &VideoProvider::keyframesReady,
                    &app, [&] { ++metadataSignals; });
                const auto times = QObject::connect(&provider, &VideoProvider::timecodesReady,
                    &app, [&] { ++metadataSignals; });
                delivered.clear();
                control.arm();
                if (operation == 0) provider.requestFrameAsync(1, 1);
                else if (operation == 1) provider.extractKeyframesAsync();
                else provider.extractTimecodesAsync();
                require(control.waitEntered(2000), "native operation did not enter gate");
                std::promise<void> finished;
                auto ready = finished.get_future();
                std::jthread watchdog([&] {
                    if (ready.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) control.release();
                });
                provider.close();
                const bool returnedWhileBlocked = control.isBlocked();
                bool heartbeat = false;
                QTimer::singleShot(0, &app, [&] { heartbeat = true; });
                QCoreApplication::processEvents();
                require(provider.open(source), "open while old native operation blocked failed");
                provider.requestFrameAsync(2, 2);
                const bool newFrameArrived = waitFor([&] { return !delivered.isEmpty(); });
                const bool stillBlocked = control.isBlocked();
                control.release();
                finished.set_value();
                watchdog.join();
                require(returnedWhileBlocked && heartbeat && stillBlocked && newFrameArrived,
                    "close or new generation waited for obsolete native operation");
                require(QThreadPool::globalInstance()->waitForDone(2000), "old native operation did not drain");
                QCoreApplication::processEvents();
                require(metadataSignals == 0 && provider.getKeyframes().isEmpty() && provider.getTimecodes().isEmpty(),
                    "old native scan updated reopened provider");
                require(delivered.size() == 1 && delivered.back().number == 2, "old decoded frame reached reopened generation");
                require(control.liveSessions() == 1, "old native session was not eventually released");
                QObject::disconnect(keys);
                QObject::disconnect(times);
            }
        } else if (mode == "close-idle") {
            control.armClose();
            std::promise<void> finished;
            auto ready = finished.get_future();
            std::jthread watchdog([&] {
                if (ready.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) control.release();
            });
            provider.close();
            const bool nativeCloseStarted = control.waitEntered(2000);
            const bool returnedWhileBlocked = control.isBlocked();
            control.release();
            finished.set_value();
            watchdog.join();
            require(nativeCloseStarted && returnedWhileBlocked, "idle native close blocked GUI caller");
        } else if (mode == "destroy-active") {
            provider.close();
            require(QThreadPool::globalInstance()->waitForDone(2000), "initial session cleanup did not finish");
            for (int operation = 0; operation < 3; ++operation) {
                auto doomed = std::make_unique<AstraVideoProvider>();
                require(doomed->open(source), "doomed provider open failed");
                control.arm();
                if (operation == 0) doomed->requestFrameAsync(1, 1);
                else if (operation == 1) doomed->extractKeyframesAsync();
                else doomed->extractTimecodesAsync();
                require(control.waitEntered(2000), "doomed native operation did not enter gate");
                std::promise<void> finished;
                auto ready = finished.get_future();
                std::jthread watchdog([&] {
                    if (ready.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) control.release();
                });
                doomed.reset();
                const bool returnedWhileBlocked = control.isBlocked();
                control.release();
                finished.set_value();
                watchdog.join();
                require(returnedWhileBlocked, "provider destructor waited for native operation");
                require(QThreadPool::globalInstance()->waitForDone(2000), "destroyed-provider work did not drain");
                QCoreApplication::processEvents();
                require(control.liveSessions() == 0, "destroyed provider leaked native session");
            }
        } else if (mode == "shutdown-active") {
            control.arm();
            provider.requestFrameAsync(1, 1);
            require(control.waitEntered(2000), "shutdown native operation did not enter gate");
            provider.close();
            require(control.isBlocked(), "shutdown close waited for native operation");
            shutdownLiveSessions = control.liveSessions;
            std::atexit([] {
                if (shutdownLiveSessions() != 0) std::abort();
                std::cout << "PASS Qt application shutdown drained native session cleanup\n";
            });
            // Deliberately leave the native call alive until QCoreApplication's
            // destructor drains its global pool; the bridge keeps the DLL loaded.
            std::thread([release = control.release] {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                release();
            }).detach();
            releaseGuard.armed = false;
            std::cout << "PASS shutdown returns with active native work\n";
            return 0;
        } else {
            // Supersede A while native decoding is blocked. Only the latest of
            // B/C should decode next, and A must never reach the consumer.
            control.arm();
            provider.requestFrameAsync(1, 1);
            require(control.waitEntered(2000), "blocked decode did not start");
            const int before = control.calls();
            provider.requestFrameAsync(2, 2);
            provider.requestFrameAsync(3, 3);
            control.release();
            require(QThreadPool::globalInstance()->waitForDone(2000), "coalesced decoding did not finish");
            QCoreApplication::processEvents();
            require(control.calls() == before + 1, "pending requests were not coalesced");
            require(delivered.size() == 1 && delivered.back().number == 3, "superseded native frame reached consumer");

            // A is already queued for GUI delivery when B is submitted.
            delivered.clear();
            provider.requestFrameAsync(4, 4);
            require(QThreadPool::globalInstance()->waitForDone(2000), "first queued decode did not finish");
            provider.requestFrameAsync(5, 5);
            require(QThreadPool::globalInstance()->waitForDone(2000), "second queued decode did not finish");
            QCoreApplication::processEvents();
            require(delivered.size() == 1 && delivered.back().number == 5, "queued stale frame reached consumer");

            // Requests at the same frame number can still supersede one another.
            delivered.clear();
            control.arm();
            provider.requestFrameAsync(9, 9);
            require(control.waitEntered(2000), "same-frame decode did not start");
            provider.requestFrameAsync(9, 9.5);
            control.release();
            require(QThreadPool::globalInstance()->waitForDone(2000), "same-frame decode did not finish");
            QCoreApplication::processEvents();
            require(delivered.size() == 1 && delivered.back().pts == 9.5, "frame number alone failed to reject obsolete request");

            // A downscaled frame must not overwrite the full-resolution request
            // on release, even if both carry the same frame number and time.
            delivered.clear();
            VideoProvider &base = provider;
            base.setScrubMode(true);
            provider.requestFrameAsync(10, 10);
            require(QThreadPool::globalInstance()->waitForDone(2000), "scrub decode did not finish");
            base.setScrubMode(false);
            provider.requestFrameAsync(10, 10);
            require(QThreadPool::globalInstance()->waitForDone(2000), "full-resolution decode did not finish");
            QCoreApplication::processEvents();
            require(delivered.size() == 1 && delivered.back().width == 960, "obsolete scrub proxy reached consumer");

            // close/reopen must suppress an old generation's queued callback.
            delivered.clear();
            provider.requestFrameAsync(11, 11);
            require(QThreadPool::globalInstance()->waitForDone(2000), "old-generation decode did not finish");
            provider.close();
            require(provider.open(source), "generation reopen failed");
            provider.requestFrameAsync(12, 12);
            require(QThreadPool::globalInstance()->waitForDone(2000), "new-generation decode did not finish");
            QCoreApplication::processEvents();
            require(delivered.size() == 1 && delivered.back().number == 12, "old-generation queued callback reached consumer");
        }
        provider.close();
        require(QThreadPool::globalInstance()->waitForDone(2000), "final native cleanup did not drain");
        require(control.liveSessions() == 0, "native session leaked");
        require(control.unsafeCloses() == 0, "native session closed while grab still active");
        std::cout << "PASS Astra provider " << mode.toStdString() << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
