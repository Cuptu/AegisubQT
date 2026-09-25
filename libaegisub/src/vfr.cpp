// Copyright (c) 2010, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#include <libaegisub/vfr.h>

#include <libaegisub/io.h>
#include <libaegisub/line_iterator.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <sstream>

namespace {
static const int64_t default_denominator = 1000000000;
using agi::line_iterator;
using namespace agi::vfr;

void validate_timecodes(std::vector<int> const& timecodes) {
    if (timecodes.size() <= 1)
        throw InvalidFramerate("Must have at least two timecodes to do anything useful");
    if (!std::is_sorted(timecodes.begin(), timecodes.end()))
        throw InvalidFramerate("Timecodes are out of order");
    if (timecodes.front() == timecodes.back())
        throw InvalidFramerate("Timecodes are all identical");
}

void normalize_timecodes(std::vector<int> &timecodes) {
    if (int front = timecodes.front()) {
        for (int &tc : timecodes)
            tc -= front;
    }
}

struct TimecodeRange {
    int start = 0;
    int end = 0;
    double fps = 0.0;
    auto operator<=>(TimecodeRange const& cmp) const = default;
};

TimecodeRange v1_parse_line(std::string const& str) {
    if (str.empty() || str[0] == '#') return {};

    std::istringstream ss(str);
    TimecodeRange range;
    char comma1 = 0, comma2 = 0;
    ss >> range.start >> comma1 >> range.end >> comma2 >> range.fps;
    if (ss.fail() || comma1 != ',' || comma2 != ',' || !ss.eof())
        throw MalformedLine(str);
    if (range.start < 0 || range.end < 0)
        throw InvalidFramerate("Cannot specify frame rate for negative frames.");
    if (range.end < range.start)
        throw InvalidFramerate("End frame must be greater than or equal to start frame");
    if (range.fps <= 0.)
        throw InvalidFramerate("FPS must be greater than zero");
    if (range.fps > 1000.)
        throw InvalidFramerate("FPS must be at most 1000");
    return range;
}

int64_t v1_parse(line_iterator<std::string> file, std::string line, std::vector<int> &timecodes, int64_t &last) {
    double fps = std::atof(line.substr(7).c_str());
    if (fps <= 0.) throw InvalidFramerate("Assumed FPS must be greater than zero");
    if (fps > 1000.) throw InvalidFramerate("Assumed FPS must not be greater than 1000");

    std::vector<TimecodeRange> ranges;
    for (auto const& l : file) {
        auto range = v1_parse_line(l);
        if (range.fps != 0)
            ranges.push_back(range);
    }

    std::sort(ranges.begin(), ranges.end());

    if (!ranges.empty())
        timecodes.reserve(ranges.back().end + 2);
    double time = 0.;
    int frame = 0;
    for (auto const& range : ranges) {
        if (frame > range.start) {
            throw InvalidFramerate("Override ranges must not overlap");
        }
        for (; frame < range.start; ++frame) {
            timecodes.push_back(static_cast<int>(time + .5));
            time += 1000. / fps;
        }
        for (; frame <= range.end; ++frame) {
            timecodes.push_back(static_cast<int>(time + .5));
            time += 1000. / range.fps;
        }
    }
    timecodes.push_back(static_cast<int>(time + .5));
    last = static_cast<int64_t>(time * fps * default_denominator);
    return static_cast<int64_t>(fps * default_denominator);
}
} // anonymous namespace

