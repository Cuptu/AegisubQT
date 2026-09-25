// Copyright (c) 2026, arch1t3cht <arch1t3cht@gmail.com>
// Modernized C++20 implementation for AegisubQtQuick
#include <libaegisub/ycbcr.h>

#include <libaegisub/split.h>

#include <algorithm>
#include <cctype>

namespace agi::ycbcr {

namespace {

header_variant parse_ycbcr_header(std::string const& matrix) {
    ycbcr_matrix CM = ycbcr_matrix::Unspecified;
    ycbcr_range CR = ycbcr_range::Unspecified;

    std::string lower = matrix;
    for (char &c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::string_view sv = agi::Trim(lower);

    if (sv.empty())
        return header_missing{};

    if (sv == "none")
        return header_none{};

    std::vector<std::string> parts;
    agi::Split(parts, sv, '.');
    if (parts.size() == 2) {
        if (parts[0] == "tv") {
            CR = ycbcr_range::MPEG;
        } else if (parts[0] == "pc") {
            CR = ycbcr_range::JPEG;
        }

        if (parts[1] == "709") {
            CM = ycbcr_matrix::BT709;
        } else if (parts[1] == "601") {
            CM = ycbcr_matrix::SMPTE170M;
        } else if (parts[1] == "fcc") {
            CM = ycbcr_matrix::FCC;
        } else if (parts[1] == "240m") {
            CM = ycbcr_matrix::SMPTE240M;
        }
    }

    if (CM == ycbcr_matrix::Unspecified || CR == ycbcr_range::Unspecified)
        return header_invalid{};

    // Brace-init: parenthesized aggregate initialization (P0960) is not
    // implemented by Apple clang 15, unlike MSVC/GCC.
    return header_colorspace{CM, CR};
}

} // anonymous namespace

const char *matrix_to_string(ycbcr_matrix matrix) {
    using enum ycbcr_matrix;
    switch (matrix) {
        case RGB: return "RGB";
        case BT709: return "BT.709";
        case Unspecified: return "Unspecified";
        case FCC: return "FCC";
        case BT470BG: return "BT.470-BG";
        case SMPTE170M: return "SMPTE ST 170M";
        case SMPTE240M: return "SMPTE ST 240M";
        case YCoCg: return "YCoCg";
        case BT2020_NCL: return "BT.2020 NCL";
        case BT2020_CL: return "BT.2020 CL";
        case SMPTE2085: return "SMPTE 2085";
        case ChromaticityDerivedNCL: return "Chromaticity-derived NCL";
        case ChromaticityDerivedCL: return "Chromaticity-derived CL";
        case ICtCp: return "ICtCp";
        default: return "<Unknown or invalid matrix>";
    }
}

const char *range_to_string(ycbcr_range range) {
    using enum ycbcr_range;
    switch (range) {
        case Unspecified: return "Unspecified";
        case MPEG: return "Limited";
        case JPEG: return "Full";
        default: return "<Unknown or invalid range>";
    }
}

Header::Header(header_variant v) : header_variant(v) {
    if (auto *cs = std::get_if<header_colorspace>(this)) {
        if (cs->matrix == ycbcr_matrix::BT470BG)
            cs->matrix = ycbcr_matrix::SMPTE170M;
    }
}

Header::Header(std::string const& matrix) : Header(parse_ycbcr_header(matrix)) {}

bool Header::valid() const {
    return to_string().has_value();
}

std::optional<std::string> Header::to_string() const {
    if (auto *cs = std::get_if<header_colorspace>(this)) {
        std::string result;
        switch (cs->range) {
            case ycbcr_range::MPEG: result = "TV"; break;
            case ycbcr_range::JPEG: result = "PC"; break;
            default: return std::nullopt;
        }

        switch (cs->matrix) {
            case ycbcr_matrix::BT709: return result + ".709";
            case ycbcr_matrix::FCC: return result + ".FCC";
            case ycbcr_matrix::BT470BG:
            case ycbcr_matrix::SMPTE170M: return result + ".601";
            case ycbcr_matrix::SMPTE240M: return result + ".240M";
            default: return std::nullopt;
        }
    } else if (std::get_if<header_none>(this)) {
        return "None";
    } else if (std::get_if<header_missing>(this)) {
        return "";
    }

    return std::nullopt;
}

Header Header::to_effective() const {
    return valid() && !std::holds_alternative<header_missing>(*this) ? *this : Header(ycbcr_matrix::SMPTE170M, ycbcr_range::MPEG);
}

Header Header::to_existing() const {
    if (valid()) return *this;
    if (std::holds_alternative<header_colorspace>(*this))
        return Header(header_none{});
    return Header(header_missing{});
}

Header Header::to_best_practice() const {
    if (auto *cs = std::get_if<header_colorspace>(this)) {
        switch (cs->matrix) {
            case ycbcr_matrix::BT709:
            case ycbcr_matrix::SMPTE170M:
            case ycbcr_matrix::BT470BG:
                if (cs->range != ycbcr_range::Unspecified)
                    return *this;
            default: break;
        }
    } else if (std::get_if<header_missing>(this) || std::get_if<header_invalid>(this)) {
        return Header(header_missing{});
    }

    return Header(header_none{});
}

void Header::override_colorspace(ycbcr_matrix &CM, ycbcr_range &CR, int Width, int Height) const {
    guess_colorspace(CM, CR, Width, Height);
    Header effective = to_effective();
    if (auto *cs = std::get_if<header_colorspace>(&effective)) {
        CM = cs->matrix;
        CR = cs->range;
    }
}

void guess_colorspace(ycbcr_matrix &CM, ycbcr_range &CR, int Width, int Height) {
    if (CM == ycbcr_matrix::Unspecified)
        CM = (Width > 1024 || Height >= 600) ? ycbcr_matrix::BT709 : ycbcr_matrix::SMPTE170M;
    if (CR == ycbcr_range::Unspecified)
        CR = ycbcr_range::MPEG;
}

} // namespace agi::ycbcr
