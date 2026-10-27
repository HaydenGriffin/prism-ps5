// Prism - Setup, launcher, favourites, debounced settings and foreground remote flow.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prism/application.hpp"
#include "prism/decimal.hpp"
#include <algorithm>
#include <set>

namespace prism
{
using hui::Action;
using hui::Direction;
using hui::ui::ListItem;
Application::Application(Config c) : config_(std::move(c))
{
    auto theme = hui::ui::default_theme();
    using hui::gfx::Color;
    theme.page = Color::rgb(0x0d0f10);
    theme.surface = Color::rgb(0x181b20);
    theme.surface_high = Color::rgb(0x22272e);
    theme.text = Color::rgb(0xf5f4ef);
    theme.text_muted = Color::rgb(0xb9bec5);
    theme.page_text = theme.text;
    theme.page_text_muted = theme.text_muted;
    theme.focus = Color::rgb(0x7bdafa);
    theme.primary = theme.focus;
    theme.accent = theme.focus;
    theme.on_primary = theme.page;
    theme.style = hui::ui::SurfaceStyle::flat;
    theme.radius = 16;
    theme.radius_card = 20;
    theme.border = 0;
    theme.focus_width = 4;
    theme.focus_gap = 5;
    grid_.style.theme = theme;
    grid_.style.columns = 4;
    grid_.style.cell_height = 236;
    grid_.style.gap_x = 28;
    grid_.style.gap_y = 28;
    grid_.style.card.focus_scale = 1.025f;
    grid_.style.card.lift = 2;
    grid_.style.padding = 12;
    grid_.set_bounds({84, 252, 1752, 650});
    list_.style.theme = theme;
    list_.style.row_height = 82;
    list_.style.title_size = 30;
    list_.style.subtitle_size = 24;
    list_.style.value_size = 28;
    list_.style.highlight.kind = hui::ui::HighlightKind::ring;
    list_.style.cards = false;
    list_.set_bounds({96, 250, 1080, 650});
    setup_view();
    if (!config_.configured())
        page_ = Page::setup;
    rebuild();
}
const Tv *Application::tv() const
{
    for (const auto &t : snapshot_.tvs)
        if (t.device == config_.device)
            return &t;
    return nullptr;
}
bool Application::ready() const
{
    return snapshot_.ready && tv() && tv()->usable();
}
const App *Application::find_app(const std::string &id) const
{
    for (const auto &a : apps_)
        if (a.id == id)
            return &a;
    return nullptr;
}
std::string Application::app_name(const std::string &id) const
{
    if (const auto *a = find_app(id))
        return a->title;
    auto title = system_title(id);
    return title.empty() ? id : title;
}
void Application::persist()
{
    if (save)
        save(config_);
}
std::uint64_t Application::dispatch(Command c)
{
    auto ticket = send ? send(std::move(c)) : 0;
    if (!ticket)
        notice_ = "Not sent. Check the connection and TV in Setup.";
    return ticket;
}
void Application::receive(Snapshot s)
{
    const bool generation_changed = generation_ != s.generation;
    if (generation_changed)
    {
        generation_ = s.generation;
        remote_.reset();
        if (edit_ || !pending_edits_.empty())
        {
            notice_ = "Connection changed; pending settings were cancelled.";
            edit_.reset();
            pending_edits_.clear();
        }
    }
    snapshot_ = std::move(s);
    if (tv())
        apps_ = catalog(*tv(), snapshot_.states);
    else
        apps_.clear();
    // Home Assistant can answer service calls in any order, so each result is
    // handled once by ticket rather than by a high-water mark.
    std::set<std::uint64_t> listed;
    for (const auto &r : snapshot_.results)
        if (listed.insert(r.ticket).second && handled_results_.insert(r.ticket).second)
        {
            if (!r.ok)
            {
                notice_ = r.message;
                if (r.ticket == launch_ticket_)
                {
                    launch_target_.clear();
                    launch_deadline_ = 0;
                    if (tv() && target(*tv(), snapshot_.states) == config_.input_app)
                        page_ = Page::launcher;
                }
                if (r.ticket == return_ticket_)
                    returning_ = false;
            }
            for (auto it = pending_edits_.begin(); it != pending_edits_.end();)
                if (it->second.ticket == r.ticket)
                {
                    notice_ = r.ok ? "Setting sent; waiting for the TV" : r.message;
                    if (!r.ok)
                        it = pending_edits_.erase(it);
                    else
                    {
                        it->second.accepted = true;
                        ++it;
                    }
                }
                else
                    ++it;
        }
    std::erase_if(handled_results_, [&](std::uint64_t t) { return !listed.contains(t); });
    for (auto it = pending_edits_.begin(); it != pending_edits_.end();)
    {
        const auto &e = it->second;
        const auto confirmed = snapshot_.states.get(e.entity).state;
        const bool matches = e.role == "oled_light"
                                 ? !confirmed.empty() && parse_decimal(confirmed.c_str()) ==
                                                             parse_decimal(e.value.c_str())
                                 : confirmed == e.value;
        if (e.accepted && matches)
        {
            notice_ = "TV setting updated";
            it = pending_edits_.erase(it);
        }
        else
            ++it;
    }
    if (!snapshot_.ready)
    {
        remote_.reset();
        if (cancel_transient)
            cancel_transient();
    }
    const auto current = tv() ? target(*tv(), snapshot_.states) : "";
    follow_lg_surface(current, generation_changed);
    if (current != last_target_)
    {
        last_target_ = current;
        if (launchable(current) && find_app(current))
        {
            std::erase(config_.recent, current);
            config_.recent.insert(config_.recent.begin(), current);
            if (config_.recent.size() > 32)
                config_.recent.resize(32);
            persist();
        }
    }
    rebuild();
}
// LG Settings hands over to screens such as Game Optimizer or the User Guide by showing the
// PS5 input for a few seconds in between, which returns Prism to its launcher. When an LG
// screen appears over the PS5 input while the launcher is up, the controller drives it again.
void Application::follow_lg_surface(const std::string &current, bool generation_changed)
{
    const bool surface = ready() && current != config_.input_app &&
                         controls_lg(*tv(), snapshot_.states, config_.input_app);
    if (generation_changed || !ready())
        lg_baseline_ = false;
    const bool appeared = lg_baseline_ && surface && !lg_surface_;
    lg_surface_ = surface;
    lg_baseline_ = ready();
    if (!appeared || (page_ != Page::launcher && page_ != Page::apps) || config_.input_app.empty())
        return;
    // open() resets the remote, which forwards nothing until every button is released.
    open(Page::remote);
    seen_remote_ = true;
    notice_ = app_name(current) + " is on the LG. Hold Circle to return to the PS5.";
}
void Application::open(Page p)
{
    flush_setting();
    page_ = p;
    notice_.clear();
    remote_.reset();
    seen_remote_ = false;
    cued_ = false;
    list_.set_focus(0);
    grid_.set_focus(0);
    rebuild();
    list_.enter();
    grid_.enter();
}
void Application::icon(const std::string &url, std::uint32_t texture)
{
    textures_[url] = texture;
}
void Application::rebuild()
{
    grid_.style.reduced_motion = config_.reduced_motion;
    list_.style.reduced_motion = config_.reduced_motion;
    std::string focused = grid_.focus() < static_cast<int>(tiles_.size())
                              ? tiles_[static_cast<std::size_t>(grid_.focus())].id
                              : "";
    tiles_.clear();
    auto add_app = [&](const std::string &id, const std::string &label)
    {
        if (const auto *a = find_app(id))
        {
            tiles_.push_back({a->id, a->title, label, a->icon, a->color});
            if (request_icon)
                request_icon(a->icon);
        }
    };
    if (page_ == Page::launcher)
    {
        std::string resume;
        for (const auto &id : config_.recent)
            if (find_app(id) && launchable(id))
            {
                resume = id;
                break;
            }
        if (!resume.empty())
            add_app(resume, "Continue on the LG");
        for (const auto &id : config_.favourites)
            if (launchable(id))
                add_app(id, "Favourite");
        int recent = 0;
        for (const auto &id : config_.recent)
            if (id != resume && find_app(id) && launchable(id) &&
                std::find(config_.favourites.begin(), config_.favourites.end(), id) ==
                    config_.favourites.end() &&
                recent++ < 3)
                add_app(id, "Recently used");
        for (const auto &[id, title, sub] :
             std::vector<std::tuple<std::string, std::string, std::string>>{
                 {"@apps", "All apps", "Choose an LG app"},
                 {"@home", "LG Home", "Open the TV home screen"},
                 {"@settings", "LG Settings", "Open the TV settings"},
                 {"@quick", "Quick TV settings", "Picture, sound and input"},
                 {"@remote", "Use as a remote", "Control what's on the LG"},
                 {"@setup", "Setup", "Connection, TV and favourites"}})
            tiles_.push_back({id, title, sub, "", ""});
    }
    else if (page_ == Page::apps)
    {
        for (const auto &a : apps_)
            if (launchable(a.id))
                add_app(a.id, std::find(config_.favourites.begin(), config_.favourites.end(),
                                        a.id) != config_.favourites.end()
                                  ? "Favourite"
                                  : "LG app");
    }
    grid_.set_count(static_cast<int>(tiles_.size()));
    if (!focused.empty())
        for (std::size_t i = 0; i < tiles_.size(); i++)
            if (tiles_[i].id == focused)
            {
                grid_.set_focus(static_cast<int>(i));
                break;
            }
    std::vector<ListItem> rows;
    auto row =
        [&](std::string title, std::string sub = "", std::string value = "", bool disabled = false)
    {
        ListItem r;
        r.title = std::move(title);
        r.subtitle = std::move(sub);
        r.value = std::move(value);
        r.disabled = disabled;
        rows.push_back(std::move(r));
    };
    if (page_ == Page::setup)
    {
        row("Home Assistant address", "Numeric IPv4 on your local network",
            config_.host.empty() ? "Not set" : config_.host);
        row("Port", "Home Assistant's local HTTP port", std::to_string(config_.port));
        row("Access token", "Long-lived token; hidden while you type",
            config_.token.empty() ? "Not set" : "Set");
        row("Connect / retry", "Apply the address and token");
        row("TV", "Select your Glasshouse TV", tv() ? tv()->name : "Choose a TV");
        row("Use this input for the PS5", "Keep the PS5 visible, then select this row",
            config_.input.empty() ? "Not set" : config_.input, !ready());
        row("Favourites", "Choose apps; L1 / R1 changes their order");
        row("Reduce motion", "Keep focus changes still", config_.reduced_motion ? "On" : "Off");
        row("Controller vibration", "A short rumble on each move",
            config_.vibration ? "On" : "Off");
        row("Input diagnostics", "Write controller counts to local status.json",
            config_.diagnostics ? "On" : "Off");
        row("Controls and setup check", "Connection, return shortcut and controller focus");
        row("Disconnect and forget token", "Keep the TV and favourites on this PS5");
        row("Close Prism", "Return to the PS5 home screen");
    }
    else if (page_ == Page::tvs)
    {
        for (const auto &t : snapshot_.tvs)
            row(t.name,
                t.usable() ? "Glasshouse TV"
                           : "Enable Active App, Launch App, Input Source and navigation buttons",
                t.device == config_.device ? "Selected" : "", !t.usable());
    }
    else if (page_ == Page::favourites)
    {
        for (const auto &id : config_.favourites)
            row(app_name(id), "Favourite · L1 / R1 to reorder", "On");
        for (const auto &a : apps_)
            if (launchable(a.id) && std::find(config_.favourites.begin(), config_.favourites.end(),
                                              a.id) == config_.favourites.end())
                row(a.title, "Select to add to favourites", "Off");
    }
    else if (page_ == Page::quick)
    {
        for (const auto &s : settings())
        {
            const auto &e = tv() ? snapshot_.states.get(tv()->role(s.role)) : Entity{};
            std::string value = edit_ && edit_->role == s.role    ? edit_->value
                                : pending_edits_.contains(s.role) ? pending_edits_.at(s.role).value
                                                                  : e.state;
            row(s.label, "", value.empty() ? "Not available" : value, !ready() || !e.available());
        }
    }
    else if (page_ == Page::quit)
    {
        row("Keep using Prism");
        row("Close Prism", "The DualSense will stop controlling the LG");
    }
    int focus = list_.focus();
    list_.set_items(std::move(rows));
    list_.set_focus(focus);
}
void Application::launch(const std::string &id)
{
    if (!ready() || config_.input.empty() || config_.input_app.empty())
    {
        notice_ = "Choose a TV and capture the PS5 input in Setup first.";
        return;
    }
    Command command;
    if (id == "@home")
        command = press(*tv(), "remote_home");
    else
    {
        auto *app = find_app(id);
        // Settings is not always listed; Glasshouse still launches it by its usual name.
        if (!app && id != "com.palm.app.settings")
        {
            notice_ = "This app is not in the TV's current catalogue.";
            return;
        }
        command = select(tv()->role("app"), app ? app->title : "Settings");
    }
    command.transient = false;
    auto ticket = dispatch(command);
    if (!ticket)
        return;
    open(Page::remote);
    launch_ticket_ = ticket;
    launch_target_ = id;
    launch_deadline_ = now_ + 10000000;
    notice_ = "Opening on the LG…";
}
void Application::return_to_ps5()
{
    if (!ready())
    {
        notice_ = "Reconnect to Home Assistant to return to the PS5. The LG remote still works.";
        return;
    }
    if (config_.input.empty())
    {
        notice_ = "The PS5 input is not set. Use the LG remote, then capture it in Setup.";
        return;
    }
    if (cancel_transient)
        cancel_transient();
    auto command = overlay_on_input(*tv(), snapshot_.states, config_.input_app)
                       ? press(*tv(), "remote_back")
                       : select(tv()->role("input_source"), config_.input);
    command.transient = false;
    return_ticket_ = dispatch(command);
    if (!return_ticket_)
        return;
    returning_ = true;
    launch_deadline_ = 0;
    launch_target_.clear();
    return_deadline_ = now_ + 8000000;
    return_next_ = now_ + 500000;
    return_backs_ = 0;
    notice_ = "Returning to the PS5…";
}
void Application::request_keyboard(int kind)
{
    keyboard_kind_ = kind;
    hui::ps5::KeyboardOptions k;
    if (kind == 1)
    {
        k.title = "Home Assistant IPv4 address";
        k.initial = config_.host;
        k.max_length = 64;
        k.kind = hui::ps5::KeyboardKind::url;
    }
    if (kind == 2)
    {
        k.title = "Home Assistant access token";
        k.secret = true;
        k.max_length = 2048;
    }
    if (kind == 3)
    {
        k.title = "Home Assistant port";
        k.initial = std::to_string(config_.port);
        k.max_length = 5;
        k.kind = hui::ps5::KeyboardKind::number;
    }
    if (kind == 4)
    {
        k.title = "Type on the LG";
        k.max_length = 256;
    }
    keyboard_ = std::move(k);
}
std::optional<hui::ps5::KeyboardOptions> Application::take_keyboard()
{
    auto k = std::move(keyboard_);
    keyboard_.reset();
    return k;
}
void Application::keyboard_result(const hui::ps5::KeyboardOutcome &result)
{
    if (!result.accepted)
    {
        notice_ = "Text entry cancelled";
        return;
    }
    if (keyboard_kind_ == 1)
    {
        Config test = config_;
        test.host = result.text;
        test.token = "validation";
        if (!test.configured())
        {
            notice_ = "Use a numeric IPv4 address, such as 192.0.2.20.";
            return;
        }
        config_.host = result.text;
    }
    else if (keyboard_kind_ == 2)
    {
        if (result.text.empty())
        {
            notice_ = "The token cannot be empty.";
            return;
        }
        config_.token = result.text;
    }
    else if (keyboard_kind_ == 3)
    {
        char *end = nullptr;
        long port = std::strtol(result.text.c_str(), &end, 10);
        if (port < 1 || port > 65535 || !end || *end)
        {
            notice_ = "Port must be between 1 and 65535.";
            return;
        }
        config_.port = static_cast<int>(port);
    }
    else if (keyboard_kind_ == 4)
    {
        if (ready() && !tv()->role("keyboard_text").empty() &&
            field(snapshot_.states.get(tv()->role("active_app")).attributes, "keyboard_visible") ==
                true)
            dispatch({tv()->role("keyboard_text"), "set_value", {{"value", result.text}}});
        else
            notice_ = "The LG text field is no longer focused. Nothing was sent.";
        return;
    }
    persist();
    notice_ = "Select Connect / retry to apply these credentials.";
    rebuild();
}
void Application::activate(int index)
{
    if (page_ == Page::launcher || page_ == Page::apps)
    {
        if (index < 0 || index >= static_cast<int>(tiles_.size()))
            return;
        auto id = tiles_[static_cast<std::size_t>(index)].id;
        if (id == "@apps")
            open(Page::apps);
        else if (id == "@quick")
            open(Page::quick);
        else if (id == "@setup")
            open(Page::setup);
        else if (id == "@settings")
            launch("com.palm.app.settings");
        else if (id == "@remote")
        {
            if (ready() && !config_.input.empty() && !config_.input_app.empty())
            {
                open(Page::remote);
                notice_ = "Remote ready. Hold Circle to return to the PS5.";
            }
            else
                notice_ = "Complete the TV and PS5 input steps in Setup first.";
        }
        else
            launch(id);
        return;
    }
    if (page_ == Page::setup)
    {
        switch (index)
        {
        case 0:
            request_keyboard(1);
            break;
        case 1:
            request_keyboard(3);
            break;
        case 2:
            request_keyboard(2);
            break;
        case 3:
            if (reconnect)
                reconnect(config_);
            notice_ = "Connecting…";
            break;
        case 4:
            open(Page::tvs);
            break;
        case 5:
            if (ready())
            {
                const auto &input = snapshot_.states.get(tv()->role("input_source"));
                const auto &active = snapshot_.states.get(tv()->role("active_app"));
                auto id = text(active.attributes, "app_id");
                if (!input.available() || !id.starts_with("com.webos.app.hdmi"))
                {
                    notice_ = "Show the PS5's HDMI input on the TV, then try again.";
                    break;
                }
                config_.input = input.state;
                config_.input_app = id;
                persist();
                notice_ = "PS5 return input saved. Nothing was switched.";
                rebuild();
            }
            break;
        case 6:
            open(Page::favourites);
            break;
        case 7:
            config_.reduced_motion = !config_.reduced_motion;
            persist();
            rebuild();
            break;
        case 8:
            config_.vibration = !config_.vibration;
            persist();
            rebuild();
            break;
        case 9:
            config_.diagnostics = !config_.diagnostics;
            persist();
            rebuild();
            break;
        case 10:
            open(Page::help);
            break;
        case 11:
            config_.token.clear();
            persist();
            if (reconnect)
                reconnect(config_);
            notice_ = "Disconnected. Saving the token removal…";
            rebuild();
            break;
        case 12:
            open(Page::quit);
            break;
        }
    }
    else if (page_ == Page::tvs)
    {
        if (index >= 0 && index < static_cast<int>(snapshot_.tvs.size()))
        {
            auto id = snapshot_.tvs[static_cast<std::size_t>(index)].device;
            if (id != config_.device)
            {
                config_.device = id;
                config_.input.clear();
                config_.input_app.clear();
                config_.recent.clear();
                config_.favourites.clear();
                persist();
                if (reconnect)
                    reconnect(config_);
            }
            open(Page::setup);
        }
    }
    else if (page_ == Page::favourites)
    {
        std::vector<std::string> ids = config_.favourites;
        for (const auto &a : apps_)
            if (launchable(a.id) && std::find(ids.begin(), ids.end(), a.id) == ids.end())
                ids.push_back(a.id);
        if (index >= 0 && index < static_cast<int>(ids.size()))
        {
            auto id = ids[static_cast<std::size_t>(index)];
            if (!std::erase(config_.favourites, id))
                config_.favourites.push_back(id);
            persist();
            rebuild();
        }
    }
    else if (page_ == Page::quick)
        change_setting(1);
    else if (page_ == Page::quit)
    {
        if (index == 1)
        {
            flush_setting();
            quitting_ = true;
        }
        else
            open(Page::launcher);
    }
}
void Application::change_setting(int direction)
{
    if (!ready())
        return;
    int at = list_.focus();
    if (at < 0 || at >= static_cast<int>(settings().size()))
        return;
    const auto &s = settings()[static_cast<std::size_t>(at)];
    auto id = tv()->role(s.role);
    const auto &e = snapshot_.states.get(id);
    if (!e.available())
        return;
    if (edit_ && edit_->role != s.role)
        flush_setting();
    auto current = edit_                             ? edit_->value
                   : pending_edits_.contains(s.role) ? pending_edits_.at(s.role).value
                                                     : e.state;
    auto value = step_setting(e, current, direction, s.number);
    edit_ = Edit{s.role, id, value, now_ + 500000, 0};
    rebuild();
}
void Application::flush_setting()
{
    if (!edit_)
        return;
    if (!ready())
    {
        edit_.reset();
        return;
    }
    auto e = *edit_;
    edit_.reset();
    auto command =
        e.role == "oled_light" ? set_number(e.entity, e.value) : select(e.entity, e.value);
    e.ticket = dispatch(command);
    e.due = now_ + 8000000;
    if (e.ticket)
        pending_edits_[e.role] = std::move(e);
}
void Application::update(const hui::InputFrame &input, float dt, std::int64_t now,
                         hui::ui::Feedback &feedback)
{
    now_ = now;
    has_focus_ = input.connected && !input.focus_lost;
    if (input.pressed)
        ++presses_;
    grid_.update(dt);
    list_.update(dt);
    if (edit_ && !edit_->ticket && now_ >= edit_->due)
        flush_setting();
    for (auto it = pending_edits_.begin(); it != pending_edits_.end();)
        if (now_ > it->second.due)
        {
            notice_ = "The TV did not confirm the setting. Showing its reported value.";
            it = pending_edits_.erase(it);
            rebuild();
        }
        else
            ++it;
    if (input.focus_lost)
    {
        remote_.reset();
        local_blocked_ = true;
        if (cancel_transient)
            cancel_transient();
        return;
    }
    if (!input.connected)
        return;
    if (local_blocked_)
    {
        if (!input.held && !input.pressed)
            local_blocked_ = false;
        return;
    }
    if (page_ == Page::remote)
    {
        const auto current = ready() ? target(*tv(), snapshot_.states) : "";
        if (launch_deadline_)
        {
            if (current == launch_target_ ||
                (launch_target_ == "@home" && current == "com.webos.app.home") ||
                (launch_target_ == "com.palm.app.settings" && current == "com.webos.app.settings"))
            {
                launch_deadline_ = 0;
                launch_target_.clear();
                notice_ = "";
            }
            else if (now_ > launch_deadline_)
            {
                launch_deadline_ = 0;
                launch_target_.clear();
                notice_ = "The TV did not confirm the app. Hold Circle to return.";
            }
        }
        if (returning_)
        {
            if (current == config_.input_app &&
                field(snapshot_.states.get(tv()->role("active_app")).attributes,
                      "keyboard_visible") != true)
            {
                returning_ = false;
                open(Page::launcher);
                notice_ = "Back on the PS5";
                return;
            }
            if (now_ > return_deadline_)
            {
                returning_ = false;
                notice_ = "The TV did not confirm the PS5 input. Try again or use the LG remote.";
            }
            else if (ready() && now_ >= return_next_ && return_backs_ < 8 &&
                     overlay_on_input(*tv(), snapshot_.states, config_.input_app))
            {
                auto c = press(*tv(), "remote_back");
                c.transient = false;
                return_ticket_ = dispatch(c);
                return_next_ = now_ + 600000;
                ++return_backs_;
            }
        }
        auto actions = remote_.update(input, now_, ready());
        if (actions.return_input)
        {
            return_to_ps5();
            return;
        }
        const bool control = ready() && controls_lg(*tv(), snapshot_.states, config_.input_app);
        if (control)
            seen_remote_ = true;
        if (seen_remote_ && !control && current == config_.input_app && !launch_deadline_ &&
            !returning_)
        {
            open(Page::launcher);
            return;
        }
        if (!returning_ && !launch_deadline_ && control)
        {
            if (!actions.buttons.empty() && !cued_)
            {
                cued_ = true;
                if (!tv()->role("screen_notification").empty())
                    dispatch({tv()->role("screen_notification"),
                              "set_value",
                              {{"value", "Prism remote on the LG · hold Circle for the PS5"}},
                              true});
            }
            for (const auto &button : actions.buttons)
            {
                if (dispatch(press(*tv(), button)))
                    ++forwarded_;
            }
            const bool keyboard = field(snapshot_.states.get(tv()->role("active_app")).attributes,
                                        "keyboard_visible") == true;
            if (keyboard && input.is_pressed(Action::jump_prev) &&
                !tv()->role("keyboard_backspace").empty())
                dispatch(press(*tv(), "keyboard_backspace"));
            if (keyboard && input.is_pressed(Action::jump_next) &&
                !tv()->role("keyboard_enter").empty())
                dispatch(press(*tv(), "keyboard_enter"));
            if (input.is_pressed(Action::touch))
            {
                if (keyboard && !tv()->role("keyboard_text").empty())
                    request_keyboard(4);
                else
                    notice_ = "Open a text field on the LG first. Glasshouse keyboard support is "
                              "required.";
            }
        }
        return;
    }
    if (input.is_pressed(Action::menu))
    {
        open(page_ == Page::setup ? Page::launcher : Page::setup);
        return;
    }
    if (input.is_pressed(Action::back))
    {
        open(page_ == Page::launcher ? Page::quit : Page::launcher);
        return;
    }
    if (page_ == Page::help)
        return;
    if (page_ == Page::launcher || page_ == Page::apps)
    {
        if (input.is_pressed(Action::west) && !tiles_.empty())
        {
            const auto &id = tiles_[static_cast<std::size_t>(grid_.focus())].id;
            if (find_app(id) && launchable(id))
            {
                if (!std::erase(config_.favourites, id))
                    config_.favourites.push_back(id);
                persist();
                rebuild();
            }
        }
        if (grid_.handle(input, feedback) == hui::ui::Event::activated)
            activate(grid_.focus());
    }
    else
    {
        if (page_ == Page::quick && (input.nav == Direction::left || input.nav == Direction::right))
        {
            change_setting(input.nav == Direction::left ? -1 : 1);
            return;
        }
        if (page_ == Page::quick && (input.nav == Direction::up || input.nav == Direction::down))
            flush_setting();
        if (page_ == Page::favourites &&
            (input.is_pressed(Action::page_prev) || input.is_pressed(Action::page_next)))
        {
            int i = list_.focus(), j = i + (input.is_pressed(Action::page_prev) ? -1 : 1);
            if (i >= 0 && i < static_cast<int>(config_.favourites.size()) && j >= 0 &&
                j < static_cast<int>(config_.favourites.size()))
            {
                std::swap(config_.favourites[static_cast<std::size_t>(i)],
                          config_.favourites[static_cast<std::size_t>(j)]);
                persist();
                rebuild();
                list_.set_focus(j);
            }
            return;
        }
        if (list_.handle(input, feedback) == hui::ui::Event::activated)
            activate(list_.focus());
    }
}
Json Application::diagnostic() const
{
    return {{"uptime_us", now_},
            {"page", static_cast<int>(page_)},
            {"controller_focus", has_focus_},
            {"press_frames", presses_},
            {"forwarded", forwarded_},
            {"ha_ready", snapshot_.ready},
            {"remote_mode", remote_mode()}};
}
} // namespace prism
