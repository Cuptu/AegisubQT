// Copyright (c) 2012, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#include "libaegisub/ass/dialogue_parser.h"

#include <cassert>
#include <cctype>

namespace {

using TokenVec = std::vector<agi::ass::DialogueToken>;
using namespace agi::ass;
namespace dt = DialogueTokenType;
namespace ss = SyntaxStyle;

class SyntaxHighlighter {
    TokenVec ranges;
    std::string_view text;

    void SetStyling(size_t len, int type) {
        if (!ranges.empty() && ranges.back().type == type)
            ranges.back().length += len;
        else
            ranges.push_back(DialogueToken{type, len});
    }

public:
    SyntaxHighlighter(std::string_view text) : text(text) { }

    TokenVec Highlight(TokenVec const& tokens) {
        if (tokens.empty()) return ranges;

        for (auto tok : tokens) {
            switch (tok.type) {
                case dt::KARAOKE_TEMPLATE: SetStyling(tok.length, ss::KARAOKE_TEMPLATE); break;
                case dt::KARAOKE_VARIABLE: SetStyling(tok.length, ss::KARAOKE_VARIABLE); break;
                case dt::LINE_BREAK: SetStyling(tok.length, ss::LINE_BREAK); break;
                case dt::ERROR:      SetStyling(tok.length, ss::ERROR);      break;
                case dt::ARG:        SetStyling(tok.length, ss::PARAMETER);  break;
                case dt::COMMENT:    SetStyling(tok.length, ss::COMMENT);    break;
                case dt::DRAWING_CMD:SetStyling(tok.length, ss::DRAWING_CMD);break;
                case dt::DRAWING_X:  SetStyling(tok.length, ss::DRAWING_X);  break;
                case dt::DRAWING_Y:  SetStyling(tok.length, ss::DRAWING_Y);  break;
                case dt::DRAWING_ENDPOINT_X: SetStyling(tok.length, ss::DRAWING_ENDPOINT_X); break;
                case dt::DRAWING_ENDPOINT_Y: SetStyling(tok.length, ss::DRAWING_ENDPOINT_Y); break;
                case dt::TEXT:       SetStyling(tok.length, ss::NORMAL);     break;
                case dt::TAG_NAME:   SetStyling(tok.length, ss::TAG);        break;
                case dt::OPEN_PAREN: case dt::CLOSE_PAREN: case dt::ARG_SEP: case dt::TAG_START:
                    SetStyling(tok.length, ss::PUNCTUATION);
                    break;
                case dt::OVR_BEGIN: case dt::OVR_END:
                    SetStyling(tok.length, ss::OVERRIDE);
                    break;
                case dt::WHITESPACE:
                    if (!ranges.empty() && ranges.back().type == ss::PARAMETER)
                        SetStyling(tok.length, ss::PARAMETER);
                    else if (!ranges.empty() && ranges.back().type == ss::DRAWING_ENDPOINT_X)
                        SetStyling(tok.length, ss::DRAWING_ENDPOINT_X);
                    else
                        SetStyling(tok.length, ss::NORMAL);
                    break;
                case dt::WORD:
                    SetStyling(tok.length, ss::NORMAL);
                    break;
                default:
                    SetStyling(tok.length, ss::NORMAL);
                    break;
            }
        }
        return ranges;
    }
};

class WordSplitter {
    std::string_view text;
    std::vector<DialogueToken> &tokens;
    size_t pos = 0;

    void SwitchTo(size_t &i, int type, size_t len) {
        auto old = tokens[i];
        tokens[i].type = type;
        tokens[i].length = len;

        if (old.length != len) {
            tokens.insert(tokens.begin() + i + 1, DialogueToken{old.type, old.length - len});
            ++i;
        }
    }

    static bool is_word_char(unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c >= 0x80);
    }

    void SplitText(size_t &i) {
        size_t token_len = tokens[i].length;
        if (token_len == 0) return;

        size_t offset = 0;
        while (offset < token_len) {
            bool in_word = is_word_char(static_cast<unsigned char>(text[pos + offset]));
            size_t seg_start = offset;
            while (offset < token_len && is_word_char(static_cast<unsigned char>(text[pos + offset])) == in_word) {
                // If it's a UTF-8 lead byte, consume full character
                unsigned char uc = static_cast<unsigned char>(text[pos + offset]);
                if (uc >= 0xC0) {
                    size_t char_bytes = 1;
                    if ((uc & 0xE0) == 0xC0) char_bytes = 2;
                    else if ((uc & 0xF0) == 0xE0) char_bytes = 3;
                    else if ((uc & 0xF8) == 0xF0) char_bytes = 4;
                    offset += std::min(char_bytes, token_len - offset);
                } else {
                    ++offset;
                }
            }
            size_t seg_len = offset - seg_start;
            SwitchTo(i, in_word ? dt::WORD : dt::TEXT, seg_len);
        }
    }

