// Prism - TV-safe native screens, preserving Prism's dark palette and cyan focus.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prism/application.hpp"
#include "ui/glyphs.hpp"
#include <cstdlib>

namespace prism
{
using hui::gfx::Color;
using hui::gfx::Rect;
using hui::ui::Button;
namespace
{
constexpr auto paper = Color::rgb(0xf5f4ef), muted = Color::rgb(0xb9bec5),
               accent = Color::rgb(0x7bdafa), surface = Color::rgb(0x181b20);
void label(hui::ui::Canvas &c, const std::string &s, float x, float y, float size, float width,
           Color color = paper, bool bold = false)
{
    const auto &font = bold ? c.fonts.semibold : c.fonts.regular;
    hui::ui::text(c.list, font, font.font->fit(s, size, width), x, y, size, color);
}
} // namespace
void Application::setup_view()
{
    grid_.content = [this](hui::ui::Canvas &c, const Rect &r, const hui::ui::CardItem &, int index,
                           float) { draw_tile(c, r, index); };
}
void Application::draw_tile(hui::ui::Canvas &c, const Rect &r, int index) const
{
    if (index < 0 || index >= static_cast<int>(tiles_.size()))
        return;
    const auto &tile = tiles_[static_cast<std::size_t>(index)];
    c.list.rounded_rect(r, 20, surface);
    Rect icon{r.x + 24, r.y + 24, 90, 90};
    auto it = textures_.find(tile.icon);
    if (it != textures_.end())
        c.list.image(it->second, icon, hui::gfx::kFullUv, paper, 14);
    else if (tile.id.starts_with("@"))
    {
        const auto color = accent;
        const float x = icon.x + 20, y = icon.y + 18;
        if (tile.id == "@apps")
        {
            for (int i = 0; i < 4; i++)
                c.list.bordered_rect({x + (i % 2) * 28.0f, y + (i / 2) * 28.0f, 20, 20}, 3,
                                     Color::rgb(0x181b20), 3, color);
        }
        else if (tile.id == "@home")
        {
            c.list.line(x, y + 25, x + 24, y + 3, 4, color);
            c.list.line(x + 24, y + 3, x + 48, y + 25, 4, color);
            c.list.bordered_rect({x + 8, y + 25, 32, 30}, 2, surface, 3, color);
        }
        else if (tile.id == "@remote")
        {
            c.list.rounded_rect({x + 17, y, 18, 56}, 4, color);
            c.list.rounded_rect({x - 2, y + 19, 56, 18}, 4, color);
        }
        else
        {
            for (int i = 0; i < 3; i++)
            {
                c.list.line(x, y + i * 23.0f, x + 54, y + i * 23.0f, 3, color);
                c.list.circle(x + (i % 2 ? 38 : 14), y + i * 23.0f, 6, color);
            }
        }
    }
    else
    {
        Color bg = Color::rgb(0x28343d);
        if (tile.color.size() == 7 && tile.color[0] == '#')
        {
            char *end = nullptr;
            auto rgb = std::strtoul(tile.color.c_str() + 1, &end, 16);
            if (end && !*end)
                bg = Color::rgb(static_cast<std::uint32_t>(rgb));
        }
        c.list.rounded_rect(icon, 16, bg);
        auto initial = tile.title.empty() ? "?" : tile.title.substr(0, 1);
        const float luminance = bg.r * .2126f + bg.g * .7152f + bg.b * .0722f;
        label(c, initial, icon.x + 28, icon.y + 62, 50, 60,
              luminance > .55f ? Color::rgb(0x101214) : paper, true);
    }
    label(c, tile.title, r.x + 24, r.y + 160, 32, r.w - 48, paper, true);
    label(c, tile.subtitle, r.x + 24, r.y + 201, 25, r.w - 48,
          tile.subtitle == "Continue on the LG" ? accent : muted);
}
void Application::draw(hui::ui::Canvas &c) const
{
    c.list.rounded_rect({0, 0, 1920, 1080}, 0, Color::rgb(0x0d0f10));
    std::string title = "Prism", subtitle;
    switch (page_)
    {
    case Page::launcher:
        subtitle = tv() ? tv()->name + " · PS5 on " +
                              (config_.input.empty() ? "an input to choose" : config_.input)
                        : "Your LG apps, with the controller already in your hands";
        break;
    case Page::apps:
        title = "All LG apps";
        subtitle = "Select an app to open it on the LG. Square adds a favourite.";
        break;
    case Page::setup:
        title = "Setup";
        subtitle = "One connection. One TV. A way back to your PS5.";
        break;
    case Page::quick:
        title = "Quick TV settings";
        subtitle = "Left and Right change a value. It is sent after half a second.";
        break;
    case Page::tvs:
        title = "Choose your TV";
        subtitle = "Glasshouse TVs found in Home Assistant";
        break;
    case Page::favourites:
        title = "Favourites";
        subtitle = "Select to add or remove. L1 and R1 reorder your favourites.";
        break;
    case Page::help:
        title = "Ready for the LG";
        subtitle = "Setup check and controller guide";
        break;
    case Page::remote:
        title = "Your controller is the LG remote";
        subtitle = tv() ? tv()->name : "Prism";
        break;
    case Page::quit:
        title = "Close Prism?";
        subtitle = "Remote forwarding stops when the application closes.";
        break;
    }
    label(c, title, 96, 129, 58, 1530, paper, true);
    label(c, subtitle, 96, 188, 30, 1700, muted);
    c.list.circle(1808, 108, 9, snapshot_.ready ? accent : Color::rgb(0xe6b86b));
    if (page_ == Page::launcher || page_ == Page::apps)
    {
        if (page_ == Page::apps && tiles_.empty())
        {
            label(c, "No apps to show yet", 108, 342, 42, 1500, paper, true);
            hui::ui::paragraph(c.list, c.fonts.regular,
                               ready() ? "Turn on the LG and check that Glasshouse exposes app_ids "
                                         "on Launch App. You can return to Setup with Options."
                                       : snapshot_.status,
                               108, 400, 30, 1400, 44, muted, 3);
        }
        else
            grid_.draw(c);
    }
    else if (page_ == Page::remote)
    {
        const auto current = tv() ? target(*tv(), snapshot_.states) : "";
        label(c,
              current.empty()                ? "Waiting for the TV"
              : current == config_.input_app ? "PS5 input is visible"
                                             : app_name(current),
              96, 326, 52, 1660, accent, true);
        hui::ui::paragraph(c.list, c.fonts.regular,
                           "Prism stays open on the PS5 while you use an LG app. Hold Circle for "
                           "0.8 seconds to come back.",
                           96, 390, 32, 1450, 46, muted, 2);
        auto style = hui::ui::GlyphStyle::dark();
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
            float x = 96 + (i % 3) * 576.0f, y = 535 + (i / 3) * 158.0f;
            hui::ui::draw_button(c.list, c.fonts, style, controls[i].button, x, y + 20, 58);
            label(c, controls[i].name, x + 108, y + 12, 31, 410, paper, true);
            label(c, controls[i].description, x + 108, y + 55, 27, 410, muted);
        }
        if (tv() && !tv()->role("keyboard_text").empty())
            label(c, "Touchpad opens text entry for the LG keyboard", 96, 859, 28, 1700, muted);
        auto progress = remote_.hold_progress(now_);
        if (progress > 0)
        {
            c.list.rounded_rect({96, 890, 1728, 8}, 4, surface);
            c.list.rounded_rect({96, 890, 1728 * progress, 8}, 4, accent);
        }
    }
    else if (page_ == Page::help)
    {
        const std::vector<std::pair<std::string, std::string>> checks = {
            {"Home Assistant", snapshot_.ready ? "Connected" : snapshot_.status},
            {"Glasshouse TV", tv() ? (tv()->usable() ? tv()->name : "Required entities are missing")
                                   : "Choose a TV in Setup"},
            {"PS5 return input",
             config_.input.empty() ? "Show the PS5 input and capture it in Setup" : config_.input},
            {"Controller focus", has_focus_ ? "Prism has the controller"
                                            : "Press a button with Prism in the foreground"},
            {"Return shortcut", "Hold Circle for 0.8 s, or press Options in remote mode"},
            {"System menus",
             "PS button opens the system UI. Forwarding pauses while it has focus."},
            {"Background games",
             "Open Prism before switching to an LG app. It cannot intercept other titles."}};
        for (std::size_t i = 0; i < checks.size(); i++)
        {
            float y = 285 + static_cast<float>(i) * 83;
            label(c, checks[i].first, 96, y, 29, 390, paper, true);
            label(c, checks[i].second, 520, y, 28, 1300, muted);
        }
    }
    else
    {
        list_.draw(c);
        std::string heading = "On this PS5",
                    body = "Settings stay on this console. Home Assistant carries your commands to "
                           "Glasshouse on the LG.";
        if (page_ == Page::setup)
        {
            heading = "Start here";
            body = "Enter Home Assistant's local IPv4 address and token, then connect. Choose the "
                   "TV and save the input while the PS5 is visible.\n\nUse a long-lived access "
                   "token from your Home Assistant profile. Local HTTP only.";
        }
        if (page_ == Page::quick)
        {
            heading = "Small changes, live";
            body = "Values come from your TV. Missing entities are shown as unavailable.\n\nSound "
                   "output and screen-off are left out, as in Prism for Android TV.";
        }
        if (page_ == Page::favourites)
        {
            heading = "Your order";
            body = "Favourites appear on the launcher in the order shown. Use L1 and R1 to move "
                   "the selected favourite.\n\nRecent apps are remembered on this PS5.";
        }
        if (page_ == Page::tvs)
        {
            heading = snapshot_.tvs.empty() ? "No TV found" : "Choose one TV";
            body =
                snapshot_.tvs.empty()
                    ? "Connect to Home Assistant, then enable Glasshouse's Launch App, Active App, "
                      "Input Source and Remote button entities."
                    : "Entity names can change in Home Assistant. Prism follows their Glasshouse "
                      "roles.\n\nChanging TV clears the previous TV's input and favourites.";
        }
        if (page_ == Page::quit)
        {
            heading = "Before you go";
            body = "Use hold-Circle in remote mode to bring the TV back to the PS5 before closing "
                   "Prism.";
        }
        label(c, heading, 1256, 300, 34, 560, paper, true);
        hui::ui::paragraph(c.list, c.fonts.regular, body, 1256, 354, 28, 540, 41, muted, 12);
        if (page_ == Page::setup && !storage_status_.empty())
            label(c, storage_status_, 1256, 858, 26, 550, muted);
    }
    std::string status = notice_.empty() ? snapshot_.status : notice_;
    label(c, status, 96, 952, 26, 1728, notice_.empty() ? muted : accent);
    std::vector<hui::ui::Hint> hints;
    if (page_ == Page::remote)
        hints = {{Button::circle, "Hold to return"}, {Button::options, "Return to PS5"}};
    else if (page_ == Page::launcher || page_ == Page::apps)
        hints = {{Button::cross, "Open"},
                 {Button::square, "Favourite"},
                 {Button::options, "Setup"},
                 {Button::circle, page_ == Page::launcher ? "Close" : "Back"}};
    else if (page_ == Page::quick)
        hints = {
            {Button::dpad, "Change value"}, {Button::circle, "Back"}, {Button::options, "Setup"}};
    else
        hints = {
            {Button::cross, "Select"}, {Button::circle, "Back"}, {Button::options, "Launcher"}};
    hui::ui::draw_hints(c.list, c.fonts, hui::ui::GlyphStyle::dark(), hints.data(),
                        static_cast<int>(hints.size()), 96, false);
}
} // namespace prism
