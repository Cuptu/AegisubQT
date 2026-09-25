// Copyright (c) 2005, Niels Martin Hansen
// Modernized C++20 implementation for AegisubQtQuick
#include <libaegisub/ass/string_codec.h>

#include <libaegisub/format.h>

#include <cstdlib>

namespace agi::ass {

std::string inline_string_encode(std::string_view input) {
    std::string output;
    output.reserve(input.size());
    for (unsigned char c : input) {
        if (c <= 0x1F || c == 0x23 || c == 0x2C || c == 0x3A || c == 0x7C)
            output += agi::format("#%02X", c);
        else
            output += static_cast<char>(c);
    }
    return output;
}

std::string inline_string_decode(std::string_view input) {
    std::string output;
    output.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] != '#' || i + 2 >= input.size()) {
            output += input[i];
        } else {
            char buff[] = {input[i + 1], input[i + 2], 0};
            output += static_cast<char>(std::strtol(buff, nullptr, 16));
            i += 2;
        }
    }
    return output;
}

} // namespace agi::ass
