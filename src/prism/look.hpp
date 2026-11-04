// Prism - The looks Prism can wear, and the colour and text rules that go with them.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gfx/backdrop_spec.hpp"
#include "gfx/draw_list.hpp"
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace prism
{
using hui::gfx::Color;
struct Look
{
    const char *id, *name;
    bool washes; // the ground takes a soft light in the focused tile's colour
    hui::gfx::BackdropSpec ground;
    bool scanlines; // CRT lines over the finished frame
    bool square;    // no rounded corners
    bool dark;      // which controller glyphs suit the page
    Color panel, raised, sheet;
    Color edge; // a hairline round tiles; alpha 0 for none
    Color text, muted, accent, on_accent, warn, error, sky, amber;
};
std::span<const Look> looks();
// The saved look; unknown ids read as the default.
const Look &look_by_id(const std::string &id);
// The look after this one in Setup's row, wrapping round.
const Look &next_look(const Look &look);
// What to wear now: Dusk once the sun is known to be down, when Setup asks for that.
const Look &worn_look(const std::string &id, bool dusk_after_sunset, bool sun_down);
// The light for an app's colour; alpha is how strongly to draw it. Neutral colours share one
// quiet wash, and bright ones are drawn fainter, so every word on the ground stays readable.
Color wash(Color app);
// `top`, with its alpha, over an opaque colour.
Color over(Color top, Color under);
double luminance(Color color);
double contrast(Color a, Color b);
// A string split into runs of digits and of everything else: digits are drawn in Lexend, since
// Atkinson's slashed zero reads as "Ø" from the sofa.
struct TextRun
{
    std::string_view text;
    bool digits;
};
std::vector<TextRun> digit_runs(std::string_view text);
} // namespace prism
