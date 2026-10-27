// Prism - Offscreen rendering of the actual native screens with illustrative fixture data, or
// with a snapshot of a real Home Assistant (PRISM_PREVIEW_DATA, from tools/capture-promo.py).
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prism/application.hpp"
#include "core/save_file.hpp"
#include "gfx/gl_program.hpp"
#include "gfx/renderer.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "third_party/stb_image.h"
#include <cstdlib>
#include <fstream>
#include <filesystem>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#include "../third_party/stb/stb_image_write.h"
#pragma clang diagnostic pop

int main()
{
    using namespace hui;
    auto display =
        eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    EGLint major, minor;
    if (!eglInitialize(display, &major, &minor) || !eglBindAPI(EGL_OPENGL_API))
        return 1;
    const EGLint attr[] = {EGL_CONTEXT_MAJOR_VERSION,
                           4,
                           EGL_CONTEXT_MINOR_VERSION,
                           5,
                           EGL_CONTEXT_OPENGL_PROFILE_MASK,
                           EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                           EGL_NONE};
    auto context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attr);
    if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context))
        return 2;
    gfx::set_glsl_prefix("#version 450 core");
    gfx::Renderer renderer;
    if (!renderer.init())
        return 3;
    gfx::Font regular, bold, display_font;
    ui::Fonts fonts;
    auto load = [&](const char *name, gfx::Font &font, ui::FontRef &ref)
    {
        std::string bytes;
        if (!save::read_file(std::string("assets/fonts/") + name + ".huifont", &bytes) ||
            !font.load(bytes))
            std::exit(4);
        ref = {&font, renderer.batch().create_font_texture(font)};
    };
    load("atkinson-regular", regular, fonts.regular);
    load("atkinson-semibold", bold, fonts.semibold);
    load("lexend-semibold", display_font, fonts.display);
    fonts.mono = fonts.regular;
    fonts.pixel = fonts.regular;
    fonts.hand = fonts.regular;
    const char *data = std::getenv("PRISM_PREVIEW_DATA");
    const bool live = data && *data;
    const std::filesystem::path data_path = live ? data : "tests/fixture.json";
    std::ifstream file(data_path);
    auto fixture = prism::Json::parse(file);
    prism::Snapshot snapshot;
    snapshot.ready = true;
    snapshot.connected = true;
    snapshot.generation = 1;
    snapshot.status = live ? "Connected" : "Connected · illustrative preview";
    snapshot.tvs = prism::discover(fixture["entities"], fixture["devices"]);
    snapshot.states.apply(fixture["states"]);
    prism::Application app(prism::Config::parse(fixture["config"]));
    app.receive(snapshot);
    app.send = [](prism::Command) { return 1; };
    // A live snapshot names its icons' files, beside it, by the URL the TV gives each app.
    if (live && fixture.contains("icons"))
        for (const auto &[url, name] : fixture["icons"].items())
        {
            int w = 0, h = 0, channels = 0;
            std::string bytes;
            if (!save::read_file((data_path.parent_path() / name.get<std::string>()).string(),
                                 &bytes, 8 * 1024 * 1024))
                continue;
            auto *pixels =
                stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(bytes.data()),
                                      static_cast<int>(bytes.size()), &w, &h, &channels, 4);
            if (!pixels)
                continue;
            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            stbi_image_free(pixels);
            app.icon(url, texture);
        }
    const std::string remote_app = fixture.value("remote_app", std::string("org.afterglow"));
    const std::string out = live ? "build/promo/" : "build/preview/";
    gfx::Canvas target;
    target.create(1920, 1080, 1);
    std::filesystem::create_directories(out);
    for (const auto &[name, page] :
         std::vector<std::pair<const char *, prism::Page>>{{"launcher", prism::Page::launcher},
                                                           {"apps", prism::Page::apps},
                                                           {"setup", prism::Page::setup},
                                                           {"quick-settings", prism::Page::quick},
                                                           {"favourites", prism::Page::favourites},
                                                           {"tvs", prism::Page::tvs},
                                                           {"remote", prism::Page::remote},
                                                           {"help", prism::Page::help},
                                                           {"quit", prism::Page::quit},
                                                           {"offline", prism::Page::launcher},
                                                           {"empty-apps", prism::Page::apps}})
    {
        if (std::string(name) == "remote")
        {
            snapshot.states.apply(
                {{"c",
                  {{snapshot.tvs[0].role("active_app"),
                    {{"+", {{"a", {{"app_id", remote_app}, {"control_app_id", remote_app}}}}}}}}}});
            app.receive(snapshot);
        }
        if (std::string(name) == "offline")
        {
            snapshot.ready = false;
            snapshot.connected = false;
            snapshot.states.clear();
            snapshot.status = "Connection lost. Retrying automatically.";
            app.receive(snapshot);
        }
        app.open(page);
        ui::Feedback feedback;
        InputFrame neutral;
        neutral.connected = true;
        for (int i = 0; i < 60; i++)
            app.update(neutral, 1.f / 60, 1000000 + i * 16667, feedback);
        gfx::DrawList list;
        ui::Canvas canvas{list, fonts, 0, 1};
        app.draw(canvas);
        renderer.begin();
        renderer.draw(list);
        renderer.present(target.framebuffer(), 1920, 1080);
        glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer());
        std::vector<unsigned char> pixels(1920 * 1080 * 4);
        glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        stbi_flip_vertically_on_write(1);
        if (!stbi_write_png((out + name + ".png").c_str(), 1920, 1080, 4, pixels.data(), 1920 * 4))
            return 5;
    }
    target.destroy();
    renderer.release();
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglTerminate(display);
    return 0;
}
