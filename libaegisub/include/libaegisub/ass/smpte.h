// Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <libaegisub/vfr.h>

#include <string>

namespace agi {
class Time;

class SmpteFormatter {
    vfr::Framerate fps;
    char sep;

public:
    SmpteFormatter(vfr::Framerate fps, char sep = ':');

    std::string ToSMPTE(Time time) const;
    Time FromSMPTE(std::string const& str) const;
};

} // namespace agi