    void SplitDrawing(size_t &i) {
        size_t starti = i;

        size_t dpos = pos;
        size_t tlen = 0;
        bool tokentype = text[pos] == ' ' || text[pos] == '\t';
        while (tlen < tokens[i].length) {
            bool newtype = text[dpos] == ' ' || text[dpos] == '\t';
            if (newtype != tokentype) {
                tokentype = newtype;
                SwitchTo(i, tokentype ? dt::DRAWING_FULL : dt::WHITESPACE, tlen);
                tokens[i].type = tokentype ? dt::WHITESPACE : dt::DRAWING_FULL;
                tlen = 0;
            }
            ++tlen;
            ++dpos;
        }

        dpos = pos;
        int num_coord = 0;
        char lastcmd = ' ';

        for (size_t j = starti; j <= i; j++) {
            char c = text[dpos];
            if (tokens[j].type == dt::WHITESPACE) {
            } else if (lastcmd == ' ' && c != 'm') {
                tokens[j].type = dt::ERROR;
            } else if (c == 'm' || c == 'n' || c == 'l' || c == 's' || c == 'b' || c == 'p' || c == 'c') {
                tokens[j].type = dt::DRAWING_CMD;

                if (tokens[j].length != 1)
                    tokens[j].type = dt::ERROR;
                if (num_coord % 2 != 0)
                    tokens[j].type = dt::ERROR;

                lastcmd = c;
                num_coord = 0;
            } else {
                bool valid = true;
                for (size_t k = 0; k < tokens[j].length; k++) {
                    char ch = text[dpos + k];
                    if (!((ch >= '0' && ch <= '9') || ch == '.' || ch == '+' || ch == '-' || ch == 'e' || ch == 'E')) {
                        valid = false;
                    }
                }
                if (!valid)
                    tokens[j].type = dt::ERROR;
                else if (lastcmd == 'b' && num_coord % 6 >= 4)
                    tokens[j].type = num_coord % 2 == 0 ? dt::DRAWING_ENDPOINT_X : dt::DRAWING_ENDPOINT_Y;
                else
                    tokens[j].type = num_coord % 2 == 0 ? dt::DRAWING_X : dt::DRAWING_Y;
                ++num_coord;
            }

            dpos += tokens[j].length;
        }
    }

public:
    WordSplitter(std::string_view text, std::vector<DialogueToken> &tokens)
    : text(text), tokens(tokens) { }

    void SplitWords() {
        if (tokens.empty()) return;

        for (size_t i = 0; i < tokens.size(); ++i) {
            size_t len = tokens[i].length;
            if (tokens[i].type == dt::TEXT)
                SplitText(i);
            else if (tokens[i].type == dt::DRAWING_FULL) {
                SplitDrawing(i);
            }
            pos += len;
        }
    }
};

} // anonymous namespace

