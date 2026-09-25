// Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <array>
#include <cstdint>

#include <libaegisub/color.h>
#include <libaegisub/ycbcr.h>

namespace agi {

class ycbcr_converter {
    std::array<double, 9> from_ycbcr{};
    std::array<double, 9> to_ycbcr{};

    std::array<double, 3> shift_from{};
    std::array<double, 3> shift_to{};

    void init_dst(ycbcr::header_colorspace dst);
    void init_src(ycbcr::header_colorspace src);

    template<typename T>
    static std::array<double, 3> prod(std::array<double, 9> m, std::array<T, 3> v) {
        return {{
            m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
            m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
            m[6] * v[0] + m[7] * v[1] + m[8] * v[2],
        }};
    }

    template<typename T, typename U>
    static std::array<double, 3> add(std::array<T, 3> left, std::array<U, 3> right) {
        return {{left[0] + right[0], left[1] + right[1], left[2] + right[2]}};
    }

    static uint8_t clamp(double v) {
        auto i = static_cast<int>(v);
        i = i > 255 ? 255 : i;
        return i < 0 ? 0 : static_cast<uint8_t>(i);
    }

    static std::array<uint8_t, 3> to_uint8_t(std::array<double, 3> val) {
        return {{clamp(val[0] + .5), clamp(val[1] + .5), clamp(val[2] + .5)}};
    }

public:
    ycbcr_converter(ycbcr::header_colorspace srcdst);
    ycbcr_converter(ycbcr::header_colorspace src, ycbcr::header_colorspace dst);

    std::array<uint8_t, 3> rgb_to_ycbcr(std::array<uint8_t, 3> input) const {
        return to_uint8_t(add(prod(to_ycbcr, input), shift_to));
    }

    std::array<uint8_t, 3> ycbcr_to_rgb(std::array<uint8_t, 3> input) const {
        return to_uint8_t(prod(from_ycbcr, add(input, shift_from)));
    }

    std::array<uint8_t, 3> rgb_to_rgb(std::array<uint8_t, 3> input) const {
        return to_uint8_t(prod(from_ycbcr,
            add(add(prod(to_ycbcr, input), shift_to), shift_from)));
    }

    Color rgb_to_rgb(Color c) const {
        auto arr = rgb_to_rgb(std::array<uint8_t, 3>{{c.r, c.g, c.b}});
        return Color{arr[0], arr[1], arr[2], c.a};
    }
};

} // namespace agi
