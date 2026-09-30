#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>
#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace Automation::FileSystem {
inline std::filesystem::path path(const char *name) {
    if (!name) throw std::invalid_argument("File path must not be null");
    return std::filesystem::u8path(name);
}

inline std::string utf8(const std::filesystem::path &name) {
    const auto bytes = name.u8string();
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

inline const char *mode(std::filesystem::file_type type) {
    using enum std::filesystem::file_type;
    switch (type) {
        case not_found: return nullptr;
        case regular: return "file";
        case directory: return "directory";
        case symlink: return "link";
        case block: return "block device";
        case character: return "char device";
        case fifo: return "fifo";
        case socket: return "socket";
        default: return "other";
    }
}

inline const char *mode(const std::filesystem::path &name) {
    // status follows links, as upstream does. Missing files have no mode;
    // access and other status failures throw and reach Lua's error result.
    const auto status = std::filesystem::status(name);
    return mode(status.type());
}

inline int64_t modification(const std::filesystem::path &name) {
    const auto time = std::filesystem::last_write_time(name);
#ifdef _MSVC_STL_VERSION
    const auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(time);
#else
    const auto systemTime = std::chrono::file_clock::to_sys(time);
#endif
    return std::chrono::floor<std::chrono::seconds>(systemTime.time_since_epoch()).count();
}

inline uint64_t size(const std::filesystem::path &name) {
    if (std::filesystem::is_directory(name))
        throw std::filesystem::filesystem_error("Cannot get directory size", name,
            std::make_error_code(std::errc::is_a_directory));
    return std::filesystem::file_size(name);
}

inline void mkdir(const std::filesystem::path &name) {
    // Upstream CreateDirectory is recursive and accepts an existing directory.
    std::filesystem::create_directories(name);
}

inline void remove(const std::filesystem::path &name) {
    if (!std::filesystem::remove(name))
        throw std::filesystem::filesystem_error("Cannot remove missing path", name,
            std::make_error_code(std::errc::no_such_file_or_directory));
}

inline void touch(const std::filesystem::path &name) {
    if (!name.parent_path().empty()) mkdir(name.parent_path());
#ifdef _WIN32
    // Opening for append creates a missing file without truncating an existing
    // file, and reports write/sharing errors before updating its timestamp.
    std::ofstream file;
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file.open(name, std::ios::binary | std::ios::app);
    file.close();
    std::filesystem::last_write_time(name, std::filesystem::file_time_type::clock::now());
#else
    const int fd = ::open(name.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);
    if (fd < 0)
        throw std::filesystem::filesystem_error("Cannot touch file", name,
            std::error_code(errno, std::generic_category()));
    const int result = ::futimes(fd, nullptr);
    const int savedError = errno;
    ::close(fd);
    if (result < 0)
        throw std::filesystem::filesystem_error("Cannot update file time", name,
            std::error_code(savedError, std::generic_category()));
#endif
}
}
