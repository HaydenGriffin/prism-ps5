// Prism - The native screens: one compact line of chrome, and everything else for the content.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prism/application.hpp"
#include "ui/glyphs.hpp"
#include <cmath>
#include <cstdlib>

namespace prism
{
using hui::gfx::Align;
using hui::gfx::Rect;
using hui::ui::Button;
using hui::ui::Canvas;
namespace
{
constexpr float kLeft = 96.0f, kRight = 1824.0f;
constexpr int kDockTiles = 5;
const hui::ui::FontRef &face(const Canvas &c, bool bold)
{
    return bold ? c.fonts.semibold : c.fonts.regular;
}
const hui::ui::FontRef &numerals(const Canvas &c, bool bold)
{
    return bold ? c.fonts.display : c.fonts.mono;
}
float words_width(const Canvas &c, std::string_view s, float size, bool bold)
{
    float width = 0.0f;
    for (const auto &run : digit_runs(s))
        width += (run.digits ? numerals(c, bold) : face(c, bold)).measure(run.text, size);
    return width;
}
// One line of text, cut to fit `width`, with its digits in Lexend. Returns the width drawn.
float words(Canvas &c, std::string_view s, float x, float y, float size, float width, Color color,
            bool bold = false, Align align = Align::left)
{
    std::string fitted(s);
    if (words_width(c, fitted, size, bold) > width)
        fitted = face(c, bold).font->fit(s, size, width);
    const float drawn = words_width(c, fitted, size, bold);
    float at = align == Align::center ? x - drawn * 0.5f : align == Align::right ? x - drawn : x;
    for (const auto &run : digit_runs(fitted))
        at += hui::ui::text(c.list, run.digits ? numerals(c, bold) : face(c, bold), run.text, at, y,
                            size, color);
    return drawn;
}
void paragraph(Canvas &c, std::string_view s, float x, float y, float size, float width,
               float leading, Color color, int lines)
{
    const auto wrapped = face(c, false).font->wrap(s, size, width);
    for (int i = 0; i < std::min(lines, static_cast<int>(wrapped.size())); ++i)
        words(c, wrapped[static_cast<std::size_t>(i)], x, y + static_cast<float>(i) * leading, size,
              width, color);
}
// A name on up to two lines, centred on x, the first baseline at y.
void centred_name(Canvas &c, const std::string &s, float x, float y, float size, float width,
                  float leading, Color color)
{
    const auto lines = face(c, true).font->wrap(s, size, width);
    if (lines.size() < 2)
    {
        words(c, s, x, y + leading * 0.5f, size, width, color, true, Align::center);
        return;
    }
    words(c, lines[0], x, y, size, width, color, true, Align::center);
    std::string rest = lines[1];
    for (std::size_t i = 2; i < lines.size(); ++i)
        rest += " " + lines[i];
    words(c, rest, x, y + leading, size, width, color, true, Align::center);
}
// The line icons of the tiles that are places rather than apps, drawn on a 48-unit grid.
void draw_glyph(Canvas &c, const std::string &id, const Rect &box, Color color)
{
    const float s = box.w / 48.0f, stroke = 3.5f * s;
    const auto line = [&](float x1, float y1, float x2, float y2)
    { c.list.line(box.x + x1 * s, box.y + y1 * s, box.x + x2 * s, box.y + y2 * s, stroke, color); };
    const auto square = [&](float x, float y, float w, float h, float radius)
    {
        c.list.bordered_rect({box.x + x * s, box.y + y * s, w * s, h * s}, radius * s,
                             Color{0.0f, 0.0f, 0.0f, 0.0f}, stroke, color);
    };
    const auto dot = [&](float x, float y, float radius)
    { c.list.circle(box.x + x * s, box.y + y * s, radius * s, color); };
    if (id == "@apps")
    {
        for (int i = 0; i < 4; ++i)
            square(9.0f + static_cast<float>(i % 2) * 18.0f,
                   9.0f + static_cast<float>(i / 2) * 18.0f, 12.0f, 12.0f, 2.0f);
    }
    else if (id == "@home")
    {
        line(8, 23, 24, 9);
        line(24, 9, 40, 23);
        line(13, 20, 13, 39);
        line(13, 39, 35, 39);
        line(35, 39, 35, 20);
    }
    else if (id == "@settings")
    {
        // A gear: a toothed ring round a small hub.
        const float cx = box.x + 24.0f * s, cy = box.y + 24.0f * s;
        c.list.ring(cx, cy, 12.0f * s, stroke * 1.5f, color);
        c.list.ring(cx, cy, 3.5f * s, stroke * 0.8f, color);
        for (int i = 0; i < 8; ++i)
        {
            const float angle = static_cast<float>(i) * 0.7854f;
            const float dx = std::sin(angle) * s, dy = -std::cos(angle) * s;
            c.list.line(cx + dx * 14.0f, cy + dy * 14.0f, cx + dx * 18.0f, cy + dy * 18.0f,
                        stroke * 2.0f, color);
        }
    }
    else if (id == "@quick")
    {
        line(7, 13, 41, 13);
        line(7, 24, 41, 24);
        line(7, 35, 41, 35);
        dot(16, 13, 5);
        dot(32, 24, 5);
        dot(20, 35, 5);
    }
    else if (id == "@remote")
    {
        const float x[] = {18, 30, 30, 41, 41, 30, 30, 18, 18, 7, 7, 18};
        const float y[] = {7, 7, 18, 18, 30, 30, 41, 41, 30, 30, 18, 18};
        for (int i = 0; i < 12; ++i)
            line(x[i], y[i], x[(i + 1) % 12], y[(i + 1) % 12]);
    }
    else if (id == "@setup" || id == "@connect")
    {
        square(14, 5, 20, 38, 6);
        c.list.ring(box.x + 24.0f * s, box.y + 15.0f * s, 3.5f * s, stroke * 0.8f, color);
        line(20, 27, 28, 27);
        line(20, 34, 28, 34);
    }
    else
    {
        c.list.star(box.x + 24.0f * s, box.y + 24.0f * s, 19.0f * s, color, stroke);
    }
}
hui::ui::GlyphStyle glyph_style(const Look &look)
{
    if (look.square)
        return hui::ui::GlyphStyle::mono(look.text, look.panel);
    return look.dark ? hui::ui::GlyphStyle::dark() : hui::ui::GlyphStyle::light();
}
// Glasshouse's #rrggbb, or nothing.
bool parse_colour(const std::string &hex, Color &out)
{
    if (hex.size() != 7 || hex[0] != '#')
        return false;
    char *end = nullptr;
    const auto rgb = std::strtoul(hex.c_str() + 1, &end, 16);
    if (!end || *end)
        return false;
    out = Color::rgb(static_cast<std::uint32_t>(rgb));
    return true;
}
std::string sentence_case(std::string value)
{
    if (!value.empty() && value[0] >= 'a' && value[0] <= 'z')
        value[0] = static_cast<char>(value[0] - 'a' + 'A');
    return value;
}
} // namespace
void Application::setup_view()
{
    grid_.content =
        [this](Canvas &c, const Rect &r, const hui::ui::CardItem &, int index, float focus)
    {
        if (page_ == Page::setup)
            draw_setup_row(c, r, index, focus);
        else
            draw_app_cell(c, r, index, focus);
    };
    list_.content = [this](Canvas &c, const Rect &r, const hui::ui::ListItem &item, int,
                           float focus) { draw_list_row(c, r, item, focus); };
}
// A tile's plate. Its hairline gives way to the focus ring, so the two never read as a double line.
void Application::draw_panel(Canvas &c, const Rect &r, float radius, float focus) const
{
    const float round = look_->square ? 0.0f : radius;
    if (look_->edge.a > 0.0f && focus < 1.0f)
        c.list.bordered_rect(r, round, look_->panel, 2.0f, look_->edge.with_alpha(1.0f - focus));
    else
        c.list.rounded_rect(r, round, look_->panel);
}
// An app's art on the app's own colour; its initial until the icon arrives, or if it never does.
void Application::draw_art(Canvas &c, const Tile &tile, const Rect &art) const
{
    const float round = look_->square ? 0.0f : art.w * 0.2f;
    Color ground = Color::rgb(0x28343d);
    const bool coloured = parse_colour(tile.color, ground);
    const auto it = textures_.find(tile.icon);
    if (it == textures_.end())
    {
        c.list.rounded_rect(art, round, ground);
        const auto initial = tile.title.empty() ? std::string("?") : tile.title.substr(0, 1);
        const float size = art.w * 0.52f;
        words(c, initial, art.cx(), art.cy() + size * 0.36f, size, art.w,
              luminance(ground) > 0.45 ? Color::rgb(0x16181d) : Color::rgb(0xf4f2ee), true,
              Align::center);
        return;
    }
    const Color untinted{1.0f, 1.0f, 1.0f, 1.0f};
    if (coloured)
    {
        c.list.rounded_rect(art, round, ground);
        c.list.image(it->second, art.inset(art.w * 0.08f), hui::gfx::kFullUv, untinted,
                     round * 0.7f);
    }
    else
    {
        c.list.image(it->second, art, hui::gfx::kFullUv, untinted, round);
    }
}
void Application::draw_big_tile(Canvas &c, const Rect &r, int index, float focus) const
{
    const auto &tile = tiles_[static_cast<std::size_t>(index)];
    draw_panel(c, r, kBigRadius, focus);
    const Rect art{r.x + 48.0f, r.y + 48.0f, 300.0f, 300.0f};
    if (tile.id.starts_with("@"))
    {
        c.list.rounded_rect(art, look_->square ? 0.0f : 60.0f, look_->raised);
        draw_glyph(c, tile.id, art.inset(70.0f), look_->text);
    }
    else
    {
        draw_art(c, tile, art);
    }
    const float size = 68.0f, width = r.w - 96.0f, bottom = r.y + r.h - 48.0f;
    const auto lines = face(c, true).font->wrap(tile.title, size, width);
    const int count = std::min(2, static_cast<int>(lines.size()));
    for (int i = 0; i < count; ++i)
        words(c, lines[static_cast<std::size_t>(i)], r.x + 48.0f,
              bottom - static_cast<float>(count - 1 - i) * 78.0f, size, width, look_->text, true);
    words(c, tile.subtitle, r.x + 48.0f, bottom - static_cast<float>(count) * 78.0f, 32.0f, width,
          look_->muted);
}
void Application::draw_grid_tile(Canvas &c, const Rect &r, int index, float focus) const
{
    const auto &tile = tiles_[static_cast<std::size_t>(index)];
    draw_panel(c, r, kTileRadius, focus);
    const Rect art{r.cx() - 80.0f, r.y + 34.0f, 160.0f, 160.0f};
    if (tile.id.starts_with("@"))
    {
        c.list.rounded_rect(art, look_->square ? 0.0f : 32.0f, look_->raised);
        draw_glyph(c, tile.id, art.inset(36.0f), look_->text);
    }
    else
    {
        draw_art(c, tile, art);
    }
    centred_name(c, tile.title, r.cx(), r.y + 248.0f, 33.0f, r.w - 28.0f, 40.0f, look_->text);
}
void Application::draw_dock_tile(Canvas &c, const Rect &r, int index, float focus) const
{
    const auto &tile = tiles_[static_cast<std::size_t>(index)];
    draw_panel(c, r, kTileRadius, focus);
    draw_glyph(c, tile.id, {r.x + 20.0f, r.cy() - 22.0f, 44.0f, 44.0f}, look_->text);
    words(c, tile.title, r.x + 76.0f, r.cy() + 10.0f, 28.0f, r.w - 90.0f, look_->text, true);
}
void Application::draw_deck(Canvas &c) const
{
    const auto &slots = deck_.slots();
    const int count = static_cast<int>(slots.size());
    for (int i = 0; i < count; ++i)
    {
        const auto &rect = slots[static_cast<std::size_t>(i)];
        const float focus = page_ == Page::launcher ? deck_.focus_on(i) : 0.0f;
        const float grow = config_.reduced_motion ? 0.0f : focus;
        c.list.push_transform(1.0f + (i == 0 ? 0.008f : 0.02f) * grow, rect.cx(), rect.cy(), 0.0f,
                              0.0f);
        if (i == 0)
        {
            draw_big_tile(c, rect, i, focus);
        }
        else if (i >= count - kDockTiles)
        {
            draw_dock_tile(c, rect, i, focus);
        }
        else
        {
            draw_grid_tile(c, rect, i, focus);
        }
        c.list.pop_transform();
    }
    if (page_ == Page::launcher)
    {
        const int focus = deck_.focus();
        deck_.draw_ring(c, look_->square ? 0.0f : focus == 0 ? kBigRadius : kTileRadius);
    }
}
void Application::draw_app_cell(Canvas &c, const Rect &r, int index, float focus) const
{
    if (index < 0 || index >= static_cast<int>(tiles_.size()))
        return;
    const auto &tile = tiles_[static_cast<std::size_t>(index)];
    draw_panel(c, r, kTileRadius, focus);
    draw_art(c, tile, {r.cx() - 46.0f, r.y + 12.0f, 92.0f, 92.0f});
    centred_name(c, tile.title, r.cx(), r.y + 132.0f, 27.0f, r.w - 24.0f, 32.0f, look_->text);
    if (!tile.subtitle.empty())
    {
        c.list.circle(r.x + r.w - 32.0f, r.y + 32.0f, 20.0f, look_->accent);
        c.list.star(r.x + r.w - 32.0f, r.y + 32.0f, 13.0f, look_->on_accent);
    }
}
void Application::draw_setup_row(Canvas &c, const Rect &r, int index, float focus) const
{
    if (index < 0 || index >= static_cast<int>(setup_rows_.size()))
        return;
    const auto &row = setup_rows_[static_cast<std::size_t>(index)];
    draw_panel(c, r, 16.0f, focus);
    const float value = row.value.empty() ? 0.0f : words_width(c, row.value, 30.0f, false) + 24.0f;
    const float room = r.w - 56.0f - std::min(value, 360.0f);
    words(c, row.title, r.x + 28.0f, r.y + 38.0f, 30.0f, room,
          row.disabled ? look_->muted : look_->text, true);
    words(c, row.subtitle, r.x + 28.0f, r.y + 73.0f, 27.0f, room, look_->muted);
    if (!row.value.empty())
        words(c, row.value, r.x + r.w - 28.0f, r.cy() + 10.0f, 30.0f, 340.0f, look_->muted, false,
              Align::right);
}
void Application::draw_list_row(Canvas &c, const Rect &row, const hui::ui::ListItem &item,
                                float focus) const
{
    draw_panel(c, row, 16.0f, focus);
    const float pad = 28.0f, size = page_ == Page::quick ? 32.0f : 30.0f;
    const bool stepper = page_ == Page::quick;
    const auto value = stepper ? sentence_case(item.value) : item.value;
    // A long input name gives way before the row's label does.
    const float value_room = std::min(words_width(c, value, size, stepper), row.w * 0.5f);
    const float title_room = row.w - 2.0f * pad - (value.empty() ? 0.0f : value_room + 90.0f);
    const Color title = item.disabled ? look_->muted : look_->text;
    if (item.subtitle.empty())
    {
        words(c, item.title, row.x + pad, row.cy() + 11.0f, size, title_room, title, true);
    }
    else
    {
        words(c, item.title, row.x + pad, row.y + 33.0f, 30.0f, title_room, title, true);
        words(c, item.subtitle, row.x + pad, row.y + 66.0f, 27.0f, title_room, look_->muted);
    }
    if (item.tag == kFavouriteRow)
    {
        c.list.circle(row.x + row.w - 44.0f, row.cy(), 20.0f, look_->accent);
        c.list.star(row.x + row.w - 44.0f, row.cy(), 13.0f, look_->on_accent);
    }
    if (value.empty())
        return;
    const float end = row.x + row.w - pad;
    if (stepper && focus > 0.5f && !item.disabled)
    {
        const float width = value_room;
        const Rect after{end - 16.0f, row.cy() - 10.0f, 16.0f, 20.0f};
        const Rect before{end - width - 66.0f, row.cy() - 10.0f, 16.0f, 20.0f};
        c.list.triangle(after, look_->muted, 0.0f, 1.5708f);
        c.list.triangle(before, look_->muted, 0.0f, -1.5708f);
        words(c, value, end - 33.0f, row.cy() + 11.0f, size, row.w * 0.5f, look_->text, true,
              Align::right);
        return;
    }
    words(c, value, end, row.cy() + 11.0f, size, row.w * 0.5f, stepper ? look_->text : look_->muted,
          stepper, Align::right);
}
void Application::draw_beam(Canvas &c, float x, float cy) const
{
    const BeamEnd end = beam_end();
    const float words_y = cy + 9.0f, gap = 28.0f, width = 146.0f;
    const float from = x + words(c, "PS5", x, words_y, 27.0f, 80.0f, look_->amber, true) + gap;
    const float dim = end == BeamEnd::none ? 0.35f : 1.0f;
    c.list.gradient_rect_h({from, cy - 2.0f, width, 4.0f}, 2.0f, look_->amber.with_alpha(dim),
                           look_->sky.with_alpha(dim));
    const Rect prism{from + width * 0.5f - 15.0f, cy - 15.0f, 30.0f, 26.0f};
    c.list.triangle(prism, look_->sheet);
    c.list.triangle(prism, look_->text, 3.0f);
    if (end != BeamEnd::none)
        c.list.circle(from + width * beam_along_, cy, 11.0f,
                      end == BeamEnd::lg ? look_->sky : look_->amber);
    words(c, "LG", from + width + gap, words_y, 27.0f, 60.0f, look_->sky, true);
}
// Only what the buttons really do here: Square on an app, Cross where there is something to select.
std::vector<hui::ui::Hint> Application::foot_hints() const
{
    using hui::ui::Hint;
    if (page_ == Page::remote)
        return {{Button::options, "Return to PS5"}};
    std::vector<Hint> hints;
    if (page_ == Page::launcher || page_ == Page::apps)
    {
        if (!tiles_.empty())
        {
            hints.push_back({Button::cross, "Open"});
            const auto &id = tiles_[static_cast<std::size_t>(tile_focus())].id;
            if (page_ == Page::apps && find_app(id) && launchable(id))
                hints.push_back({Button::square, "Favourite"});
        }
        hints.push_back({Button::options, "Setup"});
        hints.push_back({Button::circle, page_ == Page::launcher ? "Close" : "Back"});
        return hints;
    }
    if (page_ != Page::help && !(page_ == Page::tvs && snapshot_.tvs.empty()))
        hints.push_back({Button::cross, "Select"});
    hints.push_back({Button::circle, "Back"});
    hints.push_back({Button::options, page_ == Page::setup ? "Launcher" : "Setup"});
    return hints;
}
void Application::draw_foot(Canvas &c) const
{
    const auto hints = foot_hints();
    hui::ui::HintLayout layout;
    layout.cy = kFootCentre;
    const int count = static_cast<int>(hints.size());
    const float room = hui::ui::measure_hints(c.fonts, hints.data(), count, layout);
    hui::ui::draw_hints(c.list, c.fonts, glyph_style(*look_), hints.data(), count, kRight, true,
                        layout);
    draw_beam(c, kLeft, kFootCentre);
    const float at = kLeft + 310.0f;
    // Something going wrong looks wrong; connecting and first-run prompts are just quiet.
    const auto &state = snapshot_.status;
    const bool waiting = state.starts_with("Connecting") || state.starts_with("Reading") ||
                         state.starts_with("Enter");
    const Color colour = !notice_.empty()
                             ? (notice_ == failed_notice_ ? look_->error : look_->accent)
                         : ready() ? look_->text
                         : waiting ? look_->muted
                                   : look_->error;
    words(c, live_line(), at, kFootCentre + 10.0f, 29.0f, kRight - at - room - 40.0f, colour, true);
}
void Application::draw_sheet(Canvas &c) const
{
    c.list.rounded_rect({0.0f, 0.0f, 1920.0f, 1080.0f}, 0.0f,
                        Color::rgb(0x000000, look_->dark ? 0.35f : 0.15f));
    c.list.rounded_rect({1040.0f, -40.0f, 960.0f, 1160.0f}, look_->square ? 0.0f : 36.0f,
                        look_->sheet);
    const float x = 1096.0f, width = 730.0f;
    words(c, "Quick TV settings", x, 104.0f, 46.0f, width, look_->text, true);
    if (notice_.empty())
        words(c, "Left and Right change a value", x, 150.0f, 26.0f, width, look_->muted);
    else
        words(c, notice_, x, 150.0f, 26.0f, width,
              notice_ == failed_notice_ ? look_->error : look_->accent);
    list_.draw(c);
    const hui::ui::Hint hints[] = {{Button::dpad, "Change value"}, {Button::circle, "Back"}};
    hui::ui::HintLayout layout;
    layout.cy = kFootCentre;
    hui::ui::draw_hints(c.list, c.fonts, glyph_style(*look_), hints, 2, x, false, layout);
}
void Application::draw_header(Canvas &c) const
{
    std::string title, subtitle;
    switch (page_)
    {
    case Page::apps:
        title = "All apps";
        break;
    case Page::setup:
        title = "Setup";
        subtitle =
            "Enter the local IPv4 address and a long-lived token, connect, choose the TV, then "
            "save the input while the PS5 shows.";
        break;
    case Page::tvs:
        title = "Choose your TV";
        subtitle = snapshot_.tvs.empty()
                       ? "Glasshouse TVs found in Home Assistant"
                       : "Changing TV clears the previous TV's input and favourites.";
        break;
    case Page::favourites:
        title = "Favourites";
        subtitle = "Select to add or remove. L1 and R1 reorder. The launcher shows the first ones "
                   "that fit; the rest stay in All apps.";
        break;
    case Page::help:
        title = "Setup check";
        subtitle = "Connection, return shortcut and controller focus";
        break;
    case Page::remote:
        title = "Your controller is the LG remote";
        subtitle = tv() ? tv()->name : "Prism";
        break;
    case Page::quit:
        title = "Close Prism?";
        subtitle = "Remote forwarding stops when Prism closes. To leave the TV on the PS5, hold "
                   "Circle in remote mode first.";
        break;
    default:
        break;
    }
    words(c, title, kLeft, 88.0f, 46.0f, kRight - kLeft, look_->text, true);
    words(c, subtitle, kLeft, 130.0f, 27.0f, kRight - kLeft, look_->muted);
}
void Application::draw_remote(Canvas &c) const
{
    const auto current = tv() ? target(*tv(), snapshot_.states) : "";
    words(c,
          current.empty()                ? "Waiting for the TV"
          : current == config_.input_app ? "PS5 input is visible"
                                         : app_name(current),
          kLeft, 226.0f, 52.0f, kRight - kLeft, look_->sky, true);
    paragraph(c,
              "Prism stays open on the PS5 while you use an LG app. Hold Circle for 0.8 seconds to "
              "come back.",
              kLeft, 284.0f, 30.0f, 1500.0f, 44.0f, look_->muted, 2);
    const auto style = glyph_style(*look_);
    struct Control
    {
        Button button;
        const char *name;
        const char *description;
    };
    const Control controls[] = {{Button::dpad, "Move", "Navigate the LG"},
                                {Button::cross, "Select", "Open the focused item"},
                                {Button::circle, "Back", "Hold to return to PS5"},
                                {Button::square, "Play / pause", "Control media"},
                                {Button::l1, "Rewind", "R1 fast-forwards"},
                                {Button::triangle, "LG Home", "Open the TV home screen"}};
    for (int i = 0; i < 6; i++)
    {
        const float x = kLeft + static_cast<float>(i % 3) * 576.0f;
        const float y = 400.0f + static_cast<float>(i / 3) * 170.0f;
        hui::ui::draw_button(c.list, c.fonts, style, controls[i].button, x, y + 20.0f, 58.0f);
        words(c, controls[i].name, x + 108.0f, y + 12.0f, 31.0f, 410.0f, look_->text, true);
        words(c, controls[i].description, x + 108.0f, y + 55.0f, 27.0f, 410.0f, look_->muted);
    }
    if (tv() && !tv()->role("keyboard_text").empty())
        words(c, "Touchpad opens text entry for the LG keyboard", kLeft, 780.0f, 28.0f, 1700.0f,
              look_->muted);
    const auto progress = remote_.hold_progress(now_);
    if (progress > 0)
    {
        c.list.rounded_rect({kLeft, 880.0f, kRight - kLeft, 8.0f}, 4.0f, look_->raised);
        c.list.rounded_rect({kLeft, 880.0f, (kRight - kLeft) * progress, 8.0f}, 4.0f,
                            look_->accent);
    }
}
void Application::draw_help(Canvas &c) const
{
    const std::vector<std::pair<std::string, std::string>> checks = {
        {"Home Assistant", snapshot_.ready ? "Connected" : snapshot_.status},
        {"Glasshouse TV", tv() ? (tv()->usable() ? tv()->name : "Required entities are missing")
                               : "Choose a TV in Setup"},
        {"PS5 return input",
         config_.input.empty() ? "Show the PS5 input and capture it in Setup" : config_.input},
        {"Controller focus",
         has_focus_ ? "Prism has the controller" : "Press a button with Prism in the foreground"},
        {"Return shortcut", "Hold Circle for 0.8 s, or press Options in remote mode"},
        {"System menus", "PS button opens the system UI. Forwarding pauses while it has focus."},
        {"Background games",
         "Open Prism before switching to an LG app. It cannot intercept other titles."}};
    for (std::size_t i = 0; i < checks.size(); i++)
    {
        const float y = 214.0f + static_cast<float>(i) * 96.0f;
        words(c, checks[i].first, kLeft, y, 30.0f, 400.0f, look_->text, true);
        words(c, checks[i].second, 520.0f, y, 29.0f, 1300.0f, look_->muted);
    }
}
void Application::draw(Canvas &c) const
{
    if (page_ == Page::launcher || page_ == Page::quick)
    {
        draw_deck(c);
        if (page_ == Page::quick)
            draw_sheet(c);
        else
            draw_foot(c);
        return;
    }
    draw_header(c);
    if (page_ == Page::apps && tiles_.empty())
    {
        words(c, "No apps to show yet", kLeft, 240.0f, 42.0f, 1500.0f, look_->text, true);
        if (ready())
            paragraph(c,
                      "Turn on the LG and check that Glasshouse exposes app_ids on Launch App. "
                      "You can return to Setup with Options.",
                      kLeft, 298.0f, 30.0f, 1400.0f, 44.0f, look_->muted, 3);
    }
    else if (page_ == Page::apps || page_ == Page::setup)
        grid_.draw(c);
    else if (page_ == Page::remote)
        draw_remote(c);
    else if (page_ == Page::help)
        draw_help(c);
    else if (page_ == Page::tvs && snapshot_.tvs.empty())
    {
        words(c, "No TV found", kLeft, 240.0f, 42.0f, 1500.0f, look_->text, true);
        paragraph(
            c,
            "Connect to Home Assistant, then enable Glasshouse's Launch App, Active App, Input "
            "Source and Remote button entities.",
            kLeft, 298.0f, 30.0f, 1400.0f, 44.0f, look_->muted, 3);
    }
    else
        list_.draw(c);
    draw_foot(c);
}
} // namespace prism
