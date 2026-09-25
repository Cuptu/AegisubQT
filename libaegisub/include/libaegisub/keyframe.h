// Copyright (c) 2011, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <libaegisub/exception.h>
#include <libaegisub/fs.h>

#include <vector>

namespace agi::keyframe {

std::vector<int> Load(agi::fs::path const& filename);
void Save(agi::fs::path const& filename, std::vector<int> const& keyframes);

DEFINE_EXCEPTION(KeyframeFormatParseError, agi::InvalidInputException);
DEFINE_EXCEPTION(UnknownKeyframeFormatError, agi::InvalidInputException);

} // namespace agi::keyframe
