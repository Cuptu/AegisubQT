#include "AudioClipWriter.h"
#include "AudioPcmProvider.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <cstdio>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); return 1; \
} } while (0)

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString sourcePath = dir.filePath("source.wav");
    {
        QFile source(sourcePath);
        CHECK(source.open(QIODevice::WriteOnly));
        QDataStream out(&source);
        out.setByteOrder(QDataStream::LittleEndian);
        out.writeRawData("RIFF", 4);
        out << quint32(2036);
        out.writeRawData("WAVEfmt ", 8);
        out << quint32(16) << quint16(1) << quint16(1)
            << quint32(1000) << quint32(2000) << quint16(2) << quint16(16);
        out.writeRawData("data", 4);
        out << quint32(2000);
        for (int i = 0; i < 1000; ++i) out << qint16(i - 500);
        CHECK(out.status() == QDataStream::Ok);
    }
    AudioPcmProvider provider;
    CHECK(provider.loadWav(sourcePath));
    CHECK(provider.sampleRate() == 1000 && provider.numSamples() == 1000);
    const auto outputPath = dir.filePath("clip.wav");
    const auto outputUrl = QUrl::fromLocalFile(outputPath);
    const auto result = AudioClipWriter::save(provider, outputUrl, 101, 207);
    CHECK(result.value("success").toBool());
    CHECK(result.value("sampleCount").toLongLong() == 106);
    QFile output(outputPath);
    CHECK(output.open(QIODevice::ReadOnly));
    CHECK(output.size() == 44 + 106 * 2);
    QDataStream in(&output);
    in.setByteOrder(QDataStream::LittleEndian);
    char fourcc[5] = {};
    CHECK(in.readRawData(fourcc, 4) == 4 && QByteArray(fourcc) == "RIFF");
    quint32 riffSize = 0;
    in >> riffSize;
    CHECK(riffSize == 36 + 106 * 2);
    CHECK(in.skipRawData(36) == 36);
    qint16 first = 0, last = 0;
    in >> first;
    CHECK(in.skipRawData((106 - 2) * 2) == (106 - 2) * 2);
    in >> last;
    CHECK(first == -399 && last == -294);
    output.close();

    CHECK(!AudioClipWriter::save(provider, outputUrl, 300, 300).value("success").toBool());
    CHECK(!AudioClipWriter::save(provider, outputUrl, 2000, 2100).value("success").toBool());
    CHECK(!AudioClipWriter::save(provider, QUrl("https://example.invalid/a.wav"), 0, 100)
               .value("success").toBool());
    CHECK(QFileInfo(outputPath).size() == 44 + 106 * 2);

    AudioPcmProvider blank;
    CHECK(blank.loadVirtualAudio(AudioPcmProvider::VirtualKind::Blank, 1.0, 1000));
    const auto blankPath = dir.filePath("blank.wav");
    CHECK(AudioClipWriter::save(blank, QUrl::fromLocalFile(blankPath), 100, 300)
              .value("success").toBool());
    QFile blankFile(blankPath);
    CHECK(blankFile.open(QIODevice::ReadOnly));
    const auto blankBytes = blankFile.readAll();
    CHECK(blankBytes.size() == 44 + 200 * 2);
    for (int i = 44; i < blankBytes.size(); ++i) CHECK(blankBytes[i] == 0);
    puts("PASS actual PCM and virtual WAV clip ranges, header/data bytes, invalid range and destination preserve output");
    return 0;
}
