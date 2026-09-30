#include "AudioController.h"
#include "SpectrogramItem.h"
#include <QQuickWindow>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDataStream>
#include <QThreadPool>
#include <QRunnable>
#include <QSemaphore>
#include <QElapsedTimer>
#include <QThread>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <cstdio>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); return 1; } } while (0)
static bool writeWave(const QString &path, int amplitude, int rate = 16000, int count = 16000, int impulse = -1) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    file.write("RIFF",4); stream << quint32(36 + count * 2);
    file.write("WAVEfmt ",8); stream << quint32(16) << quint16(1) << quint16(1)
        << quint32(rate) << quint32(rate * 2) << quint16(2) << quint16(16);
    file.write("data",4); stream << quint32(count * 2);
    for (int sample = 0; sample < count; ++sample)
        stream << qint16(impulse >= 0 ? (sample == impulse ? amplitude : 0) : sample % 2 ? amplitude : -amplitude);
    return stream.status() == QDataStream::Ok;
}
static bool waitLoaded(AudioController &controller) {
    QElapsedTimer time;
    time.start();
    while (controller.isLoadingAudio() && time.elapsed() < 10000) QTest::qWait(10);
    return !controller.isLoadingAudio();
}
struct WorkerGate {
    QSemaphore entered, release;
    bool unlocked = false;
    WorkerGate() {
        QThreadPool::globalInstance()->start(QRunnable::create([this] { entered.release(); release.acquire(); }));
        entered.acquire();
    }
    void unlock() { if (!unlocked) { unlocked = true; release.release(); } }
    ~WorkerGate() { unlock(); QThreadPool::globalInstance()->waitForDone(); }
};
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    if (!AstraCoreBridge::instance()->isAvailable()) {
#ifdef AEGISUB_TEST_EXPECT_NATIVE
        CHECK(false); // A source-built runtime must have been deployed for this test.
#else
        puts("SKIP audio load integration: no native media runtime in this optional-runtime build");
        return 77;
#endif
    }
    QTemporaryDir temp;
    CHECK(temp.isValid());
    const QString first = temp.filePath("first.wav"), second = temp.filePath("second.wav"), bad = temp.filePath("invalid.bin"), missing = temp.filePath("missing.wav");
    CHECK(writeWave(first, 12000) && writeWave(second, 24000));
    { QFile file(bad); CHECK(file.open(QIODevice::WriteOnly)); CHECK(file.write("not an audio file") == 17); }
    QThreadPool::globalInstance()->setMaxThreadCount(1);
    AudioController controller(nullptr);
    QSignalSpy errors(&controller, &AudioController::audioError);
    controller.openAudio(first);
    CHECK(waitLoaded(controller) && controller.hasAudio() && errors.isEmpty());
    CHECK(controller.audioPath() == first && controller.audioDuration() == 1.0 && controller.pcmProvider().sampleRate() == 16000);
    float before[16]{};
    controller.pcmProvider().getAudio(before, 0, 16);
    const auto peaks = controller.getWaveformPeaks(0, 100, 10);
    const auto spectrumRevision = controller.stftCore().revision();
    CHECK(before[0] == -12000.0f/32768 && before[1] == 12000.0f/32768 && peaks[0].toMap().value("peak").toFloat() > 0.3f);
    controller.openAudio(bad);
    CHECK(waitLoaded(controller) && errors.size() == 1);
    CHECK(controller.hasAudio() && controller.audioPath() == first && controller.audioDuration() == 1.0);
    float after[16]{};
    controller.pcmProvider().getAudio(after, 0, 16);
    CHECK(std::equal(std::begin(before), std::end(before), std::begin(after)));
    CHECK(controller.getWaveformPeaks(0, 100, 10) == peaks && controller.stftCore().revision() == spectrumRevision);
    controller.openAudio(missing);
    CHECK(!controller.isLoadingAudio() && controller.hasAudio() && controller.audioPath() == first && errors.size() == 2);
    CHECK(!controller.openAudioFromVideo(missing) && controller.audioPath() == first && errors.size() == 3);
    {
        WorkerGate gate;
        controller.openAudio(second);
        CHECK(controller.isLoadingAudio() && controller.audioPath() == first);
        controller.openAudio(missing);
        CHECK(!controller.isLoadingAudio() && controller.audioPath() == first);
        gate.unlock();
        CHECK(QThreadPool::globalInstance()->waitForDone(10000));
        QCoreApplication::processEvents();
        CHECK(controller.audioPath() == first && controller.hasAudio() && errors.size() == 4);
    }
    controller.openAudio(second);
    CHECK(waitLoaded(controller) && controller.audioPath() == second && controller.hasAudio());
    controller.pcmProvider().getAudio(after, 0, 16);
    CHECK(after[0] == -24000.0f/32768 && after[1] == 24000.0f/32768);
    CHECK(controller.getWaveformPeaks(0, 100, 10) != peaks);
    {
        WorkerGate gate;
        controller.openAudio(bad);
        CHECK(controller.openBlankAudio());
        gate.unlock();
        CHECK(QThreadPool::globalInstance()->waitForDone(10000));
        QCoreApplication::processEvents();
        CHECK(controller.hasAudio() && controller.audioPath() == "virtual://blank" && errors.size() == 4);
    }
    {
        WorkerGate gate;
        controller.openAudio(second);
        controller.closeAudio();
        gate.unlock();
        CHECK(QThreadPool::globalInstance()->waitForDone(10000));
        QCoreApplication::processEvents();
        CHECK(!controller.hasAudio() && controller.audioPath().isEmpty() && !controller.pcmProvider().isLoaded() && !controller.isLoadingAudio());
    }
    controller.openAudio(bad);
    CHECK(waitLoaded(controller) && errors.size() == 5 && !controller.hasAudio() && !controller.pcmProvider().isLoaded());
    const auto impulse = temp.filePath("impulse.wav"), tail = temp.filePath("tail.wav");
    CHECK(writeWave(impulse, 30000, 16000, 16000, 400)); // 25ms, away from a coarse pixel's start.
    controller.openAudio(impulse);
    CHECK(waitLoaded(controller) && controller.hasAudio());
    const auto coarse = controller.getWaveformPeaks(0, 100, 1);
    CHECK(coarse.size() == 1 && coarse[0].toMap().value("peak").toFloat() == 30000.0f/32768);
    const auto pair = controller.getWaveformPeaks(0, 100, 2);
    CHECK(pair[0].toMap().value("peak").toFloat() == 30000.0f/32768 && pair[1].toMap().value("peak").toFloat() == 0);
    {
        const auto renderImpulse = temp.filePath("render-impulse.wav");
        CHECK(writeWave(renderImpulse, 30000, 16000, 16000, 2000)); // 125ms, away from window edges and marker lines.
        controller.openAudio(renderImpulse);
        CHECK(waitLoaded(controller) && controller.hasAudio());
        QQuickWindow window;
        window.resize(100, 100);
        window.setColor(QColor(8,4,13));
        SpectrogramItem view(window.contentItem());
        view.setSize(QSizeF(100,100));
        view.setAudioController(&controller);
        controller.setDrawSeconds(false);
        controller.setDrawVideoPosition(false);
        controller.clearCursor();
        controller.setSelection(20000, 21000);
        controller.setSpectrumMode(false);
        controller.setMsPerPixel(100);
        const auto waveformRevision = controller.stftCore().revision();
        window.show();
        QTest::qWait(100);
        auto image = window.grabWindow();
        CHECK(!image.isNull());
        const double dpr = window.devicePixelRatio(); // Windows may enforce a wider minimum top-level window.
        CHECK(image.width() >= int(100*dpr) && image.height() >= int(100*dpr));
        CHECK(image.pixelColor(int(1*dpr), int(30*dpr)) == QColor(89,145,220));
        CHECK(image.pixelColor(int(0*dpr), int(30*dpr)) == QColor(8,4,13));
        CHECK(image.pixelColor(int(50*dpr), int(30*dpr)) == QColor(8,4,13));
        const auto output = qEnvironmentVariable("AEGISUB_WAVEFORM_SCREENSHOT");
        if (!output.isEmpty()) CHECK(image.save(output));
        controller.setMsPerPixel(1);
        controller.scrollTo(100);
        QTest::qWait(60);
        image = window.grabWindow();
        CHECK(image.pixelColor(int(25*dpr), int(30*dpr)) == QColor(89,145,220));
        CHECK(image.pixelColor(int(50*dpr), int(30*dpr)) == QColor(8,4,13));
        if (!output.isEmpty()) CHECK(image.save(output + ".zoom.png"));
        CHECK(controller.stftCore().revision() == waveformRevision); // Waveform painting must not compute a spectrum.
        for (int transition = 0; transition < 3; ++transition) {
            controller.setSpectrumMode(true);
            QElapsedTimer spectrumWait; spectrumWait.start();
            while (!view.spectrumResult() && spectrumWait.elapsed() < 10000) QTest::qWait(1);
            CHECK(view.spectrumResult() && !view.spectrumResult()->image.isNull());
            CHECK(view.spectrumResult()->workerThread != reinterpret_cast<quintptr>(QThread::currentThreadId()));
            CHECK(controller.stftCore().stftTexture().isNull()); // The GUI controller core never computes an FFT window.
            QTest::qWait(20);
            const auto spectrumImage = window.grabWindow();
            if (window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
                CHECK(spectrumImage.pixelColor(int(25*dpr), int(40*dpr)) != QColor(Qt::black));
                CHECK(spectrumImage.pixelColor(int(25*dpr), int(40*dpr)) != QColor(8,4,13));
            }
            if (!output.isEmpty() && transition == 0) CHECK(spectrumImage.save(output + ".impulse-spectrum.png"));
            controller.setSpectrumMode(false);
            QTest::qWait(60);
            CHECK(window.grabWindow().pixelColor(int(25*dpr), int(30*dpr)) == QColor(89,145,220));
        }
        controller.setVerticalZoom(0);
        QTest::qWait(60);
        CHECK(window.grabWindow().pixelColor(int(25*dpr), int(30*dpr)) == QColor(8,4,13));
        controller.setVerticalZoom(100);
        QTest::qWait(60);
        CHECK(window.grabWindow().pixelColor(int(25*dpr), int(30*dpr)) == QColor(89,145,220));
        controller.openAudio(second); // Same duration; waveform texture must still be invalidated.
        CHECK(waitLoaded(controller));
        QTest::qWait(60);
        CHECK(window.grabWindow().pixelColor(int(50*dpr), int(40*dpr)) == QColor(89,145,220));
        controller.openAudio(renderImpulse);
        CHECK(waitLoaded(controller));
        controller.scrollTo(100);
        QTest::qWait(60);
        CHECK(window.grabWindow().pixelColor(int(50*dpr), int(30*dpr)) == QColor(8,4,13));
        controller.scrollTo(200);
        QTest::qWait(60);
        image = window.grabWindow();
        CHECK(image.pixelColor(int(25*dpr), int(30*dpr)) == QColor(8,4,13));
        controller.closeAudio();
        QTest::qWait(50);
        CHECK(window.grabWindow().pixelColor(int(25*dpr), int(30*dpr)) == QColor(8,4,13));
        // A real production item must submit viewport work without waiting for a busy pool.
        CHECK(controller.openBlankAudio());
        controller.setMsPerPixel(1);
        controller.setSpectrumMode(true);
        {
            WorkerGate gate;
            QTest::qWait(20);
            CHECK(!view.spectrumResult());
            for (int offset = 0; offset < 20; ++offset) {
                controller.scrollTo(100000 + offset * 10000);
                window.grabWindow(); // Forces scene-graph updates while the FFT worker is blocked.
            }
            CHECK(!view.spectrumResult());
            gate.unlock();
            QElapsedTimer spectrumWait; spectrumWait.start();
            while (!view.spectrumResult() && spectrumWait.elapsed() < 10000) QTest::qWait(1);
            const auto renderedWindow = view.spectrumResult();
            CHECK(renderedWindow && !renderedWindow->image.isNull());
            const int frame = int(290000.0 * controller.pcmProvider().sampleRate() / (1000 * controller.stftCore().hopSamples()));
            CHECK(renderedWindow->start <= frame && renderedWindow->start + renderedWindow->count * renderedWindow->step > frame);
            CHECK(controller.stftCore().stftTexture().isNull());
            printf("PASS production spectrum worker latest scrub frame %d, cache miss %lld us\n", frame, renderedWindow->computeNanoseconds/1000);
            QTest::qWait(20); // Deliver the worker's queued GUI update before capturing the shader output.
            image = window.grabWindow();
            const auto api = window.rendererInterface()->graphicsApi();
            printf("production spectrum graphics API %d\n", int(api));
            if (!output.isEmpty()) CHECK(image.save(output + ".spectrum.png"));
            printf("spectrum pixel %s, palette zero %s, selection %d..%d\n",
                image.pixelColor(int(50*dpr), int(40*dpr)).name().toUtf8().constData(),
                controller.stftCore().paletteTexture().pixelColor(0,0).name().toUtf8().constData(),
                controller.selectionStart(),controller.selectionEnd());
            if (api != QSGRendererInterface::Software) CHECK(image.pixelColor(int(50*dpr), int(40*dpr)) == QColor(Qt::black));
        }
        controller.closeAudio();
        CHECK(!view.spectrumResult());
        view.setAudioController(nullptr);
        CHECK(controller.openBlankAudio());
        {
            WorkerGate gate;
            auto *closingView = new SpectrogramItem(window.contentItem());
            closingView->setSize(QSizeF(100,100));
            closingView->setAudioController(&controller);
            window.grabWindow();
            CHECK(!closingView->spectrumResult());
            delete closingView; // GUI destruction must not wait for the blocked worker or leave a live callback target.
            controller.closeAudio();
            gate.unlock();
            CHECK(QThreadPool::globalInstance()->waitForDone(10000));
            QCoreApplication::processEvents();
        }
        puts("PASS actual production waveform scene graph renders interior coarse/zoomed impulse, amplitude changes, same-duration source replacement, scroll silence and close cleanup");
    }
    controller.openAudio(impulse);
    CHECK(waitLoaded(controller) && controller.hasAudio());
    const auto zoom = controller.getWaveformPeaks(20, 30, 10);
    CHECK(zoom.size() == 10);
    for (const auto &pixel : zoom) CHECK(pixel.toMap().value("peak").toFloat() == 30000.0f/32768);
    const auto padded = controller.getWaveformPeaks(-100, 100, 4);
    CHECK(padded[0].toMap().value("peak").toFloat() == 0 && padded[1].toMap().value("peak").toFloat() == 0);
    CHECK(padded[2].toMap().value("peak").toFloat() == 30000.0f/32768 && padded[3].toMap().value("peak").toFloat() == 0);
    CHECK(controller.getWaveformPeaks(100, 0, 1).isEmpty() && controller.getWaveformPeaks(0, 0, 1).isEmpty());
    CHECK(controller.getWaveformPeaks(0, 100, 0).isEmpty());
    CHECK(writeWave(tail, 30000, 11025, 11026, 11025)); // Partial final 10ms bucket at a non-divisible sample rate.
    controller.openAudio(tail);
    CHECK(waitLoaded(controller) && controller.pcmProvider().sampleRate() == 11025);
    const auto tailPeak = controller.getWaveformPeaks(1000, 1010, 1);
    CHECK(tailPeak[0].toMap().value("peak").toFloat() == 30000.0f/32768);
    CHECK(controller.getWaveformPeaks(1010, 2000, 1)[0].toMap().value("peak").toFloat() == 0);
    puts("PASS actual decoded waveform interval aggregation retains off-start impulses, zoom-in buckets, negative padding, invalid ranges and non-divisible-rate partial tail");
    {
        WorkerGate gate;
        auto *destroyed = new AudioController(nullptr);
        destroyed->openAudio(second);
        delete destroyed;
        gate.unlock();
        CHECK(QThreadPool::globalInstance()->waitForDone(10000));
        QCoreApplication::processEvents();
    }
    puts("PASS actual AudioController failed replacement retains coherent PCM/path/duration/waveform/spectrum, invalid request supersedes queued success, success replaces PCM, virtual/close/deletion suppress stale results");
    return 0;
}
