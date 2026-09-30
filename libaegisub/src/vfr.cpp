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
#include <limits>
#include <numeric>

namespace {
static const int64_t default_denominator = 1000000000;
using agi::line_iterator;
using namespace agi::vfr;
constexpr int max_timecode_frames = 10'000'000;

bool multiply_nonnegative(int64_t a, int64_t b, int64_t &result) {
    if (a < 0 || b < 0 || (b && a > std::numeric_limits<int64_t>::max() / b)) return false;
    result = a * b;
    return true;
}

int bounded_integer(long double value) {
    return static_cast<int>(std::clamp(value, static_cast<long double>(std::numeric_limits<int>::min()),
                                      static_cast<long double>(std::numeric_limits<int>::max())));
}

std::string trim_line(std::string const& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

double parse_number(std::string const& text) {
    std::istringstream stream(text);
    double value = 0.;
    if (!(stream >> value) || !std::isfinite(value)) throw MalformedLine(text);
    stream >> std::ws;
    if (!stream.eof()) throw MalformedLine(text);
    return value;
}

void validate_timecodes(std::vector<int> const& timecodes) {
    if (timecodes.size() <= 1)
        throw InvalidFramerate("Must have at least two timecodes to do anything useful");
    if (timecodes.size() > max_timecode_frames || timecodes.front() < 0)
        throw InvalidFramerate("Timecodes exceed supported range");
    if (std::adjacent_find(timecodes.begin(), timecodes.end(), std::greater_equal<int>()) != timecodes.end())
        throw InvalidFramerate("Timecodes must be strictly increasing");
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
    if (ss.fail() || comma1 != ',' || comma2 != ',')
        throw MalformedLine(str);
    ss >> std::ws;
    if (!ss.eof()) throw MalformedLine(str);
    if (range.start < 0 || range.end < 0)
        throw InvalidFramerate("Cannot specify frame rate for negative frames.");
    if (range.end < range.start)
        throw InvalidFramerate("End frame must be greater than or equal to start frame");
    if (range.end >= max_timecode_frames)
        throw InvalidFramerate("Override range exceeds supported frame count");
    if (!std::isfinite(range.fps) || range.fps <= 0.)
        throw InvalidFramerate("FPS must be greater than zero");
    if (range.fps > 1000.)
        throw InvalidFramerate("FPS must be at most 1000");
    return range;
}

int64_t v1_parse(line_iterator<std::string> file, std::string line, std::vector<int> &timecodes, int64_t &last) {
    if (!line.starts_with("Assume ")) throw MalformedLine(line);
    double fps = parse_number(line.substr(7));
    if (fps < 1. / default_denominator) throw InvalidFramerate("Assumed FPS is too small");
    if (fps > 1000.) throw InvalidFramerate("Assumed FPS must not be greater than 1000");

    std::vector<TimecodeRange> ranges;
    for (auto const& l : file) {
        auto range = v1_parse_line(trim_line(l));
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
            if (time > std::numeric_limits<int>::max() - 1.)
                throw InvalidFramerate("Timecode exceeds supported duration");
            timecodes.push_back(static_cast<int>(time + .5));
            time += 1000. / fps;
        }
        for (; frame <= range.end; ++frame) {
            if (time > std::numeric_limits<int>::max() - 1.)
                throw InvalidFramerate("Timecode exceeds supported duration");
            timecodes.push_back(static_cast<int>(time + .5));
            time += 1000. / range.fps;
        }
    }
    if (time > std::numeric_limits<int>::max() - 1.)
        throw InvalidFramerate("Timecode exceeds supported duration");
    timecodes.push_back(static_cast<int>(time + .5));
    if (timecodes.size() > 1) validate_timecodes(timecodes);
    const long double unrounded = static_cast<long double>(time) * fps * default_denominator;
    if (unrounded >= std::numeric_limits<int64_t>::max())
        throw InvalidFramerate("Timecodes exceed supported timebase range");
    last = static_cast<int64_t>(unrounded);
    return static_cast<int64_t>(fps * default_denominator);
}
} // anonymous namespace

