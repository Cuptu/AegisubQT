// Copyright (c) 2012, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <string>
#include <string_view>

namespace agi {

struct Color {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    unsigned char a = 0;

    Color() = default;
    Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 0);
    Color(std::string_view str);

    bool operator==(Color const&) const noexcept = default;

    std::string GetAssStyleFormatted() const;
    std::string GetAssOverrideFormatted() const;
    std::string GetSsaFormatted() const;
    std::string GetHexFormatted(bool rgba = false) const;
    std::string GetRgbFormatted() const;

    operator std::string() const { return GetRgbFormatted(); }
};

} // namespace agi
