// Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
// Copyright (c) 2026, Aegisub Project & Contributors
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include "LuaModules.h"
#include <boost/regex/icu.hpp>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace Automation {
namespace {
using boost::u32regex;
struct agi_re_match {
    boost::cmatch match;
    int range[2];
};
struct agi_re_flag { const char *name; int value; };

// Native exceptions cannot cross LuaJIT FFI frames. Preserve the upstream
// out-parameter error contract for invalid patterns, UTF-8 and engine errors.
template<class Function>
auto regex_call(char **err, Function function) -> decltype(function()) {
    try { return function(); }
    catch (const std::exception &exception) { *err = strdup_malloc(exception.what()); }
    catch (...) { *err = strdup_malloc("Unknown regex error"); }
    return {};
}

bool search(u32regex &re, const char *str, size_t len, size_t start, boost::cmatch &result) {
    if (start > len) throw std::out_of_range("Regex start is outside the input");
    return boost::u32regex_search(str + start, str + len, result, re,
        start ? boost::match_prev_avail | boost::match_not_bob : boost::match_default);
}

agi_re_match *regex_match(u32regex *re, const char *str, size_t len, int start, char **err) {
    return regex_call(err, [&]() -> agi_re_match * {
        if (start < 0) throw std::out_of_range("Regex start must not be negative");
        auto result = std::make_unique<agi_re_match>();
        if (!search(*re, str, len, static_cast<size_t>(start), result->match)) return nullptr;
        return result.release();
    });
}

int *regex_get_match(agi_re_match *result, size_t index) {
    if (index >= result->match.size() || !result->match[index].matched) return nullptr;
    result->range[0] = static_cast<int>(std::distance(result->match.prefix().first, result->match[index].first)) + 1;
    result->range[1] = static_cast<int>(std::distance(result->match.prefix().first, result->match[index].second));
    return result->range;
}

int *regex_search(u32regex *re, const char *str, size_t len, size_t start, char **err) {
    return regex_call(err, [&]() -> int * {
        boost::cmatch match;
        if (!search(*re, str, len, start, match)) return nullptr;
        auto result = static_cast<int *>(malloc(2 * sizeof(int)));
        if (!result) return nullptr;
        result[0] = static_cast<int>(start + match.position()) + 1;
        result[1] = static_cast<int>(start + match.position() + match.length());
        return result;
    });
}

char *regex_replace(u32regex *re, const char *replacement, const char *str, size_t len, int max_count, char **err) {
    return regex_call(err, [&]() -> char * {
        auto match = boost::u32regex_iterator<const char *>(str, str + len, *re);
        const auto end = boost::u32regex_iterator<const char *>();
        const char *suffix = str;
        std::string result;
        auto output = std::back_inserter(result);
        for (; match != end && max_count > 0; ++match, --max_count) {
            std::copy(suffix, match->prefix().second, output);
            match->format(output, replacement);
            suffix = match->suffix().first;
        }
        result.append(suffix, str + len);
        return strndup_malloc(result.data(), result.size());
    });
}

u32regex *regex_compile(const char *pattern, int flags, char **err) {
    return regex_call(err, [&]() -> u32regex * {
        return new u32regex(boost::make_u32regex(pattern, boost::u32regex::perl | flags));
    });
}
void regex_free(u32regex *re) { delete re; }
void match_free(agi_re_match *match) { delete match; }
const agi_re_flag *get_regex_flags() {
    static const agi_re_flag flags[] = {
        {"ICASE", u32regex::icase}, {"NOSUB", u32regex::nosubs},
        {"COLLATE", u32regex::collate}, {"NEWLINE_ALT", u32regex::newline_alt},
        {"NO_MOD_M", u32regex::no_mod_m}, {"NO_MOD_S", u32regex::no_mod_s},
        {"MOD_S", u32regex::mod_s}, {"MOD_X", u32regex::mod_x},
        {"NO_EMPTY_SUBEXPRESSIONS", u32regex::no_empty_expressions}, {nullptr, 0}
    };
    return flags;
}
}

int luaopen_re_impl(lua_State *L) {
    do_register_lib_table(L, {"agi_re_match", "u32regex"});
    lua_createtable(L, 0, 8);
    do_register_lib_function(L, "search", "int * (*)(u32regex *, const char *, size_t, size_t, char **)", (void *)regex_search);
    do_register_lib_function(L, "match", "agi_re_match * (*)(u32regex *, const char *, size_t, int, char **)", (void *)regex_match);
    do_register_lib_function(L, "get_match", "int * (*)(agi_re_match *, size_t)", (void *)regex_get_match);
    do_register_lib_function(L, "replace", "char * (*)(u32regex *, const char *, const char *, size_t, int, char **)", (void *)regex_replace);
    do_register_lib_function(L, "compile", "u32regex * (*)(const char *, int, char **)", (void *)regex_compile);
    do_register_lib_function(L, "get_flags", "const agi_re_flag * (*)()", (void *)get_regex_flags);
    do_register_lib_function(L, "match_free", "void (*)(agi_re_match *)", (void *)match_free);
    do_register_lib_function(L, "regex_free", "void (*)(u32regex *)", (void *)regex_free);
    lua_remove(L, -2);
    return 1;
}
}
