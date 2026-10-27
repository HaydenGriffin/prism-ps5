// Prism - PS5 native entry point; only foreground input is forwarded.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prism/application.hpp"
#include "prism/resources.hpp"
#include "core/save_file.hpp"
#include "gfx/renderer.hpp"
#include "platform/ps5/display_egl.hpp"
#include "platform/ps5/pad.hpp"
#include "platform/ps5/system.hpp"
#include <algorithm>
#include <array>
#include <cstdio>

int main()
{
    using namespace hui;
    save::ensure_directory("/download0/prism");
    std::string error;
    auto config = prism::load_config("/download0/prism", "/app0/prism.conf", error);
    ps5::Display display;
    if (!display.open())
    {
        sys::log("[Prism] Display could not open");
        sys::quit();
        return 1;
    }
    gfx::Renderer renderer;
    if (!renderer.init())
    {
        sys::quit();
        return 1;
    }
    gfx::Font regular, bold, display_face;
    ui::Fonts fonts;
    auto font = [&](const char *name, gfx::Font &face, ui::FontRef &ref)
    {
        std::string bytes;
        if (!save::read_file(std::string("/app0/assets/fonts/") + name + ".huifont", &bytes) ||
            !face.load(bytes))
            return false;
        ref = {&face, renderer.batch().create_font_texture(face)};
        return true;
    };
    if (!font("atkinson-regular", regular, fonts.regular) ||
        !font("atkinson-semibold", bold, fonts.semibold) ||
        !font("lexend-semibold", display_face, fonts.display))
    {
        sys::log("[Prism] Font assets missing");
        sys::quit();
        return 1;
    }
    fonts.mono = fonts.regular;
    fonts.hand = fonts.regular;
    fonts.pixel = fonts.regular;
    ps5::Pad pad;
    pad.open();
    InputTracker tracker;
    InputSettings input_settings;
    input_settings.repeat_interval_us = 150000;
    tracker.set_settings(input_settings);
    auto ime = ps5::make_ime_backend(pad.user());
    ps5::Keyboard keyboard(*ime);
    prism::Client client;
    prism::Resources resources;
    resources.start("/download0/prism");
    prism::Application app(config);
    app.send = [&](prism::Command c) { return client.send(std::move(c)); };
    app.save = [&](const prism::Config &c) { resources.save(c); };
    app.reconnect = [&](const prism::Config &c) { client.start(c); };
    app.cancel_transient = [&] { client.cancel_transient(); };
    app.request_icon = [&](const std::string &url) { resources.request_icon(url); };
    client.start(config);
    std::int64_t previous = sys::monotonic_us(), mirror = 0, diagnostic = 0;
    std::uint64_t revision = ~std::uint64_t{0};
    bool remote_light = false, pad_connected = true;
    bool splash = false, was_keyboard = false;
    gfx::DrawList list;
    ui::Feedback feedback;
    std::vector<GLuint> textures;
    while (!app.quitting())
    {
        const auto now = sys::monotonic_us();
        const float dt = std::clamp(static_cast<float>(now - previous) / 1000000.0f, 0.0f, 0.1f);
        previous = now;
        std::array<PadSample, 64> samples;
        auto count = pad.read(samples);
        auto input = tracker.update(std::span<const PadSample>(samples.data(), count),
                                    static_cast<std::uint64_t>(now));
        // A reconnected controller shows the system colour again.
        if (input.connected && !pad_connected)
            pad.forget_light_bar();
        pad_connected = input.connected;
        if (now >= mirror)
        {
            auto state = client.snapshot();
            if (state.revision != revision)
            {
                revision = state.revision;
                app.receive(std::move(state));
            }
            // A save attempt is newer news than a settings file that could not be read.
            const auto saved = resources.save_status();
            if (!saved.empty())
                error.clear();
            app.storage_status(error.empty() ? saved : error);
            mirror = now + 100000;
        }
        ps5::KeyboardOutcome outcome;
        bool completed = keyboard.poll(outcome);
        if (completed)
        {
            app.keyboard_result(outcome);
            std::fill(outcome.text.begin(), outcome.text.end(), '\0');
        }
        if (keyboard.active() || was_keyboard || completed)
        {
            input = InputFrame{};
            input.focus_lost = true;
            client.cancel_transient();
        }
        was_keyboard = keyboard.active();
        feedback.clear();
        app.update(input, dt, now, feedback);
        if (auto request = app.take_keyboard())
        {
            keyboard.open(*request);
            std::fill(request->initial.begin(), request->initial.end(), '\0');
        }
        if (app.config().vibration)
        {
            if (feedback.rumble_strength > 0)
                pad.rumble(feedback.rumble_strength, feedback.rumble_seconds);
            else if (!feedback.cues.empty())
                pad.rumble(0.12f, 0.035f);
        }
        pad.tick(dt);
        if (app.remote_mode())
        {
            pad.set_light_bar(123, 218, 250);
            remote_light = true;
        }
        else if (remote_light)
        {
            pad.reset_light_bar();
            remote_light = false;
        }
        prism::Icon icon;
        if (resources.take_icon(icon))
        {
            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, icon.width, icon.height, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, icon.rgba.data());
            textures.push_back(texture);
            app.icon(icon.url, texture);
        }
        if (app.config().diagnostics && now >= diagnostic)
        {
            resources.diagnostic(app.diagnostic());
            diagnostic = now + 1000000;
        }
        list.clear();
        ui::Canvas canvas{list, fonts, 0, static_cast<float>(now) / 1000000.0f};
        app.draw(canvas);
        renderer.begin();
        renderer.draw(list);
        renderer.present(0, display.width(), display.height());
        if (!display.swap())
            break;
        if (!splash)
        {
            sys::hide_splash_screen();
            splash = true;
        }
    }
    keyboard.abort();
    client.stop();
    resources.stop();
    pad.reset_light_bar();
    pad.close();
    for (auto texture : textures)
        glDeleteTextures(1, &texture);
    renderer.release();
    display.close();
    sys::quit();
    return 0;
}
