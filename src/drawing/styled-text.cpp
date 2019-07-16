// Copyright (C) 1997, 1999-2001, 2008 Nathan Lamont
// Copyright (C) 2008-2017 The Antares Authors
//
// This file is part of Antares, a tactical space combat game.
//
// Antares is free software: you can redistribute it and/or modify it
// under the terms of the Lesser GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Antares is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with Antares.  If not, see http://www.gnu.org/licenses/

#include "drawing/styled-text.hpp"

#include <algorithm>
#include <limits>
#include <pn/output>

#include "data/resource.hpp"
#include "drawing/color.hpp"
#include "drawing/text.hpp"
#include "game/sys.hpp"
#include "video/driver.hpp"

using std::unique_ptr;

namespace antares {

namespace {

int hex_digit(pn::rune r) {
    int32_t c = r.value();
    if ('0' <= c && c <= '9') {
        return c - '0';
    } else if ('A' <= c && c <= 'Z') {
        return c - 'A' + 10;
    } else if ('a' <= c && c <= 'z') {
        return c - 'a' + 10;
    }
    throw std::runtime_error(pn::format("{0} is not a valid hex digit", c).c_str());
}

}  // namespace

StyledText::StyledText() : _wrap_metrics{sys.fonts.tactical} {}

StyledText::~StyledText() {}

StyledText StyledText::plain(
        pn::string_view text, WrapMetrics metrics, RgbColor fore_color, RgbColor back_color) {
    StyledText t;
    t._text         = text.copy();
    t._wrap_metrics = metrics;

    for (auto it = t._text.begin(), end = t._text.end(); it != end; ++it) {
        const auto r = *it;
        switch (r.value()) {
            case '\n':
                t._chars.push_back(StyledChar(it, LINE_BREAK, 0, fore_color, back_color));
                break;
            case ' ':
                t._chars.push_back(StyledChar(it, WORD_BREAK, 0, fore_color, back_color));
                break;
            case 0xA0:
                t._chars.push_back(StyledChar(it, NO_BREAK, 0, fore_color, back_color));
                break;
            default: t._chars.push_back(StyledChar(it, NONE, 0, fore_color, back_color)); break;
        }
    }
    if (t._chars.empty() || (t._chars.back().special != LINE_BREAK)) {
        t._chars.push_back(StyledChar(t._text.end(), LINE_BREAK, 0, fore_color, back_color));
    }

    t.rewrap();
    return t;
}

StyledText StyledText::retro(
        pn::string_view text, WrapMetrics metrics, RgbColor fore_color, RgbColor back_color) {
    StyledText t;
    t._text         = text.copy();
    t._wrap_metrics = metrics;

    const RgbColor original_fore_color = fore_color;
    const RgbColor original_back_color = back_color;
    pn::rune       r1;

    enum { START, SLASH, FG1, FG2, BG1, BG2 } state = START;

    for (auto it = t._text.begin(), end = t._text.end(); it != end; ++it) {
        const auto r = *it;
        switch (state) {
            case START:
                switch (r.value()) {
                    case '\n':
                        t._chars.push_back(StyledChar(it, LINE_BREAK, 0, fore_color, back_color));
                        break;

                    case '_':
                        // TODO(sfiera): replace use of "_" with e.g. "\_".
                        t._chars.push_back(StyledChar(it, NO_BREAK, 0, fore_color, back_color));
                        break;

                    case ' ':
                        t._chars.push_back(StyledChar(it, WORD_BREAK, 0, fore_color, back_color));
                        break;

                    case '\\':
                        state = SLASH;
                        t._chars.push_back(StyledChar(it, DELAY, 0, fore_color, back_color));
                        break;

                    default:
                        t._chars.push_back(StyledChar(it, NONE, 0, fore_color, back_color));
                        break;
                }
                break;

            case SLASH:
                switch (r.value()) {
                    case 'i':
                        std::swap(fore_color, back_color);
                        t._chars.push_back(StyledChar(it, DELAY, 0, fore_color, back_color));
                        state = START;
                        break;

                    case 'r':
                        fore_color = original_fore_color;
                        back_color = original_back_color;
                        t._chars.push_back(StyledChar(it, DELAY, 0, fore_color, back_color));
                        state = START;
                        break;

                    case 't':
                        t._chars.pop_back();
                        t._chars.push_back(StyledChar(it, TAB, 0, fore_color, back_color));
                        state = START;
                        break;

                    case '\\':
                        t._chars.pop_back();
                        t._chars.push_back(StyledChar(it, NONE, 0, fore_color, back_color));
                        state = START;
                        break;

                    case 'f':
                        t._chars.pop_back();
                        state = FG1;
                        break;

                    case 'b':
                        t._chars.pop_back();
                        state = BG1;
                        break;

                    default:
                        throw std::runtime_error(
                                pn::format("found bad special character {0}.", r.value()).c_str());
                }
                break;

            case FG1: r1 = r, state = FG2; break;
            case FG2:
                fore_color =
                        GetRGBTranslateColorShade(static_cast<Hue>(hex_digit(r1)), hex_digit(r));
                state = START;
                break;

            case BG1: r1 = r, state = BG2; break;
            case BG2:
                back_color =
                        GetRGBTranslateColorShade(static_cast<Hue>(hex_digit(r1)), hex_digit(r));
                state = START;
                break;
        }
    }

    if (state != START) {
        throw std::runtime_error(pn::format("not enough input for special code.").c_str());
    }

    if (t._chars.empty() || (t._chars.back().special != LINE_BREAK)) {
        t._chars.push_back(StyledChar(t._text.end(), LINE_BREAK, 0, fore_color, back_color));
    }

    t.rewrap();
    return t;
}

StyledText StyledText::interface(
        pn::string_view text, WrapMetrics metrics, RgbColor fore_color, RgbColor back_color) {
    StyledText t;
    t._text         = text.copy();
    t._wrap_metrics = metrics;

    const auto f = fore_color;
    const auto b = back_color;
    pn::string id;
    enum { START, CODE, ID } state = START;

    for (auto it = t._text.begin(), end = t._text.end(); it != end; ++it) {
        const auto r = *it;
        switch (state) {
            case START:
                switch (r.value()) {
                    case '\n': t._chars.push_back(StyledChar(it, LINE_BREAK, 0, f, b)); break;
                    case ' ': t._chars.push_back(StyledChar(it, WORD_BREAK, 0, f, b)); break;
                    default: t._chars.push_back(StyledChar(it, NONE, 0, f, b)); break;
                    case '^': state = CODE; break;
                }
                break;

            case CODE:
                if ((r != pn::rune{'P'}) && (r != pn::rune{'p'})) {
                    throw std::runtime_error(
                            pn::format(
                                    "found bad inline pict code {0}", pn::dump(r, pn::dump_short))
                                    .c_str());
                }
                state = ID;
                break;

            case ID:
                if (r != pn::rune{'^'}) {
                    id += r;
                    continue;
                }
                inlinePictType inline_pict;
                if ((inline_pict.object = BaseObject::get(id)) &&
                    inline_pict.object->portrait.has_value()) {
                    inline_pict.picture = inline_pict.object->portrait->copy();
                } else {
                    inline_pict.picture = std::move(id);
                }

                t._textures.push_back(Resource::texture(inline_pict.picture));
                inline_pict.bounds = t._textures.back().size().as_rect();
                t._inline_picts.emplace_back(std::move(inline_pict));
                t._chars.push_back(StyledChar(it, PICTURE, t._inline_picts.size() - 1, f, b));
                id.clear();
                state = START;
                break;
        }
    }

    if (t._chars.empty() || (t._chars.back().special != LINE_BREAK)) {
        t._chars.push_back(StyledChar(t._text.end(), LINE_BREAK, 0, f, b));
    }

    t.rewrap();
    return t;
}

pn::string_view     StyledText::text() const { return _text; }
void                StyledText::select(int from, int to) { _selection = {from, to}; }
std::pair<int, int> StyledText::selection() const { return _selection; }
void                StyledText::mark(int from, int to) { _mark = {from, to}; }
std::pair<int, int> StyledText::mark() const { return _mark; }

void StyledText::rewrap() {
    if (_wrap_metrics.tab_width <= 0) {
        _wrap_metrics.tab_width = _wrap_metrics.width / 2;
    }

    _auto_size = Size{0, 0};
    int h      = _wrap_metrics.side_margin;
    int v      = 0;

    const int line_height   = _wrap_metrics.font->height + _wrap_metrics.line_spacing;
    const int wrap_distance = _wrap_metrics.width - _wrap_metrics.side_margin;

    for (size_t i = 0; i < _chars.size(); ++i) {
        _chars[i].bounds = Rect{h, v, h, v + line_height};
        switch (_chars[i].special) {
            case NONE:
            case NO_BREAK:
                h += _wrap_metrics.font->char_width(*_chars[i].it);
                if (h >= wrap_distance) {
                    v += _wrap_metrics.font->height + _wrap_metrics.line_spacing;
                    h = move_word_down(i, v);
                }
                _auto_size.width = std::max(_auto_size.width, h);
                break;

            case TAB:
                h += _wrap_metrics.tab_width - (h % _wrap_metrics.tab_width);
                _auto_size.width = std::max(_auto_size.width, h);
                break;

            case LINE_BREAK:
                h = _wrap_metrics.side_margin;
                v += _wrap_metrics.font->height + _wrap_metrics.line_spacing;
                break;

            case WORD_BREAK: h += _wrap_metrics.font->char_width(*_chars[i].it); break;

            case PICTURE: {
                inlinePictType* pict = &_inline_picts[_chars[i].pict_index];
                if (h != _wrap_metrics.side_margin) {
                    v += _wrap_metrics.font->height + _wrap_metrics.line_spacing;
                }
                h = _wrap_metrics.side_margin;
                pict->bounds.offset(0, v - pict->bounds.top);
                v += pict->bounds.height() + _wrap_metrics.line_spacing + 3;
                if (_chars[i + 1].special == LINE_BREAK) {
                    v -= (_wrap_metrics.font->height + _wrap_metrics.line_spacing);
                }
            } break;

            case DELAY: break;
        }
        _chars[i].bounds.right = h;
    }
    _auto_size.height = v;
}

bool StyledText::empty() const {
    return _chars.size() <= 1;  // Always have \n at the end.
}

int StyledText::size() const { return _chars.size(); }

int StyledText::height() const { return _auto_size.height; }

int StyledText::auto_width() const { return _auto_size.width; }

const std::vector<inlinePictType>& StyledText::inline_picts() const { return _inline_picts; }

void StyledText::draw(const Rect& bounds) const { draw_range(bounds, 0, _chars.size()); }

void StyledText::draw_range(const Rect& bounds, int begin, int end) const {
    const Point char_adjust = {
            bounds.left, bounds.top + _wrap_metrics.font->ascent + _wrap_metrics.line_spacing};

    {
        Rects rects;
        bool  should_draw_caret = (0 <= _selection.first) &&
                                 (_selection.first == _selection.second) &&
                                 (_selection.second < _text.size());
        Rect prev_bounds;

        for (size_t i = begin; i < end; ++i) {
            const StyledChar& ch = _chars[i];
            Rect              r  = ch.bounds;
            r.offset(bounds.left, bounds.top);
            const RgbColor color = is_selected(ch) ? ch.fore_color : ch.back_color;

            if (should_draw_caret) {
                if (ch.it.offset() >= _selection.first) {
                    Rect bounds;
                    if (ch.special == LINE_BREAK) {
                        bounds = Rect{prev_bounds.right, prev_bounds.top, prev_bounds.right + 1,
                                      prev_bounds.bottom};
                    } else {
                        bounds = Rect{r.left, r.top, r.left + 1, r.bottom};
                    }
                    rects.fill(bounds, ch.fore_color);
                    should_draw_caret = false;
                } else {
                    prev_bounds = r;
                }
            }

            switch (ch.special) {
                case NONE:
                case NO_BREAK:
                case WORD_BREAK:
                case TAB:
                    if (color == RgbColor::black()) {
                        continue;
                    }
                    break;

                case LINE_BREAK:
                    if (color == RgbColor::black()) {
                        continue;
                    }
                    r.right = bounds.right;
                    break;

                case PICTURE:
                case DELAY: continue;
            }

            rects.fill(r, color);
        }

        if (should_draw_caret) {
            const StyledChar& ch = _chars.back();
            Rect              r  = ch.bounds;
            r.offset(bounds.left, bounds.top);
            rects.fill(Rect{r.right, r.top, r.right + 1, r.bottom}, ch.fore_color);
        }
    }

    {
        Quads quads(_wrap_metrics.font->texture);

        for (size_t i = begin; i < end; ++i) {
            const StyledChar& ch = _chars[i];
            if (ch.special == NONE) {
                RgbColor color = is_selected(ch) ? ch.back_color : ch.fore_color;
                Point    p = Point{ch.bounds.left + char_adjust.h, ch.bounds.top + char_adjust.v};
                _wrap_metrics.font->draw(quads, p, *ch.it, color);
            }
        }
    }

    for (size_t i = begin; i < end; ++i) {
        const StyledChar& ch     = _chars[i];
        Point             corner = bounds.origin();
        if (ch.special == PICTURE) {
            const inlinePictType& inline_pict = _inline_picts[ch.pict_index];
            const Texture&        texture     = _textures[ch.pict_index];
            corner.offset(
                    inline_pict.bounds.left, inline_pict.bounds.top + _wrap_metrics.line_spacing);
            texture.draw(corner.h, corner.v);
        }
    }
}

void StyledText::draw_cursor(const Rect& bounds, int index, const RgbColor& color) const {
    const int         line_height = _wrap_metrics.font->height + _wrap_metrics.line_spacing;
    const StyledChar& ch          = _chars[index];
    Rect              char_rect(0, 0, _wrap_metrics.font->logicalWidth, line_height);
    char_rect.offset(bounds.left + ch.bounds.left, bounds.top + ch.bounds.top);
    char_rect.clip_to(bounds);
    if ((char_rect.width() > 0) && (char_rect.height() > 0)) {
        Rects().fill(char_rect, color);
    }
}

int StyledText::move_word_down(int index, int v) {
    for (int i = index; i >= 0; --i) {
        switch (_chars[i].special) {
            case LINE_BREAK:
            case PICTURE: return _wrap_metrics.side_margin;

            case WORD_BREAK:
            case TAB:
            case DELAY: {
                if (_chars[i + 1].bounds.left <= _wrap_metrics.side_margin) {
                    return _wrap_metrics.side_margin;
                }

                int h = _wrap_metrics.side_margin;
                for (int j = i + 1; j <= index; ++j) {
                    _chars[j].bounds = Rect{Point{h, v}, _chars[j].bounds.size()};
                    h += _wrap_metrics.font->char_width(*_chars[j].it);
                }
                return h;
            }

            case NO_BREAK:
            case NONE: break;
        }
    }
    return _wrap_metrics.side_margin;
}

bool StyledText::is_selected(const StyledChar& ch) const {
    return (_selection.first <= ch.it.offset()) && (ch.it.offset() < _selection.second);
}

StyledText::StyledChar::StyledChar(
        pn::string::iterator it, SpecialChar special, int pict_index, const RgbColor& fore_color,
        const RgbColor& back_color)
        : it{it},
          special{special},
          pict_index{pict_index},
          fore_color{fore_color},
          back_color{back_color},
          bounds{0, 0, 0, 0} {}

}  // namespace antares
