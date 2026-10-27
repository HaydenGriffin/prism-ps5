// Prism - Reconnecting Home Assistant session, bounded commands and acknowledgements.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "prism/model.hpp"
#include <atomic>
#include <deque>
#include <mutex>
#include <pthread.h>

namespace prism
{
struct Result
{
    std::uint64_t ticket = 0;
    bool ok = false;
    std::string message;
};
struct Snapshot
{
    bool connected = false, ready = false;
    std::uint64_t generation = 0, revision = 0;
    std::string status = "Enter your Home Assistant address and token";
    std::vector<Tv> tvs;
    StateTable states;
    std::vector<Result> results;
};
class Client
{
  public:
    ~Client();
    bool start(const Config &config);
    void stop();
    Snapshot snapshot() const;
    // Zero means refused. Accepted commands are never replayed after reconnect.
    std::uint64_t send(Command command);
    void cancel_transient();

  private:
    struct Queued
    {
        Command command;
        std::int64_t expires = 0;
    };
    void run();
    mutable std::mutex mutex_;
    Snapshot snapshot_;
    Config config_;
    std::deque<Queued> queue_;
    std::atomic<bool> stopping_{true};
    pthread_t thread_{};
    bool started_ = false;
    std::uint64_t ticket_ = 0;
};
} // namespace prism
