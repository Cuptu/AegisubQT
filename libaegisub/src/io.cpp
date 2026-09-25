// Copyright (c) 2010, Amar Takhar <verm@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#include <libaegisub/io.h>

#include <fstream>
#include <thread>
#include <chrono>

namespace agi::io {

std::unique_ptr<std::istream> Open(agi::fs::path const& file, bool binary) {
    auto stream = std::make_unique<std::ifstream>(file, binary ? std::ios::binary : std::ios::in);
    if (!stream->is_open() || stream->fail()) {
        throw IOFatal("Failed opening " + file.string());
    }
    return stream;
}

Save::Save(agi::fs::path const& file, bool binary)
: file_name(file)
, tmp_name(file.parent_path() / (file.stem().string() + ".tmp" + file.extension().string())) {
    fp = std::make_unique<std::ofstream>(tmp_name, binary ? std::ios::binary : std::ios::out);
    if (!fp->good()) {
        throw fs::WriteDenied(tmp_name);
    }
}

Save::~Save() {
    fp.reset();
    for (int i = 0; i < 10; ++i) {
        try {
            fs::Rename(tmp_name, file_name);
            return;
        } catch (...) {
            if (i == 9) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

} // namespace agi::io
