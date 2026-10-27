// Prism - Foreground remote gesture policy; no system-wide input hooks.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/input.hpp"
#include <string>
#include <vector>

namespace prism
{
struct RemoteActions
{
    std::vector<std::string> buttons;
    bool return_input = false;
};
class Remote
{
  public:
    RemoteActions update(const hui::InputFrame &input, std::int64_t now, bool enabled);
    void reset();
    float hold_progress(std::int64_t now) const;

  private:
    std::int64_t circle_at_ = 0, next_arrow_ = 0;
    bool circle_ = false, returned_ = false, blocked_ = true;
};
} // namespace prism
