// Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>

namespace agi {

namespace detail {
    template<typename T>
    inline decltype(auto) format_arg(T&& val) {
        using Decayed = std::decay_t<T>;
        if constexpr (std::is_same_v<Decayed, std::string>) {
            return val.c_str();
        } else {
            return std::forward<T>(val);
        }
    }
}

template<typename... Args>
inline std::string format(const char *fmt, Args&&... args) {
    int size = std::snprintf(nullptr, 0, fmt, detail::format_arg(std::forward<Args>(args))...);
    if (size <= 0) return {};
    std::string str(static_cast<size_t>(size), '\0');
    std::snprintf(str.data(), static_cast<size_t>(size) + 1, fmt, detail::format_arg(std::forward<Args>(args))...);
    return str;
}

} // namespace agi
