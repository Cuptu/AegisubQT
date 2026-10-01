#!/usr/bin/env python3
"""Generate small PNG/WAV inputs using only Python's standard library."""
import math
from pathlib import Path
import struct
import sys
import wave
import zlib


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def generate(directory):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    for frame in range(10):
        pixels = b''.join(b'\0' + bytes(v for x in range(96)
            for v in ((x * 2 + frame * 7) % 256, y * 3 % 256, 80 + frame * 10))
            for y in range(64))
        png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 96, 64, 8, 2, 0, 0, 0))
        png += chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b'')
        (directory / f'frame{frame:02d}.png').write_bytes(png)
    with wave.open(str(directory / 'tone.wav'), 'wb') as output:
        output.setparams((1, 2, 16000, 16000, 'NONE', 'not compressed'))
        output.writeframes(b''.join(struct.pack('<h', round(8000 * math.sin(2 * math.pi * 440 * i / 16000)))
                                   for i in range(16000)))


if __name__ == '__main__':
    generate(sys.argv[1])
