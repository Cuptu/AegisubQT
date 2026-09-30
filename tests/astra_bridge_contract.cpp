#include "AstraCoreBridge.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QLibrary>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
}
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    try {
        QLibrary controls(QCoreApplication::applicationDirPath() + "/assets/bin/" +
#ifdef _WIN32
            "AstraCore.Native.dll"
#elif defined(__APPLE__)
            "libAstraCore.Native.dylib"
#else
            "libAstraCore.Native.so"
#endif
        );
        require(controls.load(), "fixture load failed");
        auto mode = reinterpret_cast<void(*)(int)>(controls.resolve("fixture_mode"));
        auto calls = reinterpret_cast<int(*)()>(controls.resolve("fixture_calls"));
        require(mode && calls, "fixture controls missing");
        QTemporaryDir directory;
        const QString input = directory.filePath("owned.media");
        QFile file(input); require(file.open(QIODevice::WriteOnly), "fixture file failed"); file.close();
        require(argc == 2, "scenario required");
        const QString scenario = QString::fromLocal8Bit(argv[1]);
        mode(scenario == "abi" ? 1 : 0);
        AstraCoreBridge bridge;
        if (scenario == "abi") {
            require(!bridge.isAvailable(), "incompatible ABI was accepted");
            MediaInfo info;
            require(!bridge.probe(input, info), "incompatible ABI probe executed");
            require(bridge.grabFrameImage(input, 0, 8, 8).isNull(), "incompatible ABI frame executed");
            require(!bridge.openVideoSession(input), "incompatible ABI session executed");
            require(bridge.extractWaveformPeaks(input).isEmpty(), "incompatible ABI waveform executed");
            require(bridge.extractSpectrogram(input).isEmpty(), "incompatible ABI spectrum executed");
            require(calls() == 0, "native operation executed after ABI rejection");
        } else {
            require(bridge.isAvailable(), "compatible ABI rejected");
            if (scenario == "frames") {
                double actual = -1;
                require(bridge.grabFrameImage(input, 0.5, 8, 8, &actual).size() == QSize(8, 8)
                    && actual == 0.5, "valid frame rejected");
                void *session = bridge.openVideoSession(input);
                require(session, "valid session rejected");
                for (int invalid : {2, 3, 4}) {
                    mode(invalid); actual = -1;
                    require(bridge.grabFrameImage(input, 0.5, 8, 8, &actual).isNull(), "invalid frame output accepted");
                    require(actual == -1, "failure overwrote timestamp");
                    require(bridge.grabSessionFrame(session, 0.5, 8, 8, &actual).isNull(), "invalid session output accepted");
                    require(actual == -1, "session failure overwrote timestamp");
                }
                mode(0);
                require(bridge.grabSessionFrame(session, 0.5, 8, 8).size() == QSize(8, 8), "valid session frame rejected");
                bridge.closeVideoSession(session);
                mode(0);
                require(bridge.grabFrameImage(input, std::numeric_limits<double>::quiet_NaN(), 8, 8).isNull(), "NaN time accepted");
                require(bridge.grabFrameImage(input, 0, -1, 8).isNull(), "negative dimension accepted");
                require(calls() == 0, "invalid frame input reached native");
            } else if (scenario == "spectrum") {
                int bins = -1;
                require(bridge.extractSpectrogram(input, 16000, 16, 8, &bins, 4).size() == 16 && bins == 8,
                    "valid spectrum rejected");
                for (int invalid : {5, 6}) {
                    mode(invalid); bins = -1;
                    require(bridge.extractSpectrogram(input, 16000, 16, 8, &bins, 4).isEmpty(), "invalid spectrum output accepted");
                    require(bins == -1, "failure overwrote bins");
                }
                mode(0);
                require(bridge.extractSpectrogram(input, 16000, 1 << 30, 8, nullptr, 10).isEmpty(), "oversized spectrum accepted");
                require(bridge.extractSpectrogram(input, 16000, 512, 8, nullptr, std::numeric_limits<int>::max()).isEmpty(), "overflowing capacity accepted");
                require(bridge.extractSpectrogram(input, 16000, 17, 8, nullptr, 4).isEmpty(), "non-power-of-two FFT accepted");
                require(calls() == 0, "invalid spectrum input reached native");
            } else if (scenario == "metadata") {
                MediaInfo info; require(bridge.probe(input, info), "valid metadata rejected");
                for (int invalid : {7, 8}) {
                    mode(invalid); info.width = 123;
                    require(!bridge.probe(input, info) && info.width == 123, "invalid metadata accepted or output mutated");
                }
                for (int invalid : {9, 11}) {
                    mode(invalid); double duration = -1;
                    require(bridge.extractWaveformPeaks(input, 4000, 100, 4, &duration).isEmpty(), "invalid waveform accepted");
                    require(duration == -1, "failure overwrote waveform duration");
                }
                mode(0);
                require(bridge.extractWaveformPeaks(input, 4000, 100, 4).size() == 1, "valid waveform rejected");
                mode(10); bool hdr; int depth, primaries, transfer;
                require(!bridge.probeHdr(input, hdr, depth, primaries, transfer), "invalid HDR struct accepted");
            } else require(false, "unknown scenario");
        }
        std::cout << scenario.toStdString() << " contract passed\n";
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
