// Copyright (c) 2026, arch1t3cht <arch1t3cht@gmail.com>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace agi {

enum class ycbcr_matrix : char {
    RGB = 0,
    BT709 = 1,
    Unspecified = 2,
    FCC = 4,
    BT470BG = 5,
    SMPTE170M = 6,
    SMPTE240M = 7,
    YCoCg = 8,
    BT2020_NCL = 9,
    BT2020_CL = 10,
    SMPTE2085 = 11,
    ChromaticityDerivedNCL = 12,
    ChromaticityDerivedCL = 13,
    ICtCp = 14,
};

enum class ycbcr_range : char {
    Unspecified = 0,
    MPEG = 1, // TV / Limited
    JPEG = 2, // PC / Full
};

namespace ycbcr {

const char *matrix_to_string(ycbcr_matrix matrix);
const char *range_to_string(ycbcr_range range);

inline constexpr std::string_view valid_header_strings[] = {
    "None",
    "TV.601", "PC.601",
    "TV.709", "PC.709",
    "TV.FCC", "PC.FCC",
    "TV.240M", "PC.240M",
};

struct header_missing { bool operator==(const header_missing&) const = default; };
struct header_invalid { bool operator==(const header_invalid&) const = default; };
struct header_none { bool operator==(const header_none&) const = default; };
struct header_colorspace {
    ycbcr_matrix matrix;
    ycbcr_range range;

    static header_colorspace unspecified() { return {ycbcr_matrix::Unspecified, ycbcr_range::Unspecified}; }
    bool operator==(const header_colorspace&) const = default;
};

using header_variant = std::variant<header_missing, header_invalid, header_none, header_colorspace>;

struct Header : header_variant {
    Header(header_variant v);
    Header(ycbcr_matrix cm, ycbcr_range cr) : Header(header_colorspace{cm, cr}) {}
    explicit Header(std::string const& matrix);

    bool valid() const;
    Header to_effective() const;
    Header to_existing() const;
    Header to_best_practice() const;
    std::optional<std::string> to_string() const;
    std::string to_existing_string() const { return to_existing().to_string().value(); }
    std::string to_best_practice_string() const { return to_best_practice().to_string().value(); }
    void override_colorspace(ycbcr_matrix &CM, ycbcr_range &CR, int Width, int Height) const;
};

void guess_colorspace(ycbcr_matrix &CM, ycbcr_range &CR, int Width, int Height);

} // namespace ycbcr
} // namespace agi
