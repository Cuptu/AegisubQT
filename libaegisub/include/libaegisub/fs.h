// Copyright (c) 2013, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <libaegisub/exception.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#undef CreateDirectory

namespace agi::fs {

class path : public std::filesystem::path {
public:
    path(std::string_view string) : std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t *>(string.data()), string.size())) {}
    path(std::string const& string) : std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t *>(string.data()), string.size())) {}
    path(const char *c_str) : std::filesystem::path(reinterpret_cast<const char8_t *>(c_str)) {}
    path() : std::filesystem::path() {}

    explicit path(std::filesystem::path const& inner) : std::filesystem::path(inner) {}
    explicit path(std::filesystem::path &&inner) : std::filesystem::path(std::move(inner)) {}

    inline std::string string() const {
        const auto result = std::filesystem::path::u8string();
        return std::string(reinterpret_cast<const char *>(result.c_str()), result.size());
    }

    inline std::string generic_string() const {
        const auto result = std::filesystem::path::generic_u8string();
        return std::string(reinterpret_cast<const char *>(result.c_str()), result.size());
    }

    inline friend path operator/(path const& lhs, path const& rhs) {
        const std::filesystem::path &lhs_ = lhs;
        const std::filesystem::path &rhs_ = rhs;
        return path(lhs_ / rhs_);
    }

#define WRAP_SFP(name) \
    inline path name() const { \
        return path(std::filesystem::path::name()); \
    }

    WRAP_SFP(root_name);
    WRAP_SFP(root_directory);
    WRAP_SFP(root_path);
    WRAP_SFP(relative_path);
    WRAP_SFP(parent_path);
    WRAP_SFP(filename);
    WRAP_SFP(stem);
    WRAP_SFP(extension);
#undef WRAP_SFP

    inline path& make_preferred() {
        std::filesystem::path::make_preferred();
        return *this;
    }
};

#define DEFINE_FS_EXCEPTION(type, base, message) \
    struct type : public base { \
        type(path const& p) : base(message + p.string()) { } \
        type(std::string const& s) : base(s) { } \
    }

DEFINE_EXCEPTION(FileSystemError, Exception);
DEFINE_FS_EXCEPTION(FileNotAccessible, FileSystemError, "File is not accessible: ");
DEFINE_FS_EXCEPTION(FileNotFound, FileNotAccessible, "File not found: ");
DEFINE_EXCEPTION(FileSystemUnknownError, FileSystemError);
DEFINE_FS_EXCEPTION(NotAFile, FileNotAccessible, "Path is not a file (and should be): ");
DEFINE_FS_EXCEPTION(NotADirectory, FileNotAccessible, "Path is not a directory (and should be): ");
DEFINE_FS_EXCEPTION(PathTooLong, FileSystemError, "Path is too long: ");
DEFINE_FS_EXCEPTION(DriveFull, FileSystemError, "Insufficient free space to write file: ");
DEFINE_FS_EXCEPTION(AccessDenied, FileNotAccessible, "Access denied to path: ");
DEFINE_FS_EXCEPTION(ReadDenied, AccessDenied, "Access denied when trying to read: ");
DEFINE_FS_EXCEPTION(WriteDenied, AccessDenied, "Access denied when trying to write: ");
DEFINE_FS_EXCEPTION(ReadOnlyFile, WriteDenied, "File is read-only: ");

#undef DEFINE_FS_EXCEPTION

bool Exists(path const& p);
bool FileExists(path const& file);
bool DirectoryExists(path const& dir);
uintmax_t Size(path const& file_path);
bool CreateDirectory(path const& dir_path);
void Rename(path const& from, path const& to);
void Copy(path const& from, path const& to);
bool Remove(path const& file);
bool HasExtension(path const& p, std::string const& ext);

} // namespace agi::fs
