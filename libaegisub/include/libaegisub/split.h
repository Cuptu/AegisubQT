// Copyright (c) 2015, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <locale>
#include <string>
#include <string_view>
#include <vector>

namespace agi {

template<typename Char>
class split_iterator {
    bool is_end = true;
    std::basic_string_view<Char> str;
    size_t pos = 0;
    Char delim = 0;

public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::basic_string_view<Char>;
    using pointer = value_type*;
    using reference = value_type&;
    using difference_type = ptrdiff_t;

    split_iterator(std::basic_string_view<Char> str, Char c)
    : is_end(str.empty()), str(str), delim(c) {
        pos = str.find(delim);
    }

    split_iterator() = default;

    bool eof() const { return is_end; }

    std::basic_string_view<Char> operator*() const {
        return str.substr(0, pos);
    }

    bool operator==(split_iterator const& it) const {
        if (is_end || it.is_end)
            return is_end && it.is_end;
        return str == it.str && (str.empty() || delim == it.delim);
    }

    split_iterator& operator++() {
        if (pos == str.npos) {
            str = str.substr(str.size());
            is_end = true;
        } else {
            str = str.substr(pos + 1);
            pos = str.find(delim);
        }
        return *this;
    }

    split_iterator operator++(int) {
        split_iterator tmp = *this;
        ++*this;
        return tmp;
    }
};

template<typename Char>
split_iterator<Char> begin(split_iterator<Char> const& it) { return it; }

template<typename Char>
split_iterator<Char> end(split_iterator<Char> const&) { return split_iterator<Char>(); }

template<typename Char>
split_iterator<Char> Split(std::basic_string_view<Char> str, Char delim) {
    return split_iterator<Char>(str, delim);
}

inline split_iterator<char> Split(std::basic_string_view<char> str, char delim) {
    return split_iterator<char>(str, delim);
}

template<typename Cont, typename Char>
void Split(Cont& out, std::basic_string_view<Char> str, Char delim) {
    out.clear();
    for (auto const& tok : Split(str, delim))
        out.emplace_back(tok.begin(), tok.end());
}

template<typename Cont>
void Split(Cont& out, std::basic_string_view<char> str, char delim) {
    Split<Cont, char>(out, str, delim);
}

inline std::string_view Trim(std::string_view str) {
    while (!str.empty() && (str.front() == ' ' || str.front() == '\t' || str.front() == '\r' || str.front() == '\n'))
        str.remove_prefix(1);
    while (!str.empty() && (str.back() == ' ' || str.back() == '\t' || str.back() == '\r' || str.back() == '\n'))
        str.remove_suffix(1);
    return str;
}

} // namespace agi
