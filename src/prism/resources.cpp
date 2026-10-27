// Prism - Disk writes and image decoding never run on the rendering thread.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prism/resources.hpp"
#include "afterglow/artwork.hpp"
#include "core/save_file.hpp"
#include "platform/ps5/system.hpp"
#include <algorithm>

namespace prism
{
Config load_config(const std::string &directory, const std::string &bootstrap, std::string &error)
{
    std::string data;
    if (hui::save::read_file(directory + "/settings.json", &data, 65536) ||
        hui::save::read_file(bootstrap, &data, 65536))
    {
        auto j = Json::parse(data, nullptr, false);
        if (j.is_object())
            return Config::parse(j);
        error = "Saved settings could not be read. Enter them again in Setup.";
    }
    return {};
}
Resources::~Resources()
{
    stop();
}
template <void (Resources::*Work)()> bool Resources::spawn(pthread_t &thread, Resources *owner)
{
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0)
        return false;
    int rc = pthread_attr_setstacksize(&attr, 1024 * 1024);
    if (!rc)
        rc = pthread_create(
            &thread, &attr,
            [](void *p) -> void *
            {
                (static_cast<Resources *>(p)->*Work)();
                return nullptr;
            },
            owner);
    pthread_attr_destroy(&attr);
    return rc == 0;
}
bool Resources::start(std::string directory)
{
    directory_ = std::move(directory);
    stopping_ = false;
    started_ = spawn<&Resources::run>(thread_, this);
    if (!started_)
        status_ = "Could not start storage worker. Changes cannot be saved.";
    // Icon downloads wait on the TV's icon server, so they get their own worker and never
    // hold up a settings save.
    icons_started_ = spawn<&Resources::fetch_icons>(icon_thread_, this);
    return started_;
}
void Resources::stop()
{
    stopping_ = true;
    if (started_)
        pthread_join(thread_, nullptr);
    if (icons_started_)
        pthread_join(icon_thread_, nullptr);
    started_ = false;
    icons_started_ = false;
}
void Resources::save(const Config &c)
{
    std::lock_guard lock(mutex_);
    save_ = c.json().dump();
    dirty_ = true;
    status_ = "Saving…";
}
void Resources::diagnostic(Json value)
{
    std::lock_guard lock(mutex_);
    diagnostic_ = value.dump(2);
    diagnostic_dirty_ = true;
}
std::string Resources::save_status() const
{
    std::lock_guard lock(mutex_);
    return status_;
}
void Resources::request_icon(const std::string &url)
{
    std::lock_guard lock(mutex_);
    if (!url.starts_with("http://") || url.size() > 2048 || requested_.size() >= 128 ||
        requested_.contains(url))
        return;
    // A failed icon is tried again after a minute, not on every frame.
    if (auto failed = failed_.find(url);
        failed != failed_.end() && hui::sys::monotonic_us() < failed->second)
        return;
    requested_.insert(url);
    urls_.push_back(url);
}
bool Resources::take_icon(Icon &icon)
{
    std::lock_guard lock(mutex_);
    if (icons_.empty())
        return false;
    icon = std::move(icons_.front());
    icons_.pop_front();
    return true;
}
void Resources::run()
{
    hui::save::ensure_directory(directory_);
    for (;;)
    {
        std::string save, diagnostic;
        bool dirty = false;
        {
            std::lock_guard lock(mutex_);
            dirty = dirty_;
            if (dirty)
            {
                save = save_;
                dirty_ = false;
            }
            if (diagnostic_dirty_)
            {
                diagnostic = diagnostic_;
                diagnostic_dirty_ = false;
            }
        }
        if (dirty)
        {
            auto error = hui::save::write_atomic(directory_ + "/settings.json", save, 0600);
            std::lock_guard lock(mutex_);
            status_ =
                error.empty() ? "Saved on this PS5" : "Could not save settings. Check app storage.";
        }
        if (!diagnostic.empty())
            // Counters and status only, no token: readable over FTP for tools/probe.py.
            hui::save::write_atomic(directory_ + "/status.json", diagnostic, 0644);
        if (stopping_)
        {
            std::lock_guard lock(mutex_);
            if (!dirty_)
                break;
            else
                continue;
        }
        hui::sys::sleep_us(20000);
    }
}
void Resources::fetch_icons()
{
    while (!stopping_)
    {
        std::string url;
        {
            std::lock_guard lock(mutex_);
            if (!urls_.empty() && icons_.size() < 8)
            {
                url = urls_.front();
                urls_.pop_front();
            }
        }
        if (url.empty())
        {
            hui::sys::sleep_us(20000);
            continue;
        }
        Icon icon;
        if (!load_icon(url, icon))
        {
            std::lock_guard lock(mutex_);
            requested_.erase(url);
            failed_[url] = hui::sys::monotonic_us() + 60000000;
            continue;
        }
        std::lock_guard lock(mutex_);
        icons_.push_back(std::move(icon));
    }
}
bool Resources::load_icon(const std::string &url, Icon &icon)
{
    std::vector<std::uint8_t> bytes, rgba;
    int w = 0, h = 0;
    if (!hui::afterglow::fetch_image(
            url, [&] { return stopping_.load(); }, bytes) ||
        !hui::afterglow::decode_image(bytes, rgba, w, h))
        return false;
    icon.url = url;
    float scale = std::min(1.0f, 256.0f / static_cast<float>(std::max(w, h)));
    icon.width = std::max(1, static_cast<int>(w * scale));
    icon.height = std::max(1, static_cast<int>(h * scale));
    icon.rgba.resize(static_cast<std::size_t>(icon.width * icon.height * 4));
    for (int y = 0; y < icon.height; y++)
        for (int x = 0; x < icon.width; x++)
            for (int c = 0; c < 4; c++)
                icon.rgba[static_cast<std::size_t>((y * icon.width + x) * 4 + c)] =
                    rgba[static_cast<std::size_t>(
                        ((y * h / icon.height) * w + x * w / icon.width) * 4 + c)];
    return true;
}
} // namespace prism
