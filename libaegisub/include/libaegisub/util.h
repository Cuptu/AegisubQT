// Copyright (c) 2010, Amar Takhar <verm@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <algorithm>
#include <charconv>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace agi::util {

template<typename T>
static inline T mid(T a, T b, T c) {
    return std::clamp(b, a, c);
}

bool try_parse(std::string_view str, double *out);
bool try_parse(std::string_view str, int *out);

void sleep_for(int ms);

class tagless_find_helper {
    std::vector<std::pair<size_t, size_t>> blocks;
    size_t start = 0;

public:
    std::string strip_tags(std::string const& str, size_t start);
    void map_range(size_t& start, size_t& end);
};

} // namespace agi::util
