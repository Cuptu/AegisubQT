// Copyright (c) 2012, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#include <libaegisub/color.h>

#include <libaegisub/format.h>
#include <libaegisub/split.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <cstdint>

namespace {

unsigned char hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

bool parse_css_color(agi::Color &dst, std::string_view str) {
    if (str.starts_with("#")) {
        str.remove_prefix(1);
        if (!std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isxdigit(c) != 0; })) return false;
        if (str.size() == 3) {
            unsigned char r = hex_val(str[0]) * 16 + hex_val(str[0]);
            unsigned char g = hex_val(str[1]) * 16 + hex_val(str[1]);
            unsigned char b = hex_val(str[2]) * 16 + hex_val(str[2]);
            dst = agi::Color(r, g, b, 0);
            return true;
        } else if (str.size() == 6) {
            unsigned char r = hex_val(str[0]) * 16 + hex_val(str[1]);
            unsigned char g = hex_val(str[2]) * 16 + hex_val(str[3]);
            unsigned char b = hex_val(str[4]) * 16 + hex_val(str[5]);
            dst = agi::Color(r, g, b, 0);
            return true;
        } else if (str.size() == 8) {
            unsigned char r = hex_val(str[0]) * 16 + hex_val(str[1]);
            unsigned char g = hex_val(str[2]) * 16 + hex_val(str[3]);
            unsigned char b = hex_val(str[4]) * 16 + hex_val(str[5]);
            unsigned char a = hex_val(str[6]) * 16 + hex_val(str[7]);
            dst = agi::Color(r, g, b, a);
            return true;
        }
        return false;
    }

    const bool alpha = str.starts_with("rgba(");
    if ((alpha || str.starts_with("rgb(")) && str.ends_with(")")) {
        const auto prefix = alpha ? 5 : 4;
        auto inner = str.substr(prefix, str.size() - prefix - 1);
        std::vector<std::string> parts;
        agi::Split(parts, inner, ',');
        if (parts.size() != (alpha ? 4 : 3)) return false;
        unsigned char channels[4]{};
        for (size_t i = 0; i < parts.size(); ++i) {
            const auto value = agi::Trim(parts[i]);
            if (value.empty() || value.size() > 3) return false;
            unsigned number = 0;
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
            if (error != std::errc() || end != value.data() + value.size() || number > 255) return false;
            channels[i] = static_cast<unsigned char>(number);
        }
        dst = agi::Color(channels[0], channels[1], channels[2], channels[3]);
        return true;
    }

    return false;
}

bool parse_ass_color(agi::Color &dst, std::string_view str) {
    const bool has_ass_prefix = str.starts_with('&') || str.starts_with('H') || str.starts_with('h') || str.ends_with('&');
    if (str.starts_with('&')) str.remove_prefix(1);
    if (str.starts_with('H') || str.starts_with('h')) str.remove_prefix(1);
    if (str.ends_with('&')) str.remove_suffix(1);

    // Bare SSA numbers are decimal, even when every digit is also a hex digit.
    if (!has_ass_prefix && !str.empty()) {
        const auto number = str.starts_with('+') ? str.substr(1) : str;
        int64_t decimal = 0;
        const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), decimal, 10);
        if (error == std::errc() && end == number.data() + number.size() && decimal >= INT32_MIN && decimal <= UINT32_MAX) {
            const auto abgr = static_cast<uint32_t>(decimal);
            dst = agi::Color(abgr & 0xFF, (abgr >> 8) & 0xFF,
                             (abgr >> 16) & 0xFF, (abgr >> 24) & 0xFF);
            return true;
        }
    }

    bool all_hex = !str.empty();
    for (char c : str) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            all_hex = false;
            break;
        }
    }

    if (all_hex && str.size() <= 8) {
        unsigned int val = 0;
        for (char c : str) {
            val = (val << 4) | hex_val(c);
        }
        if (str.size() <= 6) {
            // BBGGRR (up to 6 digits, e.g. \c&HFF& = Red)
            unsigned char r = val & 0xFF;
            unsigned char g = (val >> 8) & 0xFF;
            unsigned char b = (val >> 16) & 0xFF;
            dst = agi::Color(r, g, b, 0);
        } else {
            // AABBGGRR (7 or 8 digits)
            unsigned char r = val & 0xFF;
            unsigned char g = (val >> 8) & 0xFF;
            unsigned char b = (val >> 16) & 0xFF;
            unsigned char a = (val >> 24) & 0xFF;
            dst = agi::Color(r, g, b, a);
        }
        return true;
    }

    return false;
}

} // anonymous namespace

namespace agi {

Color::Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
: r(r), g(g), b(b), a(a) { }

Color::Color(std::string_view str) {
    TryParse(*this, str);
}

bool Color::TryParse(Color &result, std::string_view str) {
    str = agi::Trim(str);
    Color parsed;
    if (!parse_css_color(parsed, str) && !parse_ass_color(parsed, str)) return false;
    result = parsed;
    return true;
}

std::string Color::GetAssStyleFormatted() const {
    return agi::format("&H%02X%02X%02X%02X", a, b, g, r);
}

std::string Color::GetAssOverrideFormatted() const {
    return agi::format("&H%02X%02X%02X&", b, g, r);
}

std::string Color::GetSsaFormatted() const {
    return std::to_string((a << 24) + (b << 16) + (g << 8) + r);
}

std::string Color::GetHexFormatted(bool rgba) const {
    if (rgba)
        return agi::format("#%02X%02X%02X%02X", r, g, b, a);
    return agi::format("#%02X%02X%02X", r, g, b);
}

std::string Color::GetRgbFormatted() const {
    if (a)
        return agi::format("rgba(%d, %d, %d, %d)", r, g, b, a);
    return agi::format("rgb(%d, %d, %d)", r, g, b);
}

} // namespace agi
