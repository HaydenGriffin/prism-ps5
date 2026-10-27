// Prism - Glasshouse entity roles, live states and bounded settings.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prism/model.hpp"
#include "prism/decimal.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cmath>
#include <cstdlib>
#include <set>

namespace prism
{
const Json &field(const Json &j, const std::string &key)
{
    static const Json empty;
    if (!j.is_object())
        return empty;
    auto it = j.find(key);
    return it == j.end() ? empty : *it;
}
std::string text(const Json &j, const std::string &key, const std::string &fallback)
{
    const auto &v = field(j, key);
    return v.is_string() ? v.get<std::string>() : fallback;
}
std::vector<std::string> strings(const Json &j)
{
    std::vector<std::string> out;
    if (j.is_array())
        for (const auto &v : j)
            if (v.is_string())
                out.push_back(v.get<std::string>());
    return out;
}
bool Config::configured() const
{
    in_addr address{};
    return inet_pton(AF_INET, host.c_str(), &address) == 1 && port > 0 && port <= 65535 &&
           !token.empty() && token.size() <= 2048 &&
           token.find_first_of("\r\n") == std::string::npos;
}
Json Config::json() const
{
    return {{"host", host},
            {"port", port},
            {"token", token},
            {"device", device},
            {"input", input},
            {"input_app", input_app},
            {"favourites", favourites},
            {"recent", recent},
            {"reduced_motion", reduced_motion},
            {"vibration", vibration},
            {"diagnostics", diagnostics}};
}
Config Config::parse(const Json &j)
{
    Config c;
    c.host = text(j, "host");
    c.token = text(j, "token");
    c.device = text(j, "device");
    c.input = text(j, "input");
    c.input_app = text(j, "input_app");
    const auto &port = field(j, "port");
    if (port.is_number_integer() && port >= 1 && port <= 65535)
        c.port = port.get<int>();
    c.reduced_motion = field(j, "reduced_motion") == true;
    c.diagnostics = field(j, "diagnostics") == true;
    c.vibration = field(j, "vibration") == true;
    c.favourites = strings(field(j, "favourites"));
    c.recent = strings(field(j, "recent"));
    if (c.favourites.size() > 128)
        c.favourites.resize(128);
    if (c.recent.size() > 32)
        c.recent.resize(32);
    return c;
}
std::string Tv::role(const std::string &key) const
{
    auto it = roles.find(key);
    return it == roles.end() ? "" : it->second;
}
bool Tv::usable() const
{
    for (const auto *key : {"active_app", "app", "input_source", "remote_up", "remote_down",
                            "remote_left", "remote_right", "remote_ok", "remote_back"})
        if (role(key).empty())
            return false;
    return true;
}
Json device_entities(const Json &entities)
{
    Json out = Json::array();
    if (!entities.is_array())
        return out;
    for (const auto &e : entities)
        if (!text(e, "device_id").empty())
            out.push_back({{"entity_id", text(e, "entity_id")},
                           {"unique_id", text(e, "unique_id")},
                           {"device_id", text(e, "device_id")},
                           {"disabled_by", field(e, "disabled_by")}});
    return out;
}
bool affects_tvs(const std::string &event_type, const Json &data, const Json &entities,
                 const std::vector<Tv> &tvs)
{
    if (tvs.empty() || text(data, "action") == "create")
        return true;
    std::set<std::string> devices;
    for (const auto &tv : tvs)
        devices.insert(tv.device);
    if (event_type == "device_registry_updated")
        return devices.contains(text(data, "device_id"));
    for (const auto *key : {"entity_id", "old_entity_id"})
    {
        const auto id = text(data, key);
        if (id.empty())
            continue;
        for (const auto &tv : tvs)
            for (const auto &[role, entity] : tv.roles)
                if (entity == id)
                    return true;
        if (entities.is_array())
            for (const auto &e : entities)
                if (text(e, "entity_id") == id && devices.contains(text(e, "device_id")))
                    return true;
    }
    return false;
}
std::vector<Tv> discover(const Json &entities, const Json &devices)
{
    std::vector<Tv> out;
    if (!entities.is_array() || !devices.is_array())
        return out;
    std::map<std::string, std::string> names;
    for (const auto &d : devices)
        names[text(d, "id")] = text(d, "name_by_user", text(d, "name", "LG TV"));
    std::set<std::string> seen;
    for (const auto &e : entities)
    {
        auto id = text(e, "entity_id"), uid = text(e, "unique_id"), dev = text(e, "device_id");
        if (dev.empty() || !field(e, "disabled_by").is_null() || !id.starts_with("select.") ||
            !uid.ends_with("_app"))
            continue;
        const auto prefix = uid.substr(0, uid.size() - 3);
        if (!seen.insert(dev + prefix).second)
            continue;
        Tv tv;
        tv.device = dev;
        tv.name = names.contains(dev) ? names[dev] : "LG TV";
        for (const auto &r : entities)
        {
            const auto rid = text(r, "entity_id"), unique = text(r, "unique_id");
            if (text(r, "device_id") != dev || !field(r, "disabled_by").is_null() ||
                !unique.starts_with(prefix))
                continue;
            const auto role = unique.substr(prefix.size());
            std::string domain = "button";
            if (role == "active_app")
                domain = "sensor";
            else if (role == "app" || role == "input_source")
                domain = "select";
            else if (role == "screen_notification" || role == "keyboard_text")
                domain = "text";
            else
                for (const auto &s : settings())
                    if (role == s.role)
                        domain = s.number ? "number" : "select";
            if (rid.starts_with(domain + ".") && (!tv.roles.contains(role) || rid < tv.roles[role]))
                tv.roles[role] = rid;
        }
        out.push_back(std::move(tv));
    }
    return out;
}
bool Entity::available() const
{
    return !state.empty() && state != "unavailable" && state != "unknown";
}
const Entity &StateTable::get(const std::string &id) const
{
    static const Entity absent;
    auto it = states_.find(id);
    return it == states_.end() ? absent : it->second;
}
void StateTable::apply(const Json &event)
{
    const auto &added = field(event, "a");
    if (added.is_object())
        for (auto it = added.begin(); it != added.end(); ++it)
        {
            Entity e;
            e.state = text(*it, "s");
            if (field(*it, "a").is_object())
                e.attributes = field(*it, "a");
            states_[it.key()] = std::move(e);
        }
    const auto &changed = field(event, "c");
    if (changed.is_object())
        for (auto it = changed.begin(); it != changed.end(); ++it)
        {
            auto found = states_.find(it.key());
            if (found == states_.end())
                continue;
            auto &e = found->second;
            const auto &plus = field(*it, "+");
            if (field(plus, "s").is_string())
                e.state = text(plus, "s");
            if (field(plus, "a").is_object())
                e.attributes.update(field(plus, "a"));
            for (const auto &key : strings(field(field(*it, "-"), "a")))
                e.attributes.erase(key);
        }
    for (const auto &id : strings(field(event, "r")))
        states_.erase(id);
}
const std::vector<Setting> &settings()
{
    static const std::vector<Setting> rows = {{"input_source", "Input"},
                                              {"picture_mode", "Picture mode"},
                                              {"oled_light", "OLED brightness", true},
                                              {"energy_saving", "Energy saving"},
                                              {"sound_mode", "Sound mode"},
                                              {"sleep_timer", "Sleep timer"},
                                              {"oled_logo_dimming", "Logo dimming"}};
    return rows;
}
std::string system_title(const std::string &id)
{
    // The same names as Prism for Android TV (RouteStatus.SYSTEM_TITLES).
    static const std::map<std::string, std::string> titles{
        {"com.webos.app.home", "LG Home"},
        {"com.palm.app.settings", "LG Settings"},
        {"com.webos.app.settings", "LG Settings"},
        {"com.webos.app.quicksettings", "Quick Settings"},
        {"com.webos.app.tvuserguide", "User Guide"},
        {"com.webos.app.voice", "LG search"},
        {"com.webos.systemui.notification", "LG message"}};
    auto it = titles.find(id);
    return it == titles.end() ? "" : it->second;
}
bool launchable(const std::string &id)
{
    return !id.empty() && !id.starts_with("com.webos.app.hdmi") && id != "com.webos.app.livetv" &&
           id != "com.palm.app.settings";
}
std::vector<App> catalog(const Tv &tv, const StateTable &states)
{
    std::vector<App> out;
    const auto &e = states.get(tv.role("app"));
    if (e.state.empty() || e.state == "unavailable")
        return out;
    const auto &ids = field(e.attributes, "app_ids");
    if (ids.is_object())
        for (auto it = ids.begin(); it != ids.end(); ++it)
            if (it.value().is_string())
            {
                auto id = it.value().get<std::string>();
                out.push_back({id, it.key(), text(field(e.attributes, "app_icons"), id),
                               text(field(e.attributes, "app_colors"), id)});
            }
    std::sort(out.begin(), out.end(),
              [](const App &a, const App &b)
              {
                  auto x = a.title, y = b.title;
                  for (auto &c : x)
                      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                  for (auto &c : y)
                      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                  return x < y;
              });
    return out;
}
std::string target(const Tv &tv, const StateTable &states)
{
    const auto &e = states.get(tv.role("active_app"));
    if (!e.available() || e.state == "off")
        return "";
    return text(e.attributes, "control_app_id", text(e.attributes, "app_id"));
}
bool overlay_on_input(const Tv &tv, const StateTable &states, const std::string &input_app)
{
    const auto &e = states.get(tv.role("active_app"));
    auto front = target(tv, states);
    return !input_app.empty() && text(e.attributes, "app_id") == input_app && !front.empty() &&
           (front != input_app || field(e.attributes, "keyboard_visible") == true);
}
bool controls_lg(const Tv &tv, const StateTable &states, const std::string &input_app)
{
    const auto &active = states.get(tv.role("active_app"));
    if (!active.available() || active.state == "off")
        return false;
    if (field(active.attributes, "keyboard_visible") == true)
        return true;
    auto id = target(tv, states);
    if (id.empty() || id == input_app || id.starts_with("com.webos.app.hdmi") ||
        id == "com.webos.app.livetv")
        return false;
    if (id.starts_with("com.webos.") || id.starts_with("com.palm.app."))
        return true;
    for (const auto &app : catalog(tv, states))
        if (app.id == id)
            return true;
    return false;
}
std::vector<std::string> choices(const Entity &entity)
{
    auto out = strings(field(entity.attributes, "options"));
    std::erase(out, "screen_off");
    return out;
}
std::string step_setting(const Entity &e, const std::string &current, int direction, bool number)
{
    if (!number)
    {
        auto opts = choices(e);
        if (opts.empty())
            return current;
        auto it = std::find(opts.begin(), opts.end(), current);
        int at = it == opts.end() ? 0
                                  : std::clamp(static_cast<int>(it - opts.begin()) + direction, 0,
                                               static_cast<int>(opts.size()) - 1);
        return opts[static_cast<std::size_t>(at)];
    }
    auto numeric = [&](const char *k, double fallback)
    {
        const auto &j = field(e.attributes, k);
        return j.is_number() ? j.get<double>() : fallback;
    };
    const double lo = numeric("min", 0), hi = numeric("max", 100),
                 quantum = std::max(0.01, numeric("step", 1));
    if (!std::isfinite(lo) || !std::isfinite(hi) || hi < lo)
        return current;
    const double value = parse_decimal(current.c_str()), increment = std::max(5.0, quantum);
    if (!std::isfinite(value) || !std::isfinite(quantum))
        return current;
    double next = std::clamp(
        lo + std::round((value + direction * increment - lo) / quantum) * quantum, lo, hi);
    return Json(next).dump();
}
Json Command::wire(int id) const
{
    auto dot = entity.find('.');
    if (dot == std::string::npos)
        return nullptr;
    auto domain = entity.substr(0, dot);
    if (!((domain == "button" && service == "press") ||
          (domain == "select" && service == "select_option") ||
          ((domain == "number" || domain == "text") && service == "set_value")))
        return nullptr;
    return {{"id", id},
            {"type", "call_service"},
            {"domain", domain},
            {"service", service},
            {"target", {{"entity_id", entity}}},
            {"service_data", data}};
}
Command press(const Tv &tv, const std::string &button)
{
    return {tv.role(button), "press", Json::object(), true};
}
Command select(const std::string &entity, const std::string &option)
{
    return {entity, "select_option", {{"option", option}}};
}
Command set_number(const std::string &entity, const std::string &value)
{
    return {entity, "set_value", {{"value", parse_decimal(value.c_str())}}};
}
} // namespace prism