namespace agi::vfr {

Framerate::Framerate(double fps)
: denominator(default_denominator)
, numerator(static_cast<int64_t>(fps * default_denominator)) {
    if (fps < 0.) throw InvalidFramerate("FPS must be greater than zero");
    if (fps > 1000.) throw InvalidFramerate("FPS must not be greater than 1000");
    timecodes.push_back(0);
}

Framerate::Framerate(int64_t numerator, int64_t denominator, bool drop)
: denominator(denominator)
, numerator(numerator)
, drop(drop && denominator != 0 && numerator % denominator != 0) {
    if (numerator <= 0 || denominator <= 0)
        throw InvalidFramerate("Numerator and denominator must both be greater than zero");
    if (numerator / denominator > 1000) throw InvalidFramerate("FPS must not be greater than 1000");
    timecodes.push_back(0);
}

void Framerate::SetFromTimecodes() {
    validate_timecodes(timecodes);
    normalize_timecodes(timecodes);
    denominator = default_denominator;
    numerator = static_cast<int64_t>(timecodes.size() - 1) * denominator * 1000 / timecodes.back();
    last = static_cast<int64_t>(timecodes.size() - 1) * denominator * 1000;
}

Framerate::Framerate(std::vector<int> timecodes)
: timecodes(std::move(timecodes)) {
    SetFromTimecodes();
}

Framerate::Framerate(std::initializer_list<int> timecodes)
: timecodes(timecodes) {
    SetFromTimecodes();
}

Framerate::Framerate(agi::fs::path const& filename)
: denominator(default_denominator) {
    auto file = agi::io::Open(filename);
    auto it = line_iterator<std::string>(*file);
    if (it == line_iterator<std::string>())
        throw UnknownFormat("Empty file");

    std::string line = *it;
    ++it;

    if (line == "# timecode format v2") {
        for (auto const& l : it) {
            if (!l.empty() && l[0] != '#') {
                timecodes.push_back(std::atoi(l.c_str()));
            }
        }
        SetFromTimecodes();
        return;
    }

    if (line == "# timecode format v1" || line.substr(0, 7) == "Assume ") {
        if (line[0] == '#') {
            if (it == line_iterator<std::string>())
                throw UnknownFormat("Premature EOF");
            line = *it;
            ++it;
        }
        numerator = v1_parse(it, line, timecodes, last);
        return;
    }

    throw UnknownFormat(line);
}

void Framerate::Save(agi::fs::path const& filename, int length) const {
    agi::io::Save file(filename);
    auto &out = file.Get();

    out << "# timecode format v2\n";
    for (int tc : timecodes)
        out << tc << "\n";
    for (int written = static_cast<int>(timecodes.size()); written < length; ++written)
        out << TimeAtFrame(written) << "\n";
}

int Framerate::FrameAtTime(int ms, Time type) const {
    if (type == START)
        return FrameAtTime(ms - 1) + 1;
    if (type == END)
        return FrameAtTime(ms - 1);

    if (ms < 0)
        return static_cast<int>((ms * numerator / denominator - 999) / 1000);

    if (ms > timecodes.back())
        return static_cast<int>(((ms + 1) * numerator - last - numerator / 2 + (1000 * denominator - 1)) / (1000 * denominator) + timecodes.size() - 2);

    return static_cast<int>(std::distance(std::lower_bound(timecodes.rbegin(), timecodes.rend(), ms, std::greater<int>()), timecodes.rend())) - 1;
}

int Framerate::TimeAtFrame(int frame, Time type) const {
    if (type == START) {
        int prev = TimeAtFrame(frame - 1);
        int cur = TimeAtFrame(frame);
        return prev + (cur - prev + 1) / 2;
    }

    if (type == END) {
        int cur = TimeAtFrame(frame);
        int next = TimeAtFrame(frame + 1);
        return cur + (next - cur + 1) / 2;
    }

    if (numerator == 0)
        return 0;

    if (frame < 0)
        return static_cast<int>(frame * denominator * 1000 / numerator);

    if (frame >= static_cast<int>(timecodes.size())) {
        int64_t frames_past_end = frame - static_cast<int>(timecodes.size()) + 1;
        return static_cast<int>((frames_past_end * 1000 * denominator + last + numerator / 2) / numerator);
    }

    return timecodes[frame];
}

void Framerate::SmpteAtFrame(int frame, int *h, int *m, int *s, int *f) const {
    frame = std::max(frame, 0);
    int ifps = static_cast<int>(std::ceil(FPS()));

    if (drop && denominator == 1001 && numerator % 30000 == 0) {
        const int drop_factor = static_cast<int>(numerator / 30000);
        const int one_minute = 60 * 30 * drop_factor - drop_factor * 2;
        const int ten_minutes = 60 * 10 * 30 * drop_factor - drop_factor * 18;
        const int ten_minute_groups = frame / ten_minutes;
        const int last_ten_minutes  = frame % ten_minutes;

        frame += ten_minute_groups * 18 * drop_factor;
        frame += (last_ten_minutes - 2 * drop_factor) / one_minute * 2 * drop_factor;
    } else if (drop && ifps != FPS() && FPS() > 0) {
        frame = static_cast<int>(frame / FPS() * ifps + 0.5);
    }

    if (ifps <= 0) ifps = 1;
    *h = frame / (ifps * 60 * 60);
    *m = (frame / (ifps * 60)) % 60;
    *s = (frame / ifps) % 60;
    *f = frame % ifps;
}

void Framerate::SmpteAtTime(int ms, int *h, int *m, int *s, int *f) const {
    SmpteAtFrame(FrameAtTime(ms), h, m, s, f);
}

int Framerate::FrameAtSmpte(int h, int m, int s, int f) const {
    int ifps = static_cast<int>(std::ceil(FPS()));
    if (ifps <= 0) ifps = 1;

    if (drop && denominator == 1001 && numerator % 30000 == 0) {
        const int drop_factor = static_cast<int>(numerator / 30000);
        const int one_minute = 60 * 30 * drop_factor - drop_factor * 2;
        const int ten_minutes = 60 * 10 * 30 * drop_factor - drop_factor * 18;

        const int ten_m = m / 10;
        m = m % 10;

        if (m != 0 && s == 0 && f < 2 * drop_factor)
            f = 2 * drop_factor;

        return h * ten_minutes * 6 + ten_m * ten_minutes + m * one_minute + s * ifps + f;
    } else if (drop && ifps != FPS() && FPS() > 0) {
        int frame = (h * 60 * 60 + m * 60 + s) * ifps + f;
        return static_cast<int>(static_cast<double>(frame) / ifps * FPS() + 0.5);
    }

    return (h * 60 * 60 + m * 60 + s) * ifps + f;
}

int Framerate::TimeAtSmpte(int h, int m, int s, int f) const {
    return TimeAtFrame(FrameAtSmpte(h, m, s, f));
}

} // namespace agi::vfr
