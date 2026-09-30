#include "AudioClipWriter.h"
#include "AudioPcmProvider.h"

#include <QDataStream>
#include <QFileInfo>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace AudioClipWriter {
namespace {
QVariantMap fail(const QString &message) {
    return {{"success", false}, {"message", message}};
}

qint64 sampleAt(const qint64 timeMs, const int rate, const qint64 total) {
    if (timeMs <= 0) return 0;
    if (timeMs > (std::numeric_limits<qint64>::max() - 999) / rate)
        return total;
    return std::min(total, (timeMs * rate + 999) / 1000);
}
}

QVariantMap save(const AudioPcmProvider &provider, const QUrl &destination,
                 const qint64 startMs, const qint64 endMs) {
    if (!provider.isLoaded() || provider.sampleRate() <= 0)
        return fail(QStringLiteral("No audio is loaded."));
    if (!destination.isLocalFile() || destination.toLocalFile().isEmpty())
        return fail(QStringLiteral("Choose a local WAV output file."));
    if (endMs <= startMs)
        return fail(QStringLiteral("The selected subtitle lines have no audio interval."));

    const int sampleRate = provider.sampleRate();
    const qint64 total = provider.numSamples();
    const qint64 first = sampleAt(startMs, sampleRate, total);
    const qint64 last = sampleAt(endMs, sampleRate, total);
    if (last <= first)
        return fail(QStringLiteral("The selected interval is outside the loaded audio."));
    const quint64 dataBytes = static_cast<quint64>(last - first) * sizeof(qint16);
    if (dataBytes > std::numeric_limits<quint32>::max() - 36)
        return fail(QStringLiteral("The audio clip is too large for a WAV file."));

    const QString path = destination.toLocalFile();
    if (QFileInfo(path).suffix().compare(QLatin1String("wav"), Qt::CaseInsensitive) != 0)
        return fail(QStringLiteral("The output file must have a .wav extension."));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(QStringLiteral("Cannot write audio clip: %1").arg(file.errorString()));

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << quint32(dataBytes + 36);
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(1)
        << quint32(sampleRate) << quint32(sampleRate * sizeof(qint16))
        << quint16(sizeof(qint16)) << quint16(16);
    out.writeRawData("data", 4);
    out << quint32(dataBytes);
    if (out.status() != QDataStream::Ok) {
        file.cancelWriting();
        return fail(QStringLiteral("Cannot write WAV header: %1").arg(file.errorString()));
    }

    constexpr qint64 chunkSamples = 32768;
    std::vector<char> bytes(static_cast<size_t>(chunkSamples * sizeof(qint16)));
    std::vector<float> generated;
    if (provider.isVirtual()) generated.resize(chunkSamples);
    const auto source = provider.samples();
    for (qint64 position = first; position < last; position += chunkSamples) {
        const qint64 count = std::min(chunkSamples, last - position);
        if (provider.isVirtual())
            provider.getAudio(generated.data(), position, static_cast<size_t>(count));
        for (qint64 i = 0; i < count; ++i) {
            qint16 sample;
            if (provider.isVirtual()) {
                const float value = std::clamp(generated[static_cast<size_t>(i)], -1.0f, 1.0f);
                sample = static_cast<qint16>(std::lround(value * 32767.0f));
            } else {
                sample = source[static_cast<size_t>(position + i)];
            }
            const quint16 bits = static_cast<quint16>(sample);
            bytes[static_cast<size_t>(i * 2)] = static_cast<char>(bits & 0xff);
            bytes[static_cast<size_t>(i * 2 + 1)] = static_cast<char>(bits >> 8);
        }
        if (file.write(bytes.data(), count * sizeof(qint16)) != count * sizeof(qint16)) {
            file.cancelWriting();
            return fail(QStringLiteral("Cannot write audio samples: %1").arg(file.errorString()));
        }
    }
    if (!file.commit())
        return fail(QStringLiteral("Cannot save audio clip: %1").arg(file.errorString()));
    return {{"success", true}, {"message", QStringLiteral("Audio clip saved to %1").arg(path)},
            {"sampleRate", sampleRate}, {"sampleCount", last - first}, {"path", path}};
}
}