namespace agi::vfr {

Framerate::Framerate(double fps)
: denominator(default_denominator) {
    if (!std::isfinite(fps) || fps < 0.) throw InvalidFramerate("FPS must be nonnegative and finite");
    if (fps > 1000.) throw InvalidFramerate("FPS must not be greater than 1000");
    numerator = static_cast<int64_t>(fps * default_denominator);
    if (fps > 0 && numerator == 0) throw InvalidFramerate("FPS is too small");
    timecodes.push_back(0);
}

Framerate::Framerate(int64_t numerator, int64_t denominator, bool drop)
: denominator(denominator)
, numerator(numerator)
, drop(drop && denominator != 0 && numerator % denominator != 0) {
    if (numerator <= 0 || denominator <= 0)
        throw InvalidFramerate("Numerator and denominator must both be greater than zero");
    if (static_cast<long double>(numerator) / denominator > 1000) throw InvalidFramerate("FPS must not be greater than 1000");
    const int64_t divisor = std::gcd(numerator, denominator);
    this->numerator /= divisor;
    this->denominator /= divisor;
    if (this->denominator > std::numeric_limits<int64_t>::max() / 1000)
        throw InvalidFramerate("Timebase exceeds supported range");
    timecodes.push_back(0);
}

void Framerate::SetFromTimecodes() {
    validate_timecodes(timecodes);
    normalize_timecodes(timecodes);
    denominator = default_denominator;
    const int64_t frame_milliseconds = static_cast<int64_t>(timecodes.size() - 1) * 1000;
    numerator = frame_milliseconds / timecodes.back() * denominator +
                frame_milliseconds % timecodes.back() * denominator / timecodes.back();
    if (!multiply_nonnegative(frame_milliseconds, denominator, last))
        throw InvalidFramerate("Timecodes exceed supported timebase range");
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

    std::string line = trim_line(*it);
    if (line.starts_with("\xEF\xBB\xBF")) line.erase(0, 3);
    ++it;

    if (line == "# timecode format v2") {
        for (auto const& entry : it) {
            const auto l = trim_line(entry);
            if (!l.empty() && l[0] != '#') {
                const double value = parse_number(l);
                if (value < 0 || value > std::numeric_limits<int>::max() - .5 ||
                    timecodes.size() >= max_timecode_frames)
                    throw InvalidFramerate("Timecodes exceed supported range");
                timecodes.push_back(static_cast<int>(value + .5));
            }
        }
        SetFromTimecodes();
        return;
    }

    if (line == "# timecode format v1" || line.substr(0, 7) == "Assume ") {
        if (line[0] == '#') {
            if (it == line_iterator<std::string>())
                throw UnknownFormat("Premature EOF");
            line = trim_line(*it);
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
    file.Commit();
}

int Framerate::FrameAtTime(int ms, Time type) const {
    if (type == START)
        return bounded_integer(static_cast<long double>(FrameAtTime(ms == std::numeric_limits<int>::min() ? ms : ms - 1)) + 1);
    if (type == END)
        return FrameAtTime(ms == std::numeric_limits<int>::min() ? ms : ms - 1);

    if (ms < 0) {
        int64_t product = 0;
        if (multiply_nonnegative(-static_cast<int64_t>(ms), numerator, product))
            return bounded_integer((-product / denominator - 999) / 1000);
        return bounded_integer(std::trunc((std::trunc(static_cast<long double>(ms) * numerator / denominator) - 999) / 1000));
    }

    if (ms > timecodes.back()) {
        int64_t product = 0;
        const int64_t divisor = 1000 * denominator;
        if (multiply_nonnegative(static_cast<int64_t>(ms) + 1, numerator, product) &&
            product >= last && product - last >= numerator / 2 &&
            product - last - numerator / 2 <= std::numeric_limits<int64_t>::max() - divisor) {
            return bounded_integer((product - last - numerator / 2 + divisor - 1) / divisor +
                                   static_cast<int64_t>(timecodes.size()) - 2);
        }
        return bounded_integer(std::floor(((static_cast<long double>(ms) + 1) * numerator - last - numerator / 2 + divisor - 1) / divisor) + timecodes.size() - 2);
    }

    return static_cast<int>(std::distance(std::lower_bound(timecodes.rbegin(), timecodes.rend(), ms, std::greater<int>()), timecodes.rend())) - 1;
}

int Framerate::TimeAtFrame(int frame, Time type) const {
    if (type == START) {
        int prev = TimeAtFrame(frame == std::numeric_limits<int>::min() ? frame : frame - 1);
        int cur = TimeAtFrame(frame);
        return bounded_integer(static_cast<int64_t>(prev) + (static_cast<int64_t>(cur) - prev + 1) / 2);
    }

    if (type == END) {
        int cur = TimeAtFrame(frame);
        int next = TimeAtFrame(frame == std::numeric_limits<int>::max() ? frame : frame + 1);
        return bounded_integer(static_cast<int64_t>(cur) + (static_cast<int64_t>(next) - cur + 1) / 2);
    }

    if (numerator == 0)
        return 0;

    if (frame < 0) {
        int64_t product = 0;
        if (multiply_nonnegative(-static_cast<int64_t>(frame), denominator * 1000, product))
            return bounded_integer(-product / numerator);
        return bounded_integer(std::trunc(static_cast<long double>(frame) * denominator * 1000 / numerator));
    }

    if (frame >= static_cast<int>(timecodes.size())) {
        int64_t frames_past_end = frame - static_cast<int>(timecodes.size()) + 1;
        int64_t product = 0;
        if (multiply_nonnegative(frames_past_end, 1000 * denominator, product) &&
            last <= std::numeric_limits<int64_t>::max() - numerator / 2 &&
            product <= std::numeric_limits<int64_t>::max() - last - numerator / 2)
            return bounded_integer((product + last + numerator / 2) / numerator);
        return bounded_integer(std::floor((static_cast<long double>(frames_past_end) * 1000 * denominator + last + numerator / 2) / numerator));
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
