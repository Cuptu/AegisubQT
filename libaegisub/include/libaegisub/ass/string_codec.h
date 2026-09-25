// Copyright (c) 2005, Niels Martin Hansen
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <string>
#include <string_view>

namespace agi::ass {
std::string inline_string_encode(std::string_view input);
std::string inline_string_decode(std::string_view input);
}
