// Copyright (c) 2011, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#include <libaegisub/keyframe.h>

#include <libaegisub/io.h>
#include <libaegisub/line_iterator.h>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {

std::vector<int> agi_keyframes(std::istream &file) {
    double fps = 0.0;
    std::string fps_str;
    file >> fps_str;
    file >> fps;

    return {agi::line_iterator<int>(file), agi::line_iterator<int>()};
}

std::vector<int> enumerated_keyframes(std::istream &file, char (*func)(std::string const&)) {
    int count = 0;
    std::vector<int> ret;
    for (auto const& line : agi::line_iterator<std::string>(file)) {
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(func(line))));
        if (c == 'i')
            ret.push_back(count++);
        else if (c == 'p' || c == 'b')
            ++count;
    }
    return ret;
}

std::vector<int> indexed_keyframes(std::istream &file, int (*func)(std::string const&)) {
    std::vector<int> ret;
    for (auto const& line : agi::line_iterator<std::string>(file)) {
        int frame_no = func(line);
        if (frame_no >= 0)
            ret.push_back(frame_no);
    }
    return ret;
}

char xvid(std::string const& line) {
    return line.empty() ? 0 : line[0];
}

char divx(std::string const& line) {
    char chrs[] = "IPB";
    for (int i = 0; i < 3; ++i) {
        auto pos = line.find(chrs[i]);
        if (pos != std::string::npos)
            return line[pos];
    }
    return 0;
}

char x264(std::string const& line) {
    auto pos = line.find("type:");
    if (pos == std::string::npos || pos + 5 >= line.size()) return 0;
    return line[pos + 5];
}

int wwxd(std::string const& line) {
    if (line.empty() || line[0] == '#')
        return -1;
    std::istringstream ss(line);
    int frame_no;
    char frame_type;
    ss >> frame_no >> frame_type;
    if (ss.fail())
        throw agi::keyframe::KeyframeFormatParseError("WWXD keyframe file not in qpfile format");
    if (frame_type == 'I')
        return frame_no;
    return -1;
}

} // anonymous namespace

namespace agi::keyframe {

void Save(agi::fs::path const& filename, std::vector<int> const& keyframes) {
    io::Save file(filename);
    std::ostream& of = file.Get();
    of << "# keyframe format v1\n";
    of << "fps 0\n";
    for (int kf : keyframes) {
        of << kf << "\n";
    }
}

std::vector<int> Load(agi::fs::path const& filename) {
    auto file = io::Open(filename);
    std::istream &is(*file);

    std::string header;
    std::getline(is, header);
    if (!header.empty() && header.back() == '\r')
        header.pop_back();

    if (header == "# keyframe format v1") return agi_keyframes(is);
    if (header.starts_with("# XviD 2pass stat file")) return enumerated_keyframes(is, xvid);
    if (header.starts_with("# ffmpeg 2-pass log file, using xvid codec")) return enumerated_keyframes(is, xvid);
    if (header.starts_with("# avconv 2-pass log file, using xvid codec")) return enumerated_keyframes(is, xvid);
    if (header.starts_with("##map version")) return enumerated_keyframes(is, divx);
    if (header.starts_with("#options:")) return enumerated_keyframes(is, x264);
    if (header.starts_with("# WWXD log file, using qpfile format")) return indexed_keyframes(is, wwxd);

    throw UnknownKeyframeFormatError("File header does not match any known formats");
}

} // namespace agi::keyframe
