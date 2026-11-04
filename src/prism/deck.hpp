// Prism - The launcher's deck: where its tiles go, and one focus that glides between them.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ui/components/component.hpp"
#include <string>
#include <vector>

namespace prism
{
// One big tile, a grid of apps ending in All apps, a dock of the TV's own places and one line of
// status and hints, all on one 1080p screen with nothing to scroll.
constexpr int kGridColumns = 4, kGridRows = 2;
// All apps always takes the grid's last place, so it never moves; the apps get the others.
constexpr int kGridApps = kGridColumns * kGridRows - 1;
constexpr float kFootCentre = 985.0f;
constexpr float kBigRadius = 28.0f, kTileRadius = 22.0f;
// The grid's apps in order: favourites, then recently used, then the rest of the TV's apps, each
// once and never the big tile's.
std::vector<std::string> grid_order(const std::vector<std::string> &favourites,
                                    const std::vector<std::string> &recent,
                                    const std::vector<std::string> &catalog,
                                    const std::string &big_tile);
hui::gfx::Rect big_tile_rect();
hui::gfx::Rect grid_rect(int cell);
hui::gfx::Rect dock_rect(int index, int count);
class Deck
{
  public:
    hui::ui::ComponentStyle style;
    void set_slots(std::vector<hui::gfx::Rect> slots);
    const std::vector<hui::gfx::Rect> &slots() const
    {
        return slots_;
    }
    int focus() const
    {
        return focus_;
    }
    void set_focus(int index, bool snap = true);
    hui::ui::Event handle(const hui::InputFrame &input, hui::ui::Feedback &feedback);
    void update(float dt);
    void draw_ring(hui::ui::Canvas &canvas, float radius) const;
    // How much of a slot the focus covers, 0..1.
    float focus_on(int index) const;
    // The nearest slot in a direction, or -1 at an edge. Chosen by distance, not by index, so it
    // works across the deck's three differently sized parts.
    int step(hui::Direction direction) const;

  private:
    std::vector<hui::gfx::Rect> slots_;
    int focus_ = 0;
    hui::ui::Highlight ring_;
};
} // namespace prism
