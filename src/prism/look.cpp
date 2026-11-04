// Prism - The looks Prism can wear, and the colour and text rules that go with them.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prism/look.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace prism
{
namespace
{
using hui::gfx::BackdropMode;
using hui::gfx::BackdropSpec;
constexpr Color rgb(std::uint32_t hex, float alpha = 1.0f)
{
    return Color::rgb(hex, alpha);
}
BackdropSpec ground(BackdropMode mode, std::uint32_t c0, std::uint32_t c1, std::uint32_t c2 = 0,
                    std::uint32_t c3 = 0)
{
    BackdropSpec spec;
    spec.mode = mode;
    spec.colors[0] = rgb(c0);
    spec.colors[1] = rgb(c1);
    spec.colors[2] = rgb(c2);
    spec.colors[3] = rgb(c3);
    return spec;
}
// The brightest a wash may make the ground, as WCAG luminance.
constexpr double kWashCeiling = 0.085;
constexpr float kWashStrength = 0.5f;
// For a tile with no colour of its own, or a white, black or grey one.
constexpr Color kNeutralWash = rgb(0x2f6f8f);
const std::array<Look, 6> kLooks = {{
    {"refract", "Refract", true, ground(BackdropMode::gradient, 0x0e0f12, 0x0e0f12), false, false,
     true, rgb(0xffffff, 0.075f), rgb(0xffffff, 0.14f), rgb(0x17191e), rgb(0, 0.0f), rgb(0xf8f1e8),
     rgb(0xd8c9b8), rgb(0xffffff), rgb(0x0e0f12), rgb(0xffb27a), rgb(0xff8a80), rgb(0x80d8ff),
     rgb(0xff9d5c)},
    {"oled", "OLED Night", false, ground(BackdropMode::gradient, 0x000000, 0x000000), false, false,
     true, rgb(0x121317), rgb(0x1d1f25), rgb(0x0d0e11), rgb(0, 0.0f), rgb(0xf4f2ee), rgb(0xb9b4ac),
     rgb(0x80d8ff), rgb(0x04222e), rgb(0xff9d5c), rgb(0xff8a80), rgb(0x80d8ff), rgb(0xff9d5c)},
    {"dusk", "Dusk", false, ground(BackdropMode::gradient, 0x1b0f18, 0x351a12), false, false, true,
     rgb(0x3a221c), rgb(0x4a2d25), rgb(0x2a1714), rgb(0, 0.0f), rgb(0xf8ece0), rgb(0xd9bfb0),
     rgb(0xff9d5c), rgb(0x2a1206), rgb(0xffc36b), rgb(0xff8a80), rgb(0x80d8ff), rgb(0xff9d5c)},
    {"daylight", "Daylight", false, ground(BackdropMode::gradient, 0xebe8e1, 0xebe8e1), false,
     false, false, rgb(0xffffff), rgb(0xdfdbd2), rgb(0xf6f4ef), rgb(0, 0.0f), rgb(0x16181d),
     rgb(0x5d5b55), rgb(0x1d8ac0), rgb(0xffffff), rgb(0x9e4a1a), rgb(0xb3261e), rgb(0x1d8ac0),
     rgb(0xd4632a)},
    {"aurora", "Aurora", false,
     ground(BackdropMode::aurora, 0x070b14, 0x0b1426, 0x168a68, 0x4a2fa8), false, false, true,
     rgb(0x070b14, 0.55f), rgb(0xffffff, 0.14f), rgb(0x0b1120), rgb(0xffffff, 0.2f), rgb(0xf2f7ff),
     rgb(0xc4d0e0), rgb(0x9fffe0), rgb(0x04221a), rgb(0xffc36b), rgb(0xff8a80), rgb(0x80d8ff),
     rgb(0xff9d5c)},
    {"phosphor", "Phosphor", false, ground(BackdropMode::phosphor, 0x06110a, 0x0e2a18), true, true,
     true, rgb(0x0a1b11), rgb(0x0f2a1a), rgb(0x081509), rgb(0x2a9d5f), rgb(0xa8ffcb), rgb(0x7fd8a4),
     rgb(0x3bff8f), rgb(0x03140a), rgb(0xffe08a), rgb(0xff9a8a), rgb(0x80d8ff), rgb(0xff9d5c)},
}};
double linear(float channel)
{
    return channel <= 0.03928f ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
}
} // namespace
std::span<const Look> looks()
{
    return kLooks;
}
const Look &look_by_id(const std::string &id)
{
    for (const auto &look : kLooks)
        if (id == look.id)
            return look;
    return kLooks[0];
}
const Look &next_look(const Look &look)
{
    const auto at = static_cast<std::size_t>(&look - kLooks.data());
    return kLooks[(at + 1) % kLooks.size()];
}
const Look &worn_look(const std::string &id, bool dusk_after_sunset, bool sun_down)
{
    return dusk_after_sunset && sun_down ? look_by_id("dusk") : look_by_id(id);
}
Color wash(Color app)
{
    const float high = std::max({app.r, app.g, app.b}), low = std::min({app.r, app.g, app.b});
    const float saturation = high > 0.0f ? (high - low) / high : 0.0f;
    Color light = kNeutralWash;
    if (saturation >= 0.25f && high >= 0.2f)
    {
        // Keep the hue; hold saturation and value down so the light is never neon.
        const float value = std::min(high, 0.8f);
        const float keep = std::min(saturation, 0.85f);
        const float floor = value * (1.0f - keep);
        const auto channel = [&](float c)
        { return floor + (c - low) / std::max(high - low, 0.001f) * (value - floor); };
        light = {channel(app.r), channel(app.g), channel(app.b), 1.0f};
    }
    light.a = std::min(kWashStrength,
                       static_cast<float>(kWashCeiling / std::max(luminance(light), 0.001)));
    return light;
}
Color over(Color top, Color under)
{
    return {top.r * top.a + under.r * (1.0f - top.a), top.g * top.a + under.g * (1.0f - top.a),
            top.b * top.a + under.b * (1.0f - top.a), 1.0f};
}
double luminance(Color color)
{
    return 0.2126 * linear(color.r) + 0.7152 * linear(color.g) + 0.0722 * linear(color.b);
}
double contrast(Color a, Color b)
{
    const double la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}
std::vector<TextRun> digit_runs(std::string_view text)
{
    std::vector<TextRun> runs;
    const auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
    std::size_t start = 0;
    while (start < text.size())
    {
        const bool digits = is_digit(text[start]);
        std::size_t end = start;
        while (end < text.size() && is_digit(text[end]) == digits)
            ++end;
        runs.push_back({text.substr(start, end - start), digits});
        start = end;
    }
    return runs;
}
} // namespace prism
