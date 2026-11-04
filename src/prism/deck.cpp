// Prism - The launcher's deck: where its tiles go, and one focus that glides between them.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prism/deck.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace prism
{
using hui::Direction;
using hui::gfx::Rect;
namespace
{
constexpr float kLeft = 96.0f, kTop = 56.0f, kWidth = 1728.0f, kGap = 24.0f;
constexpr float kBigWidth = 600.0f, kDeckHeight = 720.0f, kDockTop = 800.0f, kDockHeight = 130.0f;
} // namespace
Rect big_tile_rect()
{
    return {kLeft, kTop, kBigWidth, kDeckHeight};
}
Rect grid_rect(int cell)
{
    const float left = kLeft + kBigWidth + 28.0f;
    const float width = (kLeft + kWidth - left - kGap * (kGridColumns - 1)) / kGridColumns;
    const float height = (kDeckHeight - kGap * (kGridRows - 1)) / kGridRows;
    return {left + static_cast<float>(cell % kGridColumns) * (width + kGap),
            kTop + static_cast<float>(cell / kGridColumns) * (height + kGap), width, height};
}
Rect dock_rect(int index, int count)
{
    const float width = (kWidth - kGap * static_cast<float>(count - 1)) / static_cast<float>(count);
    return {kLeft + static_cast<float>(index) * (width + kGap), kDockTop, width, kDockHeight};
}
void Deck::set_slots(std::vector<Rect> slots)
{
    const bool first = slots_.empty();
    slots_ = std::move(slots);
    set_focus(focus_, first);
}
void Deck::set_focus(int index, bool snap)
{
    if (slots_.empty())
    {
        focus_ = 0;
        return;
    }
    focus_ = std::clamp(index, 0, static_cast<int>(slots_.size()) - 1);
    const Rect &target = slots_[static_cast<std::size_t>(focus_)];
    if (snap)
        ring_.snap(target);
    else
        ring_.target(target);
}
int Deck::step(Direction direction) const
{
    if (slots_.empty())
        return -1;
    const Rect &from = slots_[static_cast<std::size_t>(focus_)];
    const bool sideways = direction == Direction::left || direction == Direction::right;
    int best = -1;
    float best_score = std::numeric_limits<float>::max();
    for (int i = 0; i < static_cast<int>(slots_.size()); ++i)
    {
        if (i == focus_)
            continue;
        const Rect &to = slots_[static_cast<std::size_t>(i)];
        float ahead = 0.0f;
        switch (direction)
        {
        case Direction::left:
            ahead = from.x - (to.x + to.w);
            break;
        case Direction::right:
            ahead = to.x - (from.x + from.w);
            break;
        case Direction::up:
            ahead = from.y - (to.y + to.h);
            break;
        case Direction::down:
            ahead = to.y - (from.y + from.h);
            break;
        default:
            return -1;
        }
        if (ahead < -1.0f)
            continue;
        const float across =
            sideways ? std::fabs(to.cy() - from.cy()) : std::fabs(to.cx() - from.cx());
        const float score = ahead + across * 2.0f;
        if (score < best_score)
        {
            best = i;
            best_score = score;
        }
    }
    return best;
}
hui::ui::Event Deck::handle(const hui::InputFrame &input, hui::ui::Feedback &feedback)
{
    using hui::ui::Event;
    if (slots_.empty())
        return Event::none;
    const float x = slots_[static_cast<std::size_t>(focus_)].cx();
    if (input.nav != Direction::none)
    {
        const int next = step(input.nav);
        if (next < 0)
            return hui::ui::refuse(feedback, style, input, ring_.refusal(), x);
        set_focus(next, false);
        hui::ui::play_cue(feedback, style, style.sounds.move,
                          slots_[static_cast<std::size_t>(next)].cx());
        return Event::moved;
    }
    if (input.is_pressed(hui::Action::confirm))
    {
        hui::ui::play_cue(feedback, style, style.sounds.activate, x);
        return Event::activated;
    }
    return Event::none;
}
void Deck::update(float dt)
{
    if (!slots_.empty())
        ring_.target(slots_[static_cast<std::size_t>(focus_)]);
    ring_.update(dt, style);
}
void Deck::draw_ring(hui::ui::Canvas &canvas, float radius) const
{
    if (slots_.empty())
        return;
    hui::ui::HighlightStyle look;
    look.kind = hui::ui::HighlightKind::ring;
    look.radius = radius;
    ring_.draw(canvas, style, look);
}
float Deck::focus_on(int index) const
{
    if (index < 0 || index >= static_cast<int>(slots_.size()))
        return 0.0f;
    return ring_.coverage(slots_[static_cast<std::size_t>(index)]);
}
} // namespace prism
