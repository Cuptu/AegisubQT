// Copyright (c) 2010, Amar Takhar <verm@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#include <libaegisub/util.h>

#include <cassert>
#include <charconv>
#include <cstdlib>

namespace agi::util {

bool try_parse(std::string_view str, double *out) {
    if (str.empty()) return false;
    char *end = nullptr;
    std::string s(str);
    double val = std::strtod(s.c_str(), &end);
    if (end == s.c_str() + s.size()) {
        if (out) *out = val;
        return true;
    }
    return false;
}

bool try_parse(std::string_view str, int *out) {
    if (str.empty()) return false;
    int val = 0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
    if (ec == std::errc() && ptr == str.data() + str.size()) {
        if (out) *out = val;
        return true;
    }
    return false;
}

void sleep_for(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

std::string tagless_find_helper::strip_tags(std::string const& str, size_t s) {
    blocks.clear();
    const size_t bad_pos = static_cast<size_t>(-1);
    size_t ovr_start = bad_pos;
    size_t i = 0;
    for (char c : str) {
        if (c == '{' && ovr_start == bad_pos)
            ovr_start = i;
        else if (c == '}' && ovr_start != bad_pos) {
            blocks.emplace_back(ovr_start, i + 1);
            ovr_start = bad_pos;
        }
        ++i;
    }

    std::string out;
    size_t last = s;
    for (auto const& block : blocks) {
        if (block.second <= s) continue;
        if (block.first > last)
            out.append(str.begin() + last, str.begin() + block.first);
        last = block.second;
    }

    if (last < str.size())
        out.append(str.begin() + last, str.end());

    start = s;
    return out;
}

void tagless_find_helper::map_range(size_t &s, size_t &e) {
    s += start;
    e += start;

    for (auto const& block : blocks) {
        if (block.second <= start) continue;
        if (block.first <= s) {
            size_t len = block.second - std::max(block.first, start);
            s += len;
            e += len;
            continue;
        }

        if (block.first >= e) break;
        e += block.second - block.first;
    }
}

} // namespace agi::util
