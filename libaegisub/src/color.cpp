// Copyright (c) 2012, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#include <libaegisub/color.h>

#include <libaegisub/format.h>
#include <libaegisub/split.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>

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

    if (str.starts_with("rgb(") && str.ends_with(")")) {
        auto inner = str.substr(4, str.size() - 5);
        std::vector<std::string> parts;
        agi::Split(parts, inner, ',');
        if (parts.size() == 3) {
            int r = std::atoi(parts[0].c_str());
            int g = std::atoi(parts[1].c_str());
            int b = std::atoi(parts[2].c_str());
            dst = agi::Color(static_cast<unsigned char>(std::clamp(r, 0, 255)),
                             static_cast<unsigned char>(std::clamp(g, 0, 255)),
                             static_cast<unsigned char>(std::clamp(b, 0, 255)), 0);
            return true;
        }
    } else if (str.starts_with("rgba(") && str.ends_with(")")) {
        auto inner = str.substr(5, str.size() - 6);
        std::vector<std::string> parts;
        agi::Split(parts, inner, ',');
        if (parts.size() == 4) {
            int r = std::atoi(parts[0].c_str());
            int g = std::atoi(parts[1].c_str());
            int b = std::atoi(parts[2].c_str());
            int a = std::atoi(parts[3].c_str());
            dst = agi::Color(static_cast<unsigned char>(std::clamp(r, 0, 255)),
                             static_cast<unsigned char>(std::clamp(g, 0, 255)),
                             static_cast<unsigned char>(std::clamp(b, 0, 255)),
                             static_cast<unsigned char>(std::clamp(a, 0, 255)));
            return true;
        }
    }

    return false;
}

bool parse_ass_color(agi::Color &dst, std::string_view str) {
    if (str.starts_with('&')) str.remove_prefix(1);
    if (str.starts_with('H') || str.starts_with('h')) str.remove_prefix(1);
    if (str.ends_with('&')) str.remove_suffix(1);

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

    // SSA decimal integer
    bool all_dec = !str.empty();
    for (char c : str) {
        if (!std::isdigit(static_cast<unsigned char>(c)) && c != '-') {
            all_dec = false;
            break;
        }
    }
    if (all_dec) {
        int64_t val = std::atoll(std::string(str).c_str());
        unsigned int abgr = static_cast<unsigned int>(val);
        unsigned char r = abgr & 0xFF;
        unsigned char g = (abgr >> 8) & 0xFF;
        unsigned char b = (abgr >> 16) & 0xFF;
        unsigned char a = (abgr >> 24) & 0xFF;
        dst = agi::Color(r, g, b, a);
        return true;
    }

    return false;
}

} // anonymous namespace

namespace agi {

Color::Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
: r(r), g(g), b(b), a(a) { }

Color::Color(std::string_view str) {
    str = agi::Trim(str);
    if (parse_css_color(*this, str)) return;
    if (parse_ass_color(*this, str)) return;
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