namespace agi::ass {

std::vector<DialogueToken> TokenizeDialogueBody(std::string_view str, bool karaoke_templater) {
    std::vector<DialogueToken> data;
    auto emit = [&](int type, size_t len) {
        if (len == 0) return;
        if (!data.empty() && data.back().type == type) {
            data.back().length += len;
        } else {
            data.push_back(DialogueToken{type, len});
        }
    };

    enum State {
        INITIAL,
        OVR,
        TAGSTART,
        TAGNAME,
        ARG
    };

    State state = INITIAL;
    int paren_depth = 0;
    size_t i = 0;
    const size_t n = str.size();

    auto match_karaoke_template = [&]() -> size_t {
        if (!karaoke_templater || i >= n) return 0;
        if (str[i] == '!') {
            size_t end_pos = str.find('!', i + 1);
            if (end_pos != std::string_view::npos) {
                return (end_pos - i + 1);
            }
        }
        return 0;
    };

    auto match_karaoke_variable = [&]() -> size_t {
        if (!karaoke_templater || i >= n) return 0;
        if (str[i] == '$' && i + 1 < n) {
            char next = str[i + 1];
            if ((next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z') || next == '_') {
                size_t j = i + 1;
                while (j < n && ((str[j] >= 'A' && str[j] <= 'Z') || (str[j] >= 'a' && str[j] <= 'z') || str[j] == '_'))
                    ++j;
                return j - i;
            }
        }
        return 0;
    };

    while (i < n) {
        // Check karaoke templates / variables if enabled
        if (size_t kt_len = match_karaoke_template()) {
            emit(dt::KARAOKE_TEMPLATE, kt_len);
            i += kt_len;
            continue;
        }
        if (size_t kv_len = match_karaoke_variable()) {
            emit(dt::KARAOKE_VARIABLE, kv_len);
            i += kv_len;
            continue;
        }

        switch (state) {
            case INITIAL: {
                if (i + 1 < n && str[i] == '\\' && (str[i+1] == 'n' || str[i+1] == 'N' || str[i+1] == 'h')) {
                    emit(dt::LINE_BREAK, 2);
                    i += 2;
                } else if (str[i] == '{') {
                    emit(dt::OVR_BEGIN, 1);
                    paren_depth = 0;
                    state = OVR;
                    ++i;
                } else {
                    emit(dt::TEXT, 1);
                    ++i;
                }
                break;
            }
            case OVR: {
                if (str[i] == '{') {
                    emit(dt::ERROR, 1);
                    ++i;
                } else if (str[i] == '}') {
                    emit(dt::OVR_END, 1);
                    state = INITIAL;
                    ++i;
                } else if (str[i] == '\\') {
                    emit(dt::TAG_START, 1);
                    state = TAGSTART;
                    ++i;
                } else if (std::isspace(static_cast<unsigned char>(str[i]))) {
                    size_t j = i;
                    while (j < n && std::isspace(static_cast<unsigned char>(str[j]))) ++j;
                    emit(dt::WHITESPACE, j - i);
                    i = j;
                } else {
                    emit(dt::COMMENT, 1);
                    ++i;
                }
                break;
            }
            case TAGSTART: {
                if (std::isspace(static_cast<unsigned char>(str[i]))) {
                    size_t j = i;
                    while (j < n && std::isspace(static_cast<unsigned char>(str[j]))) ++j;
                    emit(dt::WHITESPACE, j - i);
                    i = j;
                } else if (str[i] == 'r') {
                    emit(dt::TAG_NAME, 1);
                    state = ARG;
                    ++i;
                } else if (str[i] == 'f' && i + 1 < n && str[i+1] == 'n') {
                    emit(dt::TAG_NAME, 2);
                    state = ARG;
                    i += 2;
                } else if (str[i] == '\\') {
                    emit(dt::TAG_START, 1);
                    state = TAGSTART;
                    ++i;
                } else if (str[i] == '}') {
                    emit(dt::OVR_END, 1);
                    state = INITIAL;
                    ++i;
                } else if ((str[i] >= 'a' && str[i] <= 'z') || (str[i] >= '0' && str[i] <= '9')) {
                    emit(dt::TAG_NAME, 1);
                    state = TAGNAME;
                    ++i;
                } else {
                    emit(dt::COMMENT, 1);
                    state = OVR;
                    ++i;
                }
                break;
            }
            case TAGNAME: {
                if (str[i] >= 'a' && str[i] <= 'z') {
                    size_t j = i;
                    while (j < n && (str[j] >= 'a' && str[j] <= 'z')) ++j;
                    emit(dt::TAG_NAME, j - i);
                    state = ARG;
                    i = j;
                } else if (str[i] == '(') {
                    emit(dt::OPEN_PAREN, 1);
                    ++paren_depth;
                    state = ARG;
                    ++i;
                } else if (str[i] == ')') {
                    emit(dt::CLOSE_PAREN, 1);
                    --paren_depth;
                    if (paren_depth == 0) state = OVR;
                    ++i;
                } else if (str[i] == '}') {
                    emit(dt::OVR_END, 1);
                    state = INITIAL;
                    ++i;
                } else if (str[i] == '\\') {
                    emit(dt::TAG_START, 1);
                    state = TAGSTART;
                    ++i;
                } else {
                    emit(dt::ARG, 1);
                    state = ARG;
                    ++i;
                }
                break;
            }
            case ARG: {
                if (str[i] == '{') {
                    emit(dt::ERROR, 1);
                    ++i;
                } else if (str[i] == '}') {
                    emit(dt::OVR_END, 1);
                    state = INITIAL;
                    ++i;
                } else if (str[i] == '(') {
                    emit(dt::OPEN_PAREN, 1);
                    ++paren_depth;
                    ++i;
                } else if (str[i] == ')') {
                    emit(dt::CLOSE_PAREN, 1);
                    --paren_depth;
                    if (paren_depth <= 0) {
                        paren_depth = 0;
                        state = OVR;
                    }
                    ++i;
                } else if (str[i] == '\\') {
                    emit(dt::TAG_START, 1);
                    state = TAGSTART;
                    ++i;
                } else if (str[i] == ',') {
                    emit(dt::ARG_SEP, 1);
                    ++i;
                } else if (std::isspace(static_cast<unsigned char>(str[i]))) {
                    size_t j = i;
                    while (j < n && std::isspace(static_cast<unsigned char>(str[j]))) ++j;
                    emit(dt::WHITESPACE, j - i);
                    i = j;
                } else {
                    emit(dt::ARG, 1);
                    ++i;
                }
                break;
            }
        }
    }

    return data;
}

void MarkDrawings(std::string_view str, std::vector<DialogueToken> &tokens) {
    if (tokens.empty()) return;

    size_t last_ovr_end = 0;
    for (size_t i = tokens.size(); i > 0; --i) {
        if (tokens[i - 1].type == dt::OVR_END) {
            last_ovr_end = i;
            break;
        }
    }

    size_t pos = 0;
    bool in_drawing = false;

    for (size_t i = 0; i < last_ovr_end; ++i) {
        size_t len = tokens[i].length;
        switch (tokens[i].type) {
            case dt::TEXT:
                if (in_drawing)
                    tokens[i].type = dt::DRAWING_FULL;
                break;
            case dt::TAG_NAME:
                if (i + 3 < tokens.size() && (len == 4 || len == 5) && str.substr(pos, len).ends_with("clip")) {
                    if (tokens[i + 1].type != dt::OPEN_PAREN)
                        goto tag_p;

                    size_t drawing_start = 0;
                    size_t drawing_end = 0;

                    for (size_t j = i + 2; j < tokens.size(); j++) {
                        if (tokens[j].type == dt::ARG_SEP) {
                            if (drawing_start) break;
                            drawing_start = j + 1;
                        } else if (tokens[j].type == dt::CLOSE_PAREN) {
                            drawing_end = j;
                            break;
                        } else if (tokens[j].type != dt::WHITESPACE && tokens[j].type != dt::ARG) {
                            break;
                        }
                    }

                    if (!drawing_end) goto tag_p;
                    if (!drawing_start) drawing_start = i + 2;
                    if (drawing_end == drawing_start) goto tag_p;

                    size_t tokenlen = 0;
                    for (size_t j = drawing_start; j < drawing_end; j++) {
                        tokenlen += tokens[j].length;
                    }

                    tokens[drawing_start].length = tokenlen;
                    tokens[drawing_start].type = dt::DRAWING_FULL;
                    tokens.erase(tokens.begin() + drawing_start + 1, tokens.begin() + drawing_end);
                    last_ovr_end -= drawing_end - drawing_start - 1;
                }
tag_p:
                if (len != 1 || i + 1 >= tokens.size() || str[pos] != 'p')
                    break;

                in_drawing = false;
                if (i + 1 == last_ovr_end || tokens[i + 1].type != dt::ARG)
                    break;

                for (size_t j = pos + len; j < pos + len + tokens[i + 1].length; ++j) {
                    char c = str[j];
                    if (c >= '1' && c <= '9')
                        in_drawing = true;
                    else if (c != '0')
                        break;
                }
                break;
            default: break;
        }

        pos += len;
    }

    for (size_t i = last_ovr_end; i < tokens.size(); ++i) {
        switch (tokens[i].type) {
            case dt::KARAOKE_TEMPLATE: break;
            case dt::KARAOKE_VARIABLE: break;
            case dt::LINE_BREAK: break;
            default:
                tokens[i].type = in_drawing ? dt::DRAWING_FULL : dt::TEXT;
                if (i > 0 && tokens[i - 1].type == tokens[i].type) {
                    tokens[i - 1].length += tokens[i].length;
                    tokens.erase(tokens.begin() + i);
                    --i;
                }
        }
    }
}

void SplitWords(std::string_view str, std::vector<DialogueToken> &tokens) {
    MarkDrawings(str, tokens);
    WordSplitter(str, tokens).SplitWords();
}

std::vector<DialogueToken> SyntaxHighlight(std::string_view text,
                                           std::vector<DialogueToken> const& tokens,
                                           SpellChecker*) {
    return SyntaxHighlighter(text).Highlight(tokens);
}

} // namespace agi::ass
