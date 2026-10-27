// Prism - Host behaviour and network integration harness.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prism/application.hpp"
#include "prism/resources.hpp"
#include "afterglow/artwork.hpp"
#include "core/save_file.hpp"
#include "platform/ps5/system.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace prism;
int checks = 0;
#define CHECK(condition)                                                                           \
    do                                                                                             \
    {                                                                                              \
        ++checks;                                                                                  \
        if (!(condition))                                                                          \
        {                                                                                          \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);              \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
Json fixture()
{
    std::ifstream f("tests/fixture.json");
    return Json::parse(f, nullptr, false);
}
Snapshot snapshot(const Json &f)
{
    Snapshot s;
    s.ready = true;
    s.connected = true;
    s.generation = 1;
    s.tvs = discover(f["entities"], f["devices"]);
    s.states.apply(f["states"]);
    return s;
}
hui::InputFrame input(hui::Action action)
{
    hui::InputFrame in;
    in.connected = true;
    in.pressed = hui::action_bit(action);
    in.held = in.pressed;
    return in;
}
void tick(Application &app, hui::InputFrame in, std::int64_t time = 1000000)
{
    hui::ui::Feedback fb;
    app.update(in, .016f, time, fb);
}
int main(int argc, char **argv)
{
    auto f = fixture();
    CHECK(f.is_object());
    if (argc == 3 && std::string(argv[1]) == "--auth-rejected")
    {
        Config c = Config::parse(f["config"]);
        c.port = std::atoi(argv[2]);
        Client client;
        CHECK(client.start(c));
        auto until = hui::sys::monotonic_us() + 5000000;
        Snapshot s;
        do
        {
            s = client.snapshot();
            if (s.status.find("rejected") != std::string::npos)
                break;
            hui::sys::sleep_us(20000);
        } while (hui::sys::monotonic_us() < until);
        CHECK(!s.ready);
        CHECK(s.status.find("rejected") != std::string::npos);
        hui::sys::sleep_us(1200000);
        CHECK(client.snapshot().generation == s.generation);
        client.stop();
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--integration")
    {
        Config c = Config::parse(f["config"]);
        c.port = std::atoi(argv[2]);
        Client client;
        CHECK(client.start(c));
        auto until = hui::sys::monotonic_us() + 15000000;
        Snapshot s;
        do
        {
            s = client.snapshot();
            if (s.ready)
                break;
            hui::sys::sleep_us(20000);
        } while (hui::sys::monotonic_us() < until);
        CHECK(s.ready);
        CHECK(s.tvs.size() == 1);
        CHECK(s.tvs[0].usable());
        while (s.states.get(s.tvs[0].role("active_app")).state.empty() &&
               hui::sys::monotonic_us() < until)
        {
            hui::sys::sleep_us(20000);
            s = client.snapshot();
        }
        CHECK(target(s.tvs[0], s.states) == "com.webos.app.hdmi3");
        auto ticket = client.send(select(s.tvs[0].role("app"), "Afterglow"));
        CHECK(ticket > 0);
        bool accepted = false;
        do
        {
            s = client.snapshot();
            for (const auto &r : s.results)
                if (r.ticket == ticket)
                    accepted = r.ok;
            if (accepted)
                break;
            hui::sys::sleep_us(20000);
        } while (hui::sys::monotonic_us() < until);
        CHECK(accepted);
        ticket = client.send(press(s.tvs[0], "remote_up"));
        CHECK(ticket > 0);
        bool rejected = false;
        do
        {
            s = client.snapshot();
            for (const auto &r : s.results)
                if (r.ticket == ticket)
                    rejected = !r.ok;
            if (rejected)
                break;
            hui::sys::sleep_us(20000);
        } while (hui::sys::monotonic_us() < until);
        CHECK(rejected);
        // The server now renames an entity and sends registry events; wait for the new
        // subscription.
        do
        {
            s = client.snapshot();
            if (s.ready && !s.tvs.empty() && s.tvs[0].role("remote_up") == "button.renamed_up")
                break;
            hui::sys::sleep_us(20000);
        } while (hui::sys::monotonic_us() < until);
        CHECK(s.ready);
        CHECK(s.tvs[0].role("remote_up") == "button.renamed_up");
        CHECK(client.send({"switch.unrelated", "toggle"}) == 0);
        const auto old = s.generation;
        ticket = client.send(press(s.tvs[0], "remote_down"));
        CHECK(ticket > 0);
        do
        {
            s = client.snapshot();
            if (s.ready && s.generation > old)
                break;
            hui::sys::sleep_us(20000);
        } while (hui::sys::monotonic_us() < until);
        CHECK(s.ready && s.generation > old);
        client.stop();
        CHECK(!client.snapshot().ready);
        std::printf("integration passed (%d checks)\n", checks);
        return 0;
    }
    auto s = snapshot(f);
    CHECK(s.tvs.size() == 1);
    auto tv = s.tvs[0];
    CHECK(tv.usable());
    CHECK(tv.name == "Living Room LG C1");
    auto changed = f;
    changed["entities"][0]["entity_id"] = "select.renamed_launcher";
    CHECK(discover(changed["entities"], changed["devices"])[0].role("app") ==
          "select.renamed_launcher");
    changed = f;
    changed["entities"][0]["disabled_by"] = "user";
    CHECK(discover(changed["entities"], changed["devices"]).empty());
    changed = f;
    for (auto &e : changed["entities"])
        if (text(e, "unique_id").ends_with("remote_up"))
            e["entity_id"] = "switch.wrong_domain";
    CHECK(!discover(changed["entities"], changed["devices"])[0].usable());
    changed = f;
    changed["entities"].push_back({{"entity_id", "sensor.loose"}, {"device_id", nullptr}});
    const auto kept = device_entities(changed["entities"]);
    CHECK(kept.size() == f["entities"].size());
    CHECK(discover(kept, f["devices"])[0].roles == tv.roles);
    CHECK(!affects_tvs("entity_registry_updated",
                       Json({{"action", "update"}, {"entity_id", "sensor.kitchen"}}), kept, s.tvs));
    CHECK(affects_tvs("entity_registry_updated",
                      Json({{"action", "update"}, {"entity_id", tv.role("remote_up")}}), kept,
                      s.tvs));
    CHECK(affects_tvs("entity_registry_updated",
                      Json({{"action", "update"},
                            {"entity_id", "button.new_name"},
                            {"old_entity_id", tv.role("remote_up")}}),
                      kept, s.tvs));
    CHECK(affects_tvs("entity_registry_updated",
                      Json({{"action", "create"}, {"entity_id", "sensor.anything"}}), kept, s.tvs));
    CHECK(affects_tvs("device_registry_updated",
                      Json({{"action", "update"}, {"device_id", tv.device}}), kept, s.tvs));
    CHECK(!affects_tvs("device_registry_updated",
                       Json({{"action", "update"}, {"device_id", "other"}}), kept, s.tvs));
    CHECK(affects_tvs("entity_registry_updated", {{"action", "update"}}, kept, {}));
    CHECK(catalog(tv, s.states).size() == 6);
    CHECK(parse_decimal("42") == 42);
    CHECK(parse_decimal("-0.5") == -0.5);
    CHECK(parse_decimal("0.001") == 0.001);
    CHECK(parse_decimal("1.7e+09") == 1.7e9);
    CHECK(parse_decimal("2.5E-3") == 2.5e-3);
    CHECK(parse_decimal("1700000000.123456") > 1700000000.1234 &&
          parse_decimal("1700000000.123456") < 1700000000.1235);
    char *end = nullptr;
    const char *partial = "12.5x";
    CHECK(parse_decimal(partial, &end) == 12.5 && end == partial + 4);
    const char *none = "x";
    CHECK(parse_decimal(none, &end) == 0 && end == none);
    const auto decimals = Json::parse(R"({"t":1700000000.123456,"e":1.7e9,"n":-0.25})");
    CHECK(decimals["e"].get<double>() == 1.7e9 && decimals["n"].get<double>() == -0.25);
    CHECK(!launchable("com.webos.app.hdmi3"));
    CHECK(system_title("com.webos.app.tvuserguide") == "User Guide");
    CHECK(system_title("com.palm.app.settings") == "LG Settings");
    CHECK(system_title("org.example.app").empty());
    CHECK(!launchable("com.palm.app.settings"));
    CHECK(launchable("youtube.leanback.v4"));
    auto active = tv.role("active_app");
    s.states.apply(
        {{"c", {{active, {{"+", {{"a", {{"control_app_id", "com.palm.app.settings"}}}}}}}}}});
    CHECK(target(tv, s.states) == "com.palm.app.settings");
    CHECK(controls_lg(tv, s.states, "com.webos.app.hdmi3"));
    CHECK(overlay_on_input(tv, s.states, "com.webos.app.hdmi3"));
    s.states.apply({{"c", {{active, {{"-", {{"a", Json::array({"control_app_id"})}}}}}}}});
    CHECK(target(tv, s.states) == "com.webos.app.hdmi3");
    CHECK(!controls_lg(tv, s.states, "com.webos.app.hdmi3"));
    s.states.apply(
        {{"c", {{active, {{"+", {{"a", {{"control_app_id", "com.webos.app.hdmi2"}}}}}}}}}});
    CHECK(!controls_lg(tv, s.states, "com.webos.app.hdmi3"));
    s.states.apply({{"c", {{active, {{"+", {{"a", {{"keyboard_visible", true}}}}}}}}}});
    CHECK(controls_lg(tv, s.states, "com.webos.app.hdmi3"));
    s.states.apply({{"r", Json::array({active})}});
    CHECK(target(tv, s.states).empty());
    s.states.apply(
        {{"c", {{active, {{"+", {{"s", "on"}, {"a", {{"app_id", "org.afterglow"}}}}}}}}}});
    CHECK(target(tv, s.states).empty());
    s = snapshot(f);
    const auto &energy = s.states.get(tv.role("energy_saving"));
    CHECK(choices(energy).size() == 4);
    CHECK(step_setting(energy, "max", 1, false) == "max");
    CHECK(step_setting(energy, "missing", 1, false) == "off");
    auto brightness = s.states.get(tv.role("oled_light"));
    CHECK(std::stod(step_setting(brightness, "98", 1, true)) == 100);
    CHECK(std::stod(step_setting(brightness, "0", -1, true)) == 0);
    brightness.attributes["min"] = 20;
    brightness.attributes["max"] = 80;
    CHECK(std::stod(step_setting(brightness, "18", -1, true)) == 20);
    Config c = Config::parse(f["config"]);
    CHECK(c.configured());
    CHECK(Config::parse(c.json()).input_app == c.input_app);
    CHECK(!c.vibration);
    c.vibration = true;
    CHECK(Config::parse(c.json()).vibration);
    c.vibration = false;
    c.host = "https://example.com";
    CHECK(!c.configured());
    c.host = "127.0.0.1";
    c.token = "x\ny";
    CHECK(!c.configured());
    auto command = select(tv.role("app"), "App \"quoted\"");
    CHECK(command.wire(9)["service_data"]["option"] == "App \"quoted\"");
    CHECK((Command{"switch.x", "toggle"}.wire(1).is_null()));
    Remote remote;
    hui::InputFrame neutral;
    neutral.connected = true;
    remote.update(neutral, 1000000, true);
    auto in = input(hui::Action::back);
    CHECK(remote.update(in, 1100000, true).buttons.empty());
    in.pressed = 0;
    CHECK(!remote.update(in, 1899999, true).return_input);
    CHECK(remote.update(in, 1900000, true).return_input);
    CHECK(!remote.update(in, 2000000, true).return_input);
    CHECK(remote.update(neutral, 2100000, true).buttons.empty());
    in = input(hui::Action::back);
    remote.update(in, 2200000, true);
    CHECK(remote.update(neutral, 2300000, true).buttons == std::vector<std::string>{"remote_back"});
    in = input(hui::Action::back);
    in.held = 0;
    CHECK(remote.update(in, 2400000, true).buttons == std::vector<std::string>{"remote_back"});
    in = input(hui::Action::confirm);
    CHECK(remote.update(in, 2500000, true).buttons == std::vector<std::string>{"remote_ok"});
    in.focus_lost = true;
    CHECK(remote.update(in, 2600000, true).buttons.empty());
    in.focus_lost = false;
    CHECK(remote.update(in, 2700000, true).buttons.empty());
    remote.update(neutral, 2800000, true);
    CHECK(remote.update(in, 2900000, true).buttons.size() == 1);
    in = neutral;
    in.nav = hui::Direction::up;
    CHECK(remote.update(in, 3000000, true).buttons.size() == 1);
    CHECK(remote.update(in, 3100000, true).buttons.empty());
    CHECK(remote.update(in, 3150000, true).buttons.size() == 1);
    CHECK(remote.update(in, 3300000, false).buttons.empty());
    // Controller frame coalescing retains a tap but discards input intercepted by the shell.
    hui::InputTracker tracker;
    hui::PadSample pad;
    pad.connected = true;
    tracker.update(std::span(&pad, 1), 1);
    pad.buttons = hui::pad_bits::kCross;
    hui::PadSample pair[2] = {pad, pad};
    pair[1].buttons = 0;
    auto frame = tracker.update(pair, 20000);
    CHECK(frame.is_pressed(hui::Action::confirm));
    pad.buttons = hui::pad_bits::kIntercepted | hui::pad_bits::kCross;
    CHECK(tracker.update(std::span(&pad, 1), 30000).focus_lost);
    CHECK(tracker.update(std::span(&pad, 1), 40000).focus_lost);
    CHECK(tracker.update({}, 50000).focus_lost);
    CHECK(tracker.update({}, 500000).nav == hui::Direction::none);
    c = Config::parse(f["config"]);
    Application app(c);
    std::vector<Command> sent;
    int saves = 0;
    app.send = [&](Command cmd)
    {
        sent.push_back(cmd);
        return sent.size();
    };
    app.save = [&](const Config &) { ++saves; };
    app.receive(snapshot(f));
    app.open(Page::quick);
    tick(app, neutral);
    in = neutral;
    in.nav = hui::Direction::down;
    tick(app, in); // picture mode
    in.nav = hui::Direction::right;
    tick(app, in, 1100000);
    CHECK(sent.empty());
    tick(app, neutral, 1599999);
    CHECK(sent.empty());
    tick(app, neutral, 1600000);
    CHECK(sent.size() == 1);
    CHECK(sent[0].entity == tv.role("picture_mode"));
    CHECK(sent[0].data["option"] == "game");
    app.open(Page::apps);
    tick(app, input(hui::Action::confirm), 2000000);
    CHECK(app.remote_mode());
    CHECK(sent.back().entity == tv.role("app"));
    CHECK(sent.back().data["option"] == "Afterglow");
    tick(app, neutral, 2100000);
    tick(app, input(hui::Action::confirm), 2200000);
    CHECK(sent.size() == 2); // launch barrier
    auto running = snapshot(f);
    running.states.apply(
        {{"c",
          {{active,
            {{"+",
              {{"a", {{"app_id", "org.afterglow"}, {"control_app_id", "org.afterglow"}}}}}}}}}});
    app.receive(running);
    tick(app, neutral, 2300000);
    tick(app, input(hui::Action::confirm), 2400000);
    CHECK(sent.back().entity == tv.role("remote_ok"));
    CHECK(saves > 0);
    tick(app, input(hui::Action::back), 2500000);
    in = input(hui::Action::back);
    in.pressed = 0;
    tick(app, in, 3300000);
    CHECK(sent.back().entity == tv.role("input_source"));
    CHECK(sent.back().data["option"] == "HDMI 3");
    app.receive(snapshot(f));
    tick(app, neutral, 3400000);
    CHECK(app.page() == Page::launcher);
    // LG Settings opens Game Optimizer by way of the PS5 input: the remote follows it.
    auto optimizer = snapshot(f);
    optimizer.states.apply(
        {{"c", {{active, {{"+", {{"a", {{"control_app_id", "com.webos.app.gameoptimizer"}}}}}}}}}});
    const auto before = sent.size();
    app.receive(optimizer);
    CHECK(app.remote_mode());
    auto held = input(hui::Action::confirm);
    held.pressed = 0;
    tick(app, held, 3600000);
    CHECK(sent.size() == before); // the press held through the switch is not forwarded
    tick(app, neutral, 3700000);
    tick(app, input(hui::Action::confirm), 3800000);
    CHECK(sent.back().entity == tv.role("remote_ok"));
    // A screen already on the LG when Prism connects does not take over the launcher.
    Application fresh(c);
    fresh.send = [&](Command cmd)
    {
        sent.push_back(cmd);
        return sent.size();
    };
    fresh.receive(optimizer);
    CHECK(fresh.page() == Page::launcher);
    // Each quick-setting row has its own draft while earlier services await a reply.
    Application settings_app(c);
    settings_app.receive(snapshot(f));
    std::vector<Command> changes;
    settings_app.send = [&](Command cmd)
    {
        changes.push_back(cmd);
        return changes.size();
    };
    settings_app.open(Page::quick);
    in = neutral;
    in.nav = hui::Direction::down;
    tick(settings_app, in, 1000000);
    in.nav = hui::Direction::right;
    tick(settings_app, in, 1100000);
    in.nav = hui::Direction::down;
    tick(settings_app, in, 1200000);
    CHECK(changes.size() == 1);
    in.nav = hui::Direction::right;
    tick(settings_app, in, 1300000);
    tick(settings_app, neutral, 1800000);
    CHECK(changes.size() == 2);
    CHECK(changes[1].entity == tv.role("oled_light"));
    CHECK(changes[1].data["value"] == 70);
    auto refused = snapshot(f);
    refused.results = {{2, false, "Fixture rejection"}};
    settings_app.receive(refused);
    CHECK(settings_app.notice() == "Fixture rejection");
    // Focus loss blocks a held confirm until release, including local menus.
    Application focus_app(c);
    focus_app.receive(snapshot(f));
    focus_app.open(Page::apps);
    int focus_sends = 0;
    focus_app.send = [&](Command) { return ++focus_sends; };
    in = input(hui::Action::confirm);
    in.focus_lost = true;
    tick(focus_app, in);
    in.focus_lost = false;
    tick(focus_app, in);
    CHECK(focus_sends == 0);
    tick(focus_app, neutral);
    tick(focus_app, input(hui::Action::confirm));
    CHECK(focus_sends == 1);
    // Decode the shipped icon; malformed bytes never become a texture.
    std::string icon_bytes;
    CHECK(hui::save::read_file("sce_sys/icon0.png", &icon_bytes));
    std::vector<std::uint8_t> bytes(icon_bytes.begin(), icon_bytes.end()), pixels;
    int width = 0, height = 0;
    CHECK(hui::afterglow::decode_image(bytes, pixels, width, height));
    CHECK(width == 512 && height == 512);
    CHECK(!hui::afterglow::decode_image({1, 2, 3}, pixels, width, height));
    const auto dir = std::filesystem::temp_directory_path() / "prism-host-test";
    std::filesystem::create_directories(dir);
    Resources resources;
    CHECK(resources.start(dir.string()));
    resources.save(c);
    resources.stop();
    std::string error;
    CHECK(load_config(dir.string(), "/nonexistent", error).token == c.token);
    CHECK(error.empty());
    std::filesystem::remove_all(dir);
    std::printf("host tests passed (%d checks)\n", checks);
}
