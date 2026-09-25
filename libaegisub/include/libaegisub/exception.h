// Copyright (c) 2009, Niels Martin Hansen
// All rights reserved.
//
// Modernized C++20 implementation for AegisubQtQuick
//
#pragma once

#include <exception>
#include <string>
#include <string_view>

namespace agi {

class Exception : public std::exception {
    std::string message;

protected:
    Exception(std::string msg) : message(std::move(msg)) { }

public:
    const char* what() const noexcept override { return message.c_str(); }
    std::string const& GetMessage() const { return message; }
};

#define AG_WHERE " (at " __FILE__ ":" #__LINE__ ")"

#define DEFINE_EXCEPTION(classname, baseclass)                 \
class classname : public baseclass {                           \
public:                                                        \
    classname(std::string msg) : baseclass(std::move(msg)) { } \
}

DEFINE_EXCEPTION(UserCancelException, Exception);
DEFINE_EXCEPTION(InternalError, Exception);
DEFINE_EXCEPTION(EnvironmentError, Exception);
DEFINE_EXCEPTION(InvalidInputException, Exception);

} // namespace agi
