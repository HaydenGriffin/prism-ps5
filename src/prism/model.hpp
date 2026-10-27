// Prism - Glasshouse TV discovery and controller-independent TV model.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "third_party/json.hpp"
#include <map>
#include <string>
#include <vector>

namespace prism
{
using Json = nlohmann::json;
std::string text(const Json &j, const std::string &key, const std::string &fallback = "");
std::vector<std::string> strings(const Json &j);
const Json &field(const Json &j, const std::string &key);
struct Config
{
    std::string host, token, device, input, input_app;
    int port = 8123;
    bool reduced_motion = false, diagnostics = false, vibration = false;
    std::vector<std::string> favourites, recent;
    bool configured() const;
    Json json() const;
    static Config parse(const Json &j);
};
struct Tv
{
    std::string name, device;
    std::map<std::string, std::string> roles;
    std::string role(const std::string &key) const;
    bool usable() const;
};
std::vector<Tv> discover(const Json &entities, const Json &devices);
// Keeps the registry fields discovery reads, for entities that belong to a device.
Json device_entities(const Json &entities);
// Whether a registry event can change the discovered TVs. Unrelated changes are ignored, so
// a busy Home Assistant does not interrupt the remote.
bool affects_tvs(const std::string &event_type, const Json &data, const Json &entities,
                 const std::vector<Tv> &tvs);
struct Entity
{
    std::string state;
    Json attributes = Json::object();
    bool available() const;
};
class StateTable
{
  public:
    void apply(const Json &event);
    const Entity &get(const std::string &id) const;
    void clear()
    {
        states_.clear();
    }

  private:
    std::map<std::string, Entity> states_;
};
struct App
{
    std::string id, title, icon, color;
};
struct Setting
{
    std::string role, label;
    bool number = false;
};
const std::vector<Setting> &settings();
std::vector<App> catalog(const Tv &tv, const StateTable &states);
bool launchable(const std::string &id);
// The LG's own screens, which are not in the app catalogue (any com.webos. or com.palm. one is
// at least "LG screen"); "" for anything else.
std::string system_title(const std::string &id);
std::string target(const Tv &tv, const StateTable &states);
bool overlay_on_input(const Tv &tv, const StateTable &states, const std::string &input_app);
bool controls_lg(const Tv &tv, const StateTable &states, const std::string &input_app);
std::vector<std::string> choices(const Entity &entity);
std::string step_setting(const Entity &entity, const std::string &current, int direction,
                         bool number);
struct Command
{
    std::string entity, service;
    Json data = Json::object();
    bool transient = false;
    std::uint64_t ticket = 0;
    Json wire(int id) const;
};
Command press(const Tv &tv, const std::string &button);
Command select(const std::string &entity, const std::string &option);
Command set_number(const std::string &entity, const std::string &value);
} // namespace prism
