// Prism - Background preferences, diagnostic evidence and icon loading.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "prism/model.hpp"
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <pthread.h>
#include <set>

namespace prism
{
struct Icon
{
    std::string url;
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
};
class Resources
{
  public:
    ~Resources();
    bool start(std::string directory);
    void save(const Config &config);
    void diagnostic(Json value);
    void request_icon(const std::string &url);
    bool take_icon(Icon &icon);
    std::string save_status() const;
    void stop();

  private:
    void run();
    void fetch_icons();
    bool load_icon(const std::string &url, Icon &icon);
    template <void (Resources::*Work)()> static bool spawn(pthread_t &thread, Resources *owner);
    std::string directory_, save_, diagnostic_, status_;
    bool dirty_ = false, diagnostic_dirty_ = false, started_ = false, icons_started_ = false;
    std::set<std::string> requested_;
    std::map<std::string, std::int64_t> failed_;
    std::deque<std::string> urls_;
    std::deque<Icon> icons_;
    mutable std::mutex mutex_;
    std::atomic<bool> stopping_{true};
    pthread_t thread_{}, icon_thread_{};
};
Config load_config(const std::string &directory, const std::string &bootstrap, std::string &error);
} // namespace prism
