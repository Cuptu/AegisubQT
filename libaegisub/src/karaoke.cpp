// Copyright (c) 2022, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#include <libaegisub/ass/karaoke.h>

#include <libaegisub/format.h>

#include <algorithm>
#include <cassert>

namespace agi::ass {

std::string KaraokeSyllable::GetText(bool k_tag) const {
    std::string ret;

    if (k_tag)
        ret = agi::format("{%s%d}", tag_type.c_str(), ((duration + 5) / 10));

    std::string_view sv = text;
    size_t idx = 0;
    for (auto const& ovr : ovr_tags) {
        ret += sv.substr(idx, ovr.first - idx);
        ret += ovr.second;
        idx = ovr.first;
    }
    ret += sv.substr(idx);
    return ret;
}

void Karaoke::SetLine(std::vector<KaraokeSyllable>&& syls, bool auto_split, std::optional<int> end_time) {
    this->syls = std::move(syls);

    if (end_time && !this->syls.empty()) {
        Normalize(*end_time);
    }

    if (auto_split && size() == 1) {
        AutoSplit();
    }

    AnnounceSyllablesChanged();
}

void Karaoke::Normalize(int end_time) {
    if (syls.empty()) return;

    auto& last_syl = syls.back();
    int last_end = last_syl.start_time + last_syl.duration;

    if (last_end < end_time)
        last_syl.duration += end_time - last_end;
    else if (last_end > end_time) {
        for (auto& syl : syls) {
            if (syl.start_time > end_time) {
                syl.start_time = end_time;
                syl.duration = 0;
            } else {
                syl.duration = std::min(syl.duration, end_time - syl.start_time);
            }
        }
    }
}

void Karaoke::AutoSplit() {
    if (syls.empty()) return;
    size_t pos;
    while ((pos = syls.back().text.find(' ')) != std::string::npos)
        DoAddSplit(syls.size() - 1, pos + 1);
}

std::string Karaoke::GetText() const {
    std::string text;
    text.reserve(size() * 10);

    for (auto const& syl : syls)
        text += syl.GetText(true);

    return text;
}

std::string_view Karaoke::GetTagType() const {
    return syls.empty() ? "\\k" : begin()->tag_type;
}

void Karaoke::SetTagType(std::string_view new_type) {
    for (auto& syl : syls)
        syl.tag_type = new_type;
}

void Karaoke::DoAddSplit(size_t syl_idx, size_t pos) {
    syls.insert(syls.begin() + syl_idx + 1, KaraokeSyllable());
    KaraokeSyllable &syl = syls[syl_idx];
    KaraokeSyllable &new_syl = syls[syl_idx + 1];

    if (pos < syl.text.size()) {
        new_syl.text = syl.text.substr(pos);
        syl.text = syl.text.substr(0, pos);
    }

    if (new_syl.text.empty())
        new_syl.duration = 0;
    else if (syl.text.empty()) {
        new_syl.duration = syl.duration;
        syl.duration = 0;
    } else {
        new_syl.duration = static_cast<int>((static_cast<double>(syl.duration) * new_syl.text.size() / (syl.text.size() + new_syl.text.size()) + 5.0) / 10.0) * 10;
        syl.duration -= new_syl.duration;
    }

    assert(syl.duration >= 0);

    new_syl.start_time = syl.start_time + syl.duration;
    new_syl.tag_type = syl.tag_type;

    size_t text_len = syl.text.size();
    for (auto it = syl.ovr_tags.begin(); it != syl.ovr_tags.end(); ) {
        if (it->first < text_len)
            ++it;
        else {
            new_syl.ovr_tags[it->first - text_len] = it->second;
            it = syl.ovr_tags.erase(it);
        }
    }
}

void Karaoke::AddSplit(size_t syl_idx, size_t pos) {
    DoAddSplit(syl_idx, pos);
    AnnounceSyllablesChanged();
}

void Karaoke::RemoveSplit(size_t syl_idx) {
    if (syl_idx == 0 || syl_idx >= syls.size()) return;

    KaraokeSyllable &syl = syls[syl_idx];
    KaraokeSyllable &prev = syls[syl_idx - 1];

    prev.duration += syl.duration;
    for (auto const& tag : syl.ovr_tags)
        prev.ovr_tags[tag.first + prev.text.size()] = tag.second;
    prev.text += syl.text;

    syls.erase(syls.begin() + syl_idx);

    AnnounceSyllablesChanged();
}

void Karaoke::SetStartTime(size_t syl_idx, int time) {
    if (syl_idx == 0 || syl_idx >= syls.size()) return;

    KaraokeSyllable &syl = syls[syl_idx];
    KaraokeSyllable &prev = syls[syl_idx - 1];

    assert(time >= prev.start_time);
    assert(time <= syl.start_time + syl.duration);

    int delta = time - syl.start_time;
    syl.start_time = time;
    syl.duration -= delta;
    prev.duration += delta;
}

void Karaoke::SetLineTimes(int start_time, int end_time) {
    if (syls.empty()) return;
    assert(end_time >= start_time);

    size_t idx = 0;
    do {
        int delta = start_time - syls[idx].start_time;
        syls[idx].start_time = start_time;
        syls[idx].duration = std::max(0, syls[idx].duration - delta);
    } while (++idx < syls.size() && syls[idx].start_time < start_time);

    idx = syls.size() - 1;
    while (idx < syls.size() && syls[idx].start_time > end_time) {
        syls[idx].start_time = end_time;
        syls[idx].duration = 0;
        if (idx == 0) break;
        --idx;
    }
    if (idx < syls.size())
        syls[idx].duration = end_time - syls[idx].start_time;
}

} // namespace agi::ass
