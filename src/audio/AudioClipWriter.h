#pragma once

#include <QUrl>
#include <QVariantMap>

class AudioPcmProvider;

namespace AudioClipWriter {
// Save the half-open millisecond interval as mono 16-bit PCM WAV.
QVariantMap save(const AudioPcmProvider &provider, const QUrl &destination,
                 qint64 startMs, qint64 endMs);
}
