// Copyright (c) 2022, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <libaegisub/signal.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace agi::ass {

struct KaraokeSyllable {
    int start_time = 0;
    int duration = 0;
    std::string text;
    std::string tag_type = "\\k";
    std::map<size_t, std::string> ovr_tags;

    std::string GetText(bool k_tag) const;
    friend bool operator==(KaraokeSyllable const&, KaraokeSyllable const&) = default;
};

class Karaoke {
    std::vector<KaraokeSyllable> syls;
    agi::signal::Signal<> AnnounceSyllablesChanged;

    void DoAddSplit(size_t syl_idx, size_t pos);
    void Normalize(int end_time);
    void AutoSplit();

public:
    void SetLine(std::vector<KaraokeSyllable>&& syls, bool auto_split, std::optional<int> end_time);

    void AddSplit(size_t syl_idx, size_t pos);
    void RemoveSplit(size_t syl_idx);
    void SetStartTime(size_t syl_idx, int time);
    void SetLineTimes(int start_time, int end_time);

    using iterator = std::vector<KaraokeSyllable>::const_iterator;
    iterator begin() const { return syls.begin(); }
    iterator end() const { return syls.end(); }
    size_t size() const { return syls.size(); }
    bool empty() const { return syls.empty(); }

    std::string GetText() const;
    std::string_view GetTagType() const;
    void SetTagType(std::string_view new_type);

    DEFINE_SIGNAL_ADDERS(AnnounceSyllablesChanged, AddSyllablesChangedListener)
};

} // namespace agi::ass
