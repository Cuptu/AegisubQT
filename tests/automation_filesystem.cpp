#include "AutomationFileSystem.h"
#include <iostream>
#include <cstring>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#endif

namespace fs = std::filesystem;
namespace api = Automation::FileSystem;
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Function> void require_error(Function function, const char *message) {
    bool failed = false;
    try { function(); } catch (const std::exception &error) { failed = error.what()[0] != '\0'; }
    require(failed, message);
}

struct Fixture {
    fs::path original = fs::current_path();
    fs::path root;
    Fixture() {
        const auto parent = fs::canonical(fs::temp_directory_path());
        root = parent / ("aegisub-lfs-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(root.is_absolute() && root.parent_path() == parent, "Temporary fixture escaped its parent");
        require(fs::create_directory(root), "Temporary fixture already exists");
    }
    ~Fixture() {
        std::error_code error;
        fs::current_path(original, error);
        // Only this newly created, checked temporary child is removed.
        fs::remove_all(root, error);
    }
};

int main() {
    try {
        Fixture fixture;
        const auto missing = fixture.root / "missing";
        require(api::mode(missing) == nullptr, "A missing file must have no mode");
        require_error([&] { (void)api::size(missing); }, "Missing size reported as success");
        require_error([&] { api::modification(missing); }, "Missing time reported as success");
        require_error([&] { api::remove(missing); }, "Removing a missing file reported success");

        const auto nested = fixture.root / "parent" / "child";
        api::mkdir(nested);
        api::mkdir(nested);
        require(std::strcmp(api::mode(nested), "directory") == 0, "Recursive/idempotent mkdir failed");
        const auto name = api::path(api::utf8(nested).append("/字幕😀.txt").c_str());
        api::touch(name);
        require(fs::file_size(name) == 0, "Touch did not create an empty file");
        require(std::strcmp(api::mode(name), "file") == 0, "Regular file mode failed");
        require(api::path(api::utf8(name).c_str()) == name, "UTF-8 path round trip failed");
        { std::ofstream stream(name, std::ios::binary); stream << "keep existing contents"; }
        const auto size = fs::file_size(name);
        fs::last_write_time(name, fs::file_time_type::clock::now() - std::chrono::hours(24));
        api::touch(name);
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        require(api::modification(name) >= now - 2 && api::modification(name) <= now + 2, "Modification is not a Unix timestamp");
        require(fs::file_size(name) == size, "Touch truncated an existing file");
        require_error([&] { api::mkdir(name / "child"); }, "mkdir accepted a file as parent");
        require_error([&] { api::touch(name / "child"); }, "touch accepted a file as parent");
        require_error([&] { (void)api::size(nested); }, "Directory size silently succeeded");
        require_error([&] { api::remove(nested); }, "Removing a nonempty directory succeeded");
        const auto child = fixture.root / "created" / "by-touch" / "file";
        api::touch(child);
        require(fs::exists(child), "Touch did not create missing parents");

        const std::pair<fs::file_type, const char *> types[] = {
            {fs::file_type::regular, "file"}, {fs::file_type::directory, "directory"},
            {fs::file_type::symlink, "link"}, {fs::file_type::block, "block device"},
            {fs::file_type::character, "char device"}, {fs::file_type::fifo, "fifo"},
            {fs::file_type::socket, "socket"}, {fs::file_type::unknown, "other"}
        };
        for (const auto &[type, expected] : types)
            require(std::strcmp(api::mode(type), expected) == 0, "File type classification differs from upstream");
#ifdef _WIN32
        require(SetFileAttributesW(name.c_str(), FILE_ATTRIBUTE_READONLY), "Cannot set test read-only attribute");
        bool denied = false;
        try { api::touch(name); } catch (const std::exception &) { denied = true; }
        const bool restored = SetFileAttributesW(name.c_str(), FILE_ATTRIBUTE_NORMAL);
        require(restored && denied, "Touch did not report read-only write failure");
        std::cout << "PASS Windows read-only write error\n";
#else
        const auto pipe = fixture.root / "pipe";
        require(::mkfifo(pipe.c_str(), 0600) == 0, "Cannot create FIFO fixture");
        require(std::strcmp(api::mode(pipe), "fifo") == 0, "FIFO mode failed");
        require(std::strcmp(api::mode("/dev/null"), "char device") == 0, "Character device mode failed");
        const auto socketPath = fixture.root / "socket";
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        // Bind relative to the owned fixture: macOS TMPDIR may be longer than
        // sockaddr_un::sun_path even though ordinary filesystem paths work.
        std::strcpy(address.sun_path, "socket");
        const int socketFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        require(socketFd >= 0, "Cannot create socket fixture");
        fs::current_path(fixture.root);
        const int bound = ::bind(socketFd, reinterpret_cast<sockaddr *>(&address), sizeof(address));
        ::close(socketFd);
        fs::current_path(fixture.original);
        require(bound == 0, "Cannot bind socket fixture");
        require(std::strcmp(api::mode(socketPath), "socket") == 0, "Socket mode failed");
        const auto link = fixture.root / "link";
        fs::create_symlink(name, link);
        require(std::strcmp(api::mode(link), "file") == 0, "Status must follow symlinks as upstream does");
        if (::geteuid() != 0) {
            const auto inaccessible = fixture.root / "inaccessible";
            api::touch(inaccessible / "file");
            fs::permissions(inaccessible, fs::perms::none);
            bool denied = false;
            try { api::mode(inaccessible / "file"); } catch (const std::exception &) { denied = true; }
            fs::permissions(inaccessible, fs::perms::owner_all);
            require(denied, "Status swallowed a permission error");
        } else std::cout << "SKIP permission-denied status under root\n";
        std::cout << "PASS real POSIX FIFO/socket/character device/symlink fixtures\n";
#endif
        api::remove(name);
        api::remove(nested);
        std::cout << "PASS filesystem failures, UTF-8 paths, size/time, touch preservation, mkdir/remove and all type mappings\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
