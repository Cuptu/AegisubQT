// Copyright (c) 2010, Amar Takhar <verm@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <libaegisub/exception.h>
#include <libaegisub/fs.h>

#include <iosfwd>
#include <memory>

namespace agi::io {

DEFINE_EXCEPTION(IOError, Exception);
DEFINE_EXCEPTION(IOFatal, IOError);

std::unique_ptr<std::istream> Open(agi::fs::path const& file, bool binary = false);

class Save {
    std::unique_ptr<std::ostream> fp;
    const agi::fs::path file_name;
    const agi::fs::path tmp_name;

public:
    Save(agi::fs::path const& file, bool binary = false);
    ~Save();
    std::ostream& Get() { return *fp; }
};

} // namespace agi::io
