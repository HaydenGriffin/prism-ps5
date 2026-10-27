// Prism - Tap Circle goes back; hold Circle returns without sending a stray Back.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prism/remote.hpp"
#include <algorithm>

namespace prism
{
void Remote::reset()
{
    circle_ = false;
    returned_ = false;
    blocked_ = true;
    next_arrow_ = 0;
}
float Remote::hold_progress(std::int64_t now) const
{
    return circle_ ? std::clamp(static_cast<float>(now - circle_at_) / 800000.0f, 0.0f, 1.0f) : 0;
}
RemoteActions Remote::update(const hui::InputFrame &in, std::int64_t now, bool enabled)
{
    RemoteActions out;
    if (!enabled || !in.connected || in.focus_lost)
    {
        reset();
        return out;
    }
    if (blocked_)
    {
        if (in.held == 0 && in.pressed == 0)
            blocked_ = false;
        return out;
    }
    using hui::Action;
    if (in.is_pressed(Action::back))
    {
        circle_ = true;
        returned_ = false;
        circle_at_ = now;
    }
    if (circle_ && !returned_ && in.is_held(Action::back) && now - circle_at_ >= 800000)
    {
        out.return_input = true;
        returned_ = true;
    }
    if (circle_ && !in.is_held(Action::back))
    {
        if (!returned_)
            out.buttons.push_back("remote_back");
        circle_ = false;
    }
    if (in.nav != hui::Direction::none && now >= next_arrow_)
    {
        switch (in.nav)
        {
        case hui::Direction::up:
            out.buttons.push_back("remote_up");
            break;
        case hui::Direction::down:
            out.buttons.push_back("remote_down");
            break;
        case hui::Direction::left:
            out.buttons.push_back("remote_left");
            break;
        case hui::Direction::right:
            out.buttons.push_back("remote_right");
            break;
        default:
            break;
        }
        next_arrow_ = now + 150000;
    }
    for (const auto &[action, key] :
         std::vector<std::pair<Action, const char *>>{{Action::confirm, "remote_ok"},
                                                      {Action::west, "play_pause"},
                                                      {Action::page_prev, "rewind"},
                                                      {Action::page_next, "fast_forward"},
                                                      {Action::north, "remote_home"}})
        if (in.is_pressed(action))
            out.buttons.push_back(key);
    if (in.is_pressed(Action::menu))
        out.return_input = true;
    if (out.return_input)
        out.buttons.clear();
    return out;
}
} // namespace prism
