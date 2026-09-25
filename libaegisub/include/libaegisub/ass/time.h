// Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <string>
#include <string_view>

namespace agi {

class Time {
    int time = 0;

public:
    Time(int ms = 0);
    Time(std::string_view text);

    operator int() const { return (time + 5) - (time + 5) % 10; }

    std::string GetAssFormatted(bool ms = false) const;
    std::string GetSrtFormatted() const;
};

} // namespace agi
