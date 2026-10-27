// Prism - Home Assistant websocket authentication, registry refresh and state subscription.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prism/client.hpp"
#include "afterglow/ma_socket.hpp"
#include "platform/ps5/system.hpp"
#include <algorithm>
#include <set>

namespace prism
{
using hui::sys::monotonic_us;
Client::~Client()
{
    stop();
}
Snapshot Client::snapshot() const
{
    std::lock_guard lock(mutex_);
    return snapshot_;
}
bool Client::start(const Config &config)
{
    stop();
    config_ = config;
    {
        std::lock_guard lock(mutex_);
        auto revision = snapshot_.revision + 1;
        auto generation = snapshot_.generation;
        snapshot_ = Snapshot{};
        snapshot_.revision = revision;
        snapshot_.generation = generation;
        snapshot_.status = config.configured() ? "Connecting to Home Assistant"
                                               : "Enter a numeric IPv4 address, port and token";
    }
    if (!config.configured())
        return false;
    stopping_ = false;
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0)
        return false;
    int rc = pthread_attr_setstacksize(&attr, 1024 * 1024);
    if (!rc)
        rc = pthread_create(
            &thread_, &attr,
            [](void *p) -> void *
            {
                static_cast<Client *>(p)->run();
                return nullptr;
            },
            this);
    pthread_attr_destroy(&attr);
    started_ = rc == 0;
    if (!started_)
    {
        stopping_ = true;
        std::lock_guard lock(mutex_);
        snapshot_.status = "Could not start the network worker. Restart Prism.";
    }
    return started_;
}
void Client::stop()
{
    stopping_ = true;
    if (started_)
        pthread_join(thread_, nullptr);
    started_ = false;
    std::lock_guard lock(mutex_);
    queue_.clear();
    snapshot_.ready = false;
    snapshot_.connected = false;
}
std::uint64_t Client::send(Command c)
{
    std::lock_guard lock(mutex_);
    if (!snapshot_.ready || queue_.size() >= 16 || c.entity.empty() || c.wire(1).is_null())
        return 0;
    bool allowed = false;
    for (const auto &tv : snapshot_.tvs)
        if (tv.device == config_.device || config_.device.empty())
            for (const auto &[role, id] : tv.roles)
                if (id == c.entity)
                    allowed = true;
    if (!allowed)
        return 0;
    c.ticket = ++ticket_;
    queue_.push_back({c, monotonic_us() + (c.transient ? 350000 : 5000000)});
    return c.ticket;
}
void Client::cancel_transient()
{
    std::lock_guard lock(mutex_);
    std::erase_if(queue_, [](const Queued &q) { return q.command.transient; });
}
void Client::run()
{
    int attempts = 0;
    while (!stopping_)
    {
        Snapshot state;
        {
            std::lock_guard lock(mutex_);
            state.generation = snapshot_.generation + 1;
            state.revision = snapshot_.revision;
        }
        auto publish = [&]
        {
            std::lock_guard lock(mutex_);
            state.revision++;
            snapshot_ = state;
        };
        auto result = [&](std::uint64_t ticket, bool ok, std::string message)
        {
            state.results.push_back({ticket, ok, std::move(message)});
            if (state.results.size() > 64)
                state.results.erase(state.results.begin());
        };
        state.status = "Connecting to Home Assistant";
        publish();
        hui::afterglow::ma_socket::Socket socket(stopping_);
        int next = 1, entity_id = 0, device_id = 0, subscription = 0, subscription_ack = 0,
            ping_id = 0;
        bool authorized = false, invalid_auth = false, have_entities = false, have_devices = false,
             subscribed = false, have_initial = false;
        Json entities = Json::array(), devices = Json::array();
        std::set<std::string> watched;
        std::map<int, std::pair<std::uint64_t, std::int64_t>> pending;
        std::map<int, std::string> requests;
        auto deadline = monotonic_us() + 15000000, last_ping = monotonic_us();
        std::int64_t ping_deadline = 0, registry_due = 0;
        auto send = [&](Json j)
        {
            j["id"] = next++;
            return socket.send_text(j.dump()) ? next - 1 : 0;
        };
        auto registries = [&]
        {
            have_entities = false;
            have_devices = false;
            entity_id = send({{"type", "config/entity_registry/list"}});
            device_id = send({{"type", "config/device_registry/list"}});
            deadline = monotonic_us() + 15000000;
        };
        auto subscribe = [&]
        {
            auto tvs = discover(entities, devices);
            std::set<std::string> ids;
            for (const auto &tv : tvs)
                for (const auto &[role, id] : tv.roles)
                    ids.insert(id);
            if (subscription && ids == watched)
            {
                // Same TV entities: keep the live subscription and only refresh names.
                state.tvs = std::move(tvs);
                if (state.ready)
                    deadline = 0;
                publish();
                return;
            }
            watched = ids;
            if (subscription)
                send({{"type", "unsubscribe_events"}, {"subscription", subscription}});
            state.ready = false;
            state.states.clear();
            state.tvs = std::move(tvs);
            subscribed = false;
            have_initial = false;
            deadline = monotonic_us() + 15000000;
            {
                std::lock_guard lock(mutex_);
                for (const auto &q : queue_)
                    result(q.command.ticket, false, "TV registry changed; try again");
                queue_.clear();
            }
            if (ids.empty())
            {
                subscription = 0;
                deadline = 0;
                state.status = "No Glasshouse TV found. Enable its Home Assistant entities.";
            }
            else
            {
                subscription = send({{"type", "subscribe_entities"}, {"entity_ids", ids}});
                subscription_ack = subscription;
                state.status = "Reading the TVs";
            }
            publish();
        };
        socket.handler = [&](int opcode, const std::uint8_t *bytes, std::size_t size)
        {
            if (opcode != 1)
                return true;
            auto j = Json::parse(bytes, bytes + size, nullptr, false);
            if (!j.is_object())
                return false;
            auto type = text(j, "type");
            int id = 0;
            const auto &idj = field(j, "id");
            if (idj.is_number_integer() && idj >= 0 && idj <= 2147483647)
                id = idj.get<int>();
            if (type == "auth_required")
                return socket.send_text(
                    Json{{"type", "auth"}, {"access_token", config_.token}}.dump());
            if (type == "auth_invalid")
            {
                invalid_auth = true;
                state.status = "Home Assistant rejected the token. Replace it in Setup.";
                return false;
            }
            if (type == "auth_ok")
            {
                authorized = true;
                state.connected = true;
                registries();
                for (const char *event : {"entity_registry_updated", "device_registry_updated"})
                {
                    int request = send({{"type", "subscribe_events"}, {"event_type", event}});
                    requests[request] = "registry_watch";
                }
                publish();
                return true;
            }
            if (type == "pong" && id == ping_id)
            {
                ping_deadline = 0;
                return true;
            }
            if (type == "event")
            {
                if (id == subscription)
                {
                    state.states.apply(field(j, "event"));
                    have_initial = true;
                    state.ready = subscribed;
                    if (state.ready)
                        deadline = 0;
                    state.status = state.ready ? "Connected" : "Reading the TVs";
                    publish();
                }
                else
                {
                    const auto &event = field(j, "event");
                    auto kind = text(event, "event_type");
                    if ((kind == "entity_registry_updated" || kind == "device_registry_updated") &&
                        affects_tvs(kind, field(event, "data"), entities, state.tvs))
                        registry_due = monotonic_us() + 500000;
                }
                return true;
            }
            if (type != "result")
                return true;
            const bool ok = field(j, "success") == true;
            if (pending.contains(id))
            {
                result(pending[id].first, ok,
                       ok ? "Accepted by Home Assistant"
                          : "Home Assistant refused the command (" +
                                text(field(j, "error"), "code", "service error") + ")");
                pending.erase(id);
                publish();
                return true;
            }
            if (id == entity_id || id == device_id)
            {
                if (!ok)
                {
                    state.status =
                        "Cannot read TV registries. Check the Home Assistant user's permissions.";
                    publish();
                    return false;
                }
                if (!field(j, "result").is_array())
                    return false;
                if (id == entity_id)
                {
                    entities = device_entities(field(j, "result"));
                    have_entities = true;
                }
                else
                {
                    devices = field(j, "result");
                    have_devices = true;
                }
                if (have_entities && have_devices)
                {
                    subscribe();
                }
            }
            else if (id == subscription_ack)
            {
                if (!ok)
                {
                    state.status = "Cannot subscribe to TV states. Check entity access.";
                    publish();
                    return false;
                }
                subscribed = true;
                state.ready = have_initial;
                state.status = state.ready ? "Connected" : "Reading the TVs";
                if (state.ready)
                    deadline = 0;
                attempts = 0;
                publish();
            }
            return true;
        };
        bool open =
            socket.open(config_.host, static_cast<std::uint16_t>(config_.port), "/api/websocket");
        while (open && !stopping_)
        {
            if (!socket.tick())
                break;
            const auto now = monotonic_us();
            if ((deadline && now > deadline) || (ping_deadline && now > ping_deadline))
            {
                state.status = "Home Assistant timed out; reconnecting";
                break;
            }
            if (authorized && now - last_ping > 15000000)
            {
                ping_id = send({{"type", "ping"}});
                last_ping = now;
                ping_deadline = now + 10000000;
                if (!ping_id)
                    break;
            }
            if (authorized && registry_due && now >= registry_due)
            {
                registry_due = 0;
                registries();
            }
            if (state.ready && pending.size() < 8)
            {
                Queued q;
                bool have = false;
                {
                    std::lock_guard lock(mutex_);
                    if (!queue_.empty())
                    {
                        q = queue_.front();
                        queue_.pop_front();
                        have = true;
                    }
                }
                if (have)
                {
                    if (now > q.expires)
                    {
                        result(q.command.ticket, false, "Command expired; press again");
                        publish();
                    }
                    else
                    {
                        int id = next++;
                        if (!socket.send_text(q.command.wire(id).dump()))
                        {
                            result(q.command.ticket, false,
                                   "Connection lost; command outcome unknown");
                            break;
                        }
                        pending[id] = {q.command.ticket, now + 5000000};
                    }
                }
            }
            bool expired = false;
            for (auto it = pending.begin(); it != pending.end();)
                if (now > it->second.second)
                {
                    result(it->second.first, false,
                           "No command acknowledgement; check the TV before retrying");
                    it = pending.erase(it);
                    expired = true;
                }
                else
                    ++it;
            if (expired)
                publish();
            hui::sys::sleep_us(10000);
        }
        socket.close();
        for (const auto &[id, p] : pending)
            result(p.first, false, "Connection lost; command outcome unknown");
        {
            std::lock_guard lock(mutex_);
            for (const auto &q : queue_)
                result(q.command.ticket, false, "Connection lost; command cancelled");
            queue_.clear();
        }
        state.ready = false;
        state.connected = false;
        state.states.clear();
        if (invalid_auth)
        {
            publish();
            return;
        }
        if (state.status == "Connected" || state.status == "Connecting to Home Assistant")
            state.status = "Connection lost. Retrying automatically.";
        publish();
        const int seconds = std::min(30, 1 << std::min(attempts++, 5));
        for (int i = 0; i < seconds * 20 && !stopping_; ++i)
            hui::sys::sleep_us(50000);
    }
}
} // namespace prism
