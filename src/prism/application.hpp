// Prism - Native controller application and screen state.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "prism/client.hpp"
#include "prism/deck.hpp"
#include "prism/look.hpp"
#include "prism/remote.hpp"
#include "platform/ps5/keyboard.hpp"
#include "ui/components/grid.hpp"
#include "ui/components/list.hpp"
#include "ui/glyphs.hpp"
#include <functional>
#include <optional>
#include <set>

namespace prism
{
enum class Page
{
    launcher,
    apps,
    quick,
    setup,
    tvs,
    favourites,
    help,
    remote,
    quit
};
class Application
{
  public:
    explicit Application(Config config);
    std::function<std::uint64_t(Command)> send;
    std::function<void(const Config &)> save, reconnect;
    std::function<void()> cancel_transient;
    std::function<void(const std::string &)> request_icon;
    void receive(Snapshot snapshot);
    void update(const hui::InputFrame &input, float dt, std::int64_t now,
                hui::ui::Feedback &feedback);
    void draw(hui::ui::Canvas &canvas) const;
    // What the renderer paints behind and over the frame's draw list.
    hui::gfx::BackdropSpec ground(float time) const;
    hui::gfx::BackdropSpec overlay() const;
    const Look &look() const
    {
        return *look_;
    }
    void open(Page page);
    void keyboard_result(const hui::ps5::KeyboardOutcome &result);
    std::optional<hui::ps5::KeyboardOptions> take_keyboard();
    void icon(const std::string &url, std::uint32_t texture);
    void storage_status(std::string status)
    {
        storage_status_ = std::move(status);
    }
    bool quitting() const
    {
        return quitting_;
    }
    bool remote_mode() const
    {
        return page_ == Page::remote;
    }
    const Config &config() const
    {
        return config_;
    }
    Page page() const
    {
        return page_;
    }
    std::string notice() const
    {
        return notice_;
    }
    // Where the controller goes, in words: the foot of the screen, beside the beam.
    std::string live_line() const;
    Json diagnostic() const;

  private:
    struct Tile
    {
        std::string id, title, subtitle, icon, color;
    };
    enum class BeamEnd
    {
        none,
        ps5,
        lg
    };
    enum class SetupRow
    {
        address,
        port,
        token,
        connect,
        tv,
        input,
        favourites,
        theme,
        dusk,
        reduced_motion,
        vibration,
        diagnostics,
        help,
        disconnect,
        close
    };
    struct Edit
    {
        std::string role, entity, value;
        std::int64_t due = 0;
        std::uint64_t ticket = 0;
        bool accepted = false;
    };
    // A list row's tag on the Favourites page: this app is one.
    static constexpr int kFavouriteRow = 1;
    void rebuild();
    void fail(std::string message);
    void disarm_disconnect();
    void build_deck();
    void setup_view();
    void arrange_page();
    void wear_look();
    void ease_light(float dt);
    int tile_focus() const;
    BeamEnd beam_end() const;
    void draw_deck(hui::ui::Canvas &canvas) const;
    void draw_big_tile(hui::ui::Canvas &canvas, const hui::gfx::Rect &bounds, int index,
                       float focus) const;
    void draw_grid_tile(hui::ui::Canvas &canvas, const hui::gfx::Rect &bounds, int index,
                        float focus) const;
    void draw_dock_tile(hui::ui::Canvas &canvas, const hui::gfx::Rect &bounds, int index,
                        float focus) const;
    void draw_app_cell(hui::ui::Canvas &canvas, const hui::gfx::Rect &bounds, int index,
                       float focus) const;
    void draw_setup_row(hui::ui::Canvas &canvas, const hui::gfx::Rect &bounds, int index,
                        float focus) const;
    void draw_list_row(hui::ui::Canvas &canvas, const hui::gfx::Rect &row,
                       const hui::ui::ListItem &item, float focus) const;
    void draw_art(hui::ui::Canvas &canvas, const Tile &tile, const hui::gfx::Rect &art) const;
    void draw_panel(hui::ui::Canvas &canvas, const hui::gfx::Rect &bounds, float radius,
                    float focus = 0.0f) const;
    void draw_header(hui::ui::Canvas &canvas) const;
    void draw_sheet(hui::ui::Canvas &canvas) const;
    void draw_remote(hui::ui::Canvas &canvas) const;
    void draw_help(hui::ui::Canvas &canvas) const;
    void draw_foot(hui::ui::Canvas &canvas) const;
    void draw_beam(hui::ui::Canvas &canvas, float x, float cy) const;
    void activate(int index);
    void launch(const std::string &id);
    void return_to_ps5();
    void change_setting(int direction);
    void flush_setting();
    void persist();
    void request_keyboard(int kind);
    std::uint64_t dispatch(Command command);
    const Tv *tv() const;
    const App *find_app(const std::string &id) const;
    std::string app_name(const std::string &id) const;
    std::vector<hui::ui::Hint> foot_hints() const;
    void follow_lg_surface(const std::string &current, bool generation_changed);
    bool ready() const;
    Config config_;
    Snapshot snapshot_;
    std::vector<App> apps_;
    std::vector<Tile> tiles_;
    std::map<std::string, std::uint32_t> textures_;
    hui::ui::GridView grid_;
    hui::ui::ListView list_;
    Deck deck_;
    std::vector<hui::ui::ListItem> setup_rows_;
    const Look *look_ = nullptr;
    bool sun_down_ = false, disconnect_armed_ = false;
    // The ground's light (Refract) and the beam's dot, each easing to where it now belongs.
    Color light_{0.0f, 0.0f, 0.0f, 0.0f};
    float light_x_ = 0.25f, light_y_ = 0.35f, beam_along_ = 0.0f;
    Page page_ = Page::launcher;
    Remote remote_;
    std::optional<hui::ps5::KeyboardOptions> keyboard_;
    int keyboard_kind_ = 0;
    std::optional<Edit> edit_;
    std::map<std::string, Edit> pending_edits_;
    std::set<std::uint64_t> handled_results_;
    std::string notice_, failed_notice_, storage_status_, last_target_, launch_target_;
    std::int64_t now_ = 0, launch_deadline_ = 0, return_deadline_ = 0, return_next_ = 0;
    std::uint64_t launch_ticket_ = 0, return_ticket_ = 0, generation_ = 0, presses_ = 0,
                  forwarded_ = 0;
    int return_backs_ = 0;
    bool quitting_ = false, returning_ = false, has_focus_ = false, local_blocked_ = false;
    bool seen_remote_ = false, cued_ = false, lg_surface_ = false, lg_baseline_ = false;
};
} // namespace prism
