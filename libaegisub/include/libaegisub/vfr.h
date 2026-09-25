// Copyright (c) 2010, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <cstdint>
#include <initializer_list>
#include <vector>

#include <libaegisub/exception.h>
#include <libaegisub/fs.h>

namespace agi::vfr {

enum Time {
    EXACT,
    START,
    END
};

DEFINE_EXCEPTION(Error, Exception);
DEFINE_EXCEPTION(InvalidFramerate, Error);
DEFINE_EXCEPTION(UnknownFormat, Error);
DEFINE_EXCEPTION(MalformedLine, Error);

class Framerate {
    int64_t denominator = 0;
    int64_t numerator = 0;
    int64_t last = 0;
    std::vector<int> timecodes;
    bool drop = false;

    void SetFromTimecodes();

public:
    Framerate(Framerate const&) = default;
    Framerate& operator=(Framerate const&) = default;

    Framerate(agi::fs::path const& filename);
    Framerate(double fps = 0.);
    Framerate(int64_t numerator, int64_t denominator, bool drop = true);
    Framerate(std::vector<int> timecodes);
    Framerate(std::initializer_list<int> timecodes);

    int FrameAtTime(int ms, Time type = EXACT) const;
    int TimeAtFrame(int frame, Time type = EXACT) const;

    void SmpteAtTime(int ms, int *h, int *m, int *s, int *f) const;
    void SmpteAtFrame(int frame, int *h, int *m, int *s, int *f) const;
    int FrameAtSmpte(int h, int m, int s, int f) const;
    int TimeAtSmpte(int h, int m, int s, int f) const;

    void Save(agi::fs::path const& file, int length = -1) const;

    bool IsVFR() const { return timecodes.size() > 1; }
    bool IsLoaded() const { return numerator > 0; }
    double FPS() const { return denominator ? double(numerator) / denominator : 0.0; }
    bool NeedsDropFrames() const { return drop; }
};

} // namespace agi::vfr
