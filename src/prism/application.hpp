// Prism - Native controller application and screen state.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "prism/client.hpp"
#include "prism/remote.hpp"
#include "platform/ps5/keyboard.hpp"
#include "ui/components/grid.hpp"
#include "ui/components/list.hpp"
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
    Json diagnostic() const;

  private:
    struct Tile
    {
        std::string id, title, subtitle, icon, color;
    };
    struct Edit
    {
        std::string role, entity, value;
        std::int64_t due = 0;
        std::uint64_t ticket = 0;
        bool accepted = false;
    };
    void rebuild();
    void setup_view();
    void draw_tile(hui::ui::Canvas &canvas, const hui::gfx::Rect &bounds, int index) const;
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
    void follow_lg_surface(const std::string &current, bool generation_changed);
    bool ready() const;
    Config config_;
    Snapshot snapshot_;
    std::vector<App> apps_;
    std::vector<Tile> tiles_;
    std::map<std::string, std::uint32_t> textures_;
    hui::ui::GridView grid_;
    hui::ui::ListView list_;
    Page page_ = Page::launcher;
    Remote remote_;
    std::optional<hui::ps5::KeyboardOptions> keyboard_;
    int keyboard_kind_ = 0;
    std::optional<Edit> edit_;
    std::map<std::string, Edit> pending_edits_;
    std::set<std::uint64_t> handled_results_;
    std::string notice_, storage_status_, last_target_, launch_target_;
    std::int64_t now_ = 0, launch_deadline_ = 0, return_deadline_ = 0, return_next_ = 0;
    std::uint64_t launch_ticket_ = 0, return_ticket_ = 0, generation_ = 0, presses_ = 0,
                  forwarded_ = 0;
    int return_backs_ = 0;
    bool quitting_ = false, returning_ = false, has_focus_ = false, local_blocked_ = false;
    bool seen_remote_ = false, cued_ = false, lg_surface_ = false, lg_baseline_ = false;
};
} // namespace prism
