// Copyright (c) 2013, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#include <libaegisub/fs.h>

#include <algorithm>
#include <cctype>

namespace agi::fs {

bool Exists(path const& p) {
    std::error_code ec;
    return std::filesystem::exists(p, ec);
}

bool FileExists(path const& file) {
    std::error_code ec;
    return std::filesystem::is_regular_file(file, ec);
}

bool DirectoryExists(path const& dir) {
    std::error_code ec;
    return std::filesystem::is_directory(dir, ec);
}

uintmax_t Size(path const& file_path) {
    std::error_code ec;
    auto s = std::filesystem::file_size(file_path, ec);
    if (ec) throw FileNotFound(file_path);
    return s;
}

bool CreateDirectory(path const& dir_path) {
    std::error_code ec;
    return std::filesystem::create_directories(dir_path, ec);
}

void Rename(path const& from, path const& to) {
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (ec) throw FileSystemUnknownError("Failed to rename file: " + from.string() + " to " + to.string());
}

void Copy(path const& from, path const& to) {
    std::error_code ec;
    std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) throw FileSystemUnknownError("Failed to copy file: " + from.string() + " to " + to.string());
}

bool Remove(path const& file) {
    std::error_code ec;
    return std::filesystem::remove(file, ec);
}

bool HasExtension(path const& p, std::string const& ext) {
    auto p_ext = p.extension().string();
    if (!p_ext.empty() && p_ext[0] == '.')
        p_ext.erase(0, 1);
    if (p_ext.size() != ext.size()) return false;
    for (size_t i = 0; i < ext.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(p_ext[i])) != std::tolower(static_cast<unsigned char>(ext[i])))
            return false;
    }
    return true;
}

} // namespace agi::fs
