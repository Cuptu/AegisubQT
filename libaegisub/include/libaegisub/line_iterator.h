// Copyright (c) 2010, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>

namespace agi {

class line_iterator_base {
    std::istream *stream = nullptr;

protected:
    bool getline(std::string &str) {
        if (!stream || !std::getline(*stream, str)) {
            stream = nullptr;
            return false;
        }
        if (!str.empty() && str.back() == '\r')
            str.pop_back();
        return true;
    }

public:
    line_iterator_base(std::istream &stream, const char* = "utf-8") : stream(&stream) {}
    line_iterator_base() = default;

    bool operator==(line_iterator_base const& rgt) const { return stream == rgt.stream; }
};

template<class OutputType = std::string>
class line_iterator final : public line_iterator_base {
    OutputType value;

    inline bool convert(std::string &str) {
        std::istringstream ss(str);
        ss >> value;
        return !ss.fail();
    }

    void next() {
        std::string str;
        if (!getline(str))
            return;
        if (!convert(str))
            next();
    }

public:
    using iterator_category = std::input_iterator_tag;
    using value_type = OutputType;
    using difference_type = std::ptrdiff_t;
    using pointer = OutputType*;
    using reference = OutputType&;

    line_iterator(std::istream &stream, const char *encoding = "utf-8")
    : line_iterator_base(stream, encoding) {
        ++(*this);
    }

    line_iterator() = default;

    OutputType const& operator*() const { return value; }
    OutputType const* operator->() const { return &value; }

    line_iterator<OutputType>& operator++() {
        next();
        return *this;
    }
    line_iterator<OutputType> operator++(int) {
        line_iterator<OutputType> tmp(*this);
        ++*this;
        return tmp;
    }
};

template<>
inline void line_iterator<std::string>::next() {
    value.clear();
    getline(value);
}

template<typename T>
line_iterator<T>& begin(line_iterator<T>& it) { return it; }

template<typename T>
line_iterator<T> end(line_iterator<T>&) { return line_iterator<T>(); }

} // namespace agi
