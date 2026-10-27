#!/usr/bin/env python3
# Prism for PS5 - README hero and screenshots from TV captures and the native renders.
# Copyright (C) 2026 Hayden Griffin
# SPDX-License-Identifier: GPL-3.0-or-later
"""Draw docs/images/hero.png and docs/images/screenshots/.

    docs/images/hero.png         3200 × 1640: LG Settings over Prism on a TV, with Prism's
                                 launcher and Quick TV settings beside it, the lockup and tagline
    docs/images/screenshots/     all LG apps, the remote screen and Quick TV settings

docs/images/tv/ holds screenshots the TV took of itself (webOS's capture service, DISPLAY
method) while the PS5 showed Prism: the launcher, and LG Settings opened from Prism over its
remote screen. The other pictures are 1920 × 1080 renders from Prism's own UI code and
shaders (build/promo/), drawn from a snapshot of a real Home Assistant
(tools/capture-promo.py). The layout follows the Android TV app's hero (prism/tools/hero.py).

    pip install pillow
    python3 tools/hero.py [build/promo]
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "images"
FONTS = ROOT / "third_party" / "fonts"
LOCKUP = ROOT / "docs" / "brand" / "prism-lockup.png"


def hex_rgb(value):
    return tuple(int(value[i:i + 2], 16) for i in (1, 3, 5))


NIGHT = hex_rgb("#0e0f12")
NIGHT_RAISED = hex_rgb("#17191e")
WARM_WHITE = hex_rgb("#f4f2ee")
MUTED = hex_rgb("#b9b4ac")
SKY = hex_rgb("#80d8ff")
AMBER = hex_rgb("#ff9d5c")

WIDTH, HEIGHT = 3200, 1640
MARGIN = 160
TAGLINE = [  # (text, font, colour, line height)
    ("One remote for your LG TV, on your PS5.", ("semibold", 54), WARM_WHITE, 70),
    ("Your DualSense drives LG apps, LG Home and quick TV settings, through Home Assistant.",
     ("regular", 42), MUTED, 58),
]

# Regions of the 1920 × 1080 renders (left, top, right, bottom).
QUICK_CROP = (64, 232, 1208, 424)     # the first two settings
LAUNCHER_CROP = (64, 250, 1856, 484)  # the first row: Continue on the LG and recent apps
TV = ROOT / "docs" / "images" / "tv"
SHOTS = ("apps", "remote", "quick-settings")


def font(weight, size):
    return ImageFont.truetype(str(FONTS / f"atkinson_{weight}.ttf"), size)


def rounded_mask(size, radius):
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius, fill=255)
    return mask


def glow(canvas, centre, radius, colour, strength):
    """A soft coloured light on the background, centred on a point."""
    layer = Image.new("L", canvas.size, 0)
    x, y = centre
    ImageDraw.Draw(layer).ellipse((x - radius, y - radius, x + radius, y + radius), fill=int(255 * strength))
    layer = layer.filter(ImageFilter.GaussianBlur(radius * 0.6))
    canvas.paste(Image.new("RGB", canvas.size, colour), (0, 0), layer)


def shadow(canvas, box, radius, opacity=0.55, blur=60, offset=30):
    left, top, right, bottom = box
    layer = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(layer).rounded_rectangle((left, top + offset, right, bottom + offset), radius,
                                            fill=int(255 * opacity))
    canvas.paste(Image.new("RGB", canvas.size, (0, 0, 0)), (0, 0), layer.filter(ImageFilter.GaussianBlur(blur)))


def edge(canvas, box, radius, alpha=26):
    """A hairline on a panel's edge so it lifts off the night ground."""
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(layer).rounded_rectangle(box, radius, outline=WARM_WHITE + (alpha,), width=2)
    canvas.paste(layer, (0, 0), layer)


def place(canvas, image, box, radius):
    left, top, right, bottom = box
    size = (right - left, bottom - top)
    canvas.paste(image.resize(size, Image.LANCZOS), (left, top), rounded_mask(size, radius))


def tv(canvas, screen, left, top, width):
    """The render on a thin-bezelled TV with a low stand."""
    bezel, radius = 18, 22
    height = round(width * 9 / 16)
    frame = (left - bezel, top - bezel, left + width + bezel, top + height + bezel)
    stand_w, stand_h = width // 4, 22
    stand_x = left + (width - stand_w) // 2
    stand = (stand_x, frame[3], stand_x + stand_w, frame[3] + stand_h)
    shadow(canvas, frame, radius + bezel)
    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle(stand, 8, fill=NIGHT_RAISED)
    draw.rounded_rectangle(frame, radius + bezel // 2, fill=(26, 28, 33))
    edge(canvas, frame, radius + bezel // 2, alpha=30)
    place(canvas, screen, (left, top, left + width, top + height), 6)


def panel(canvas, image, left, top, width, radius=28):
    height = round(width * image.height / image.width)
    box = (left, top, left + width, top + height)
    shadow(canvas, box, radius, opacity=0.5, blur=44, offset=22)
    place(canvas, image, box, radius)
    edge(canvas, box, radius)


def wrap(draw, text, face, width):
    lines, line = [], ""
    for word in text.split():
        trial = f"{line} {word}".strip()
        if line and draw.textlength(trial, font=face) > width:
            lines.append(line)
            line = word
        else:
            line = trial
    return lines + [line]


def text_column(canvas, left, top, width):
    """The lockup and the tagline; returns the y below them."""
    lockup_h = 120
    lockup = Image.open(LOCKUP).convert("RGBA")
    lockup = lockup.resize((round(lockup.width * lockup_h / lockup.height), lockup_h), Image.LANCZOS)
    canvas.paste(lockup, (left, top), lockup)
    draw = ImageDraw.Draw(canvas)
    y = top + lockup_h + 64
    for text, (weight, size), colour, leading in TAGLINE:
        face = font(weight, size)
        for line in wrap(draw, text, face, width):
            draw.text((left, y), line, font=face, fill=colour)
            y += leading
        y += 18
    return y


def caption(canvas, text, left, bottom):
    """A small spaced-out label that sits just above a panel."""
    face = font("semibold", 30)
    draw = ImageDraw.Draw(canvas)
    x = left
    for char in text.upper():
        draw.text((x, bottom), char, font=face, fill=MUTED, anchor="ls")
        x += draw.textlength(char, font=face) + 5


def captioned_panel(canvas, image, label, left, bottom, width):
    """A panel whose bottom edge is at `bottom`, with its caption above it; returns the caption's top."""
    height = round(width * image.height / image.width)
    top = bottom - height
    panel(canvas, image, left, top, width)
    caption(canvas, label, left + 4, top - 26)
    return top - 26 - 30


def hero(settings, launcher, quick):
    canvas = Image.new("RGB", (WIDTH, HEIGHT), NIGHT)
    glow(canvas, (2100, 820), 1100, SKY, 0.07)
    glow(canvas, (420, 1400), 700, AMBER, 0.045)

    column_w = 820
    gap = 120
    screen_w = WIDTH - 2 * MARGIN - column_w - gap - 36
    screen_h = round(screen_w * 9 / 16)
    top = (HEIGHT - screen_h) // 2 - 10
    screen_left = MARGIN + column_w + gap + 18
    tv(canvas, settings, screen_left, top, screen_w)
    caption(canvas, "LG Settings over Prism, driven by the DualSense", screen_left - 14, top - 50)

    below_text = text_column(canvas, MARGIN, top - 18, column_w)
    bottom = top + screen_h + 18
    quick_top = captioned_panel(canvas, quick, "Quick TV settings", MARGIN, bottom, column_w)
    launcher_top = captioned_panel(canvas, launcher, "Prism on the PS5", MARGIN, quick_top - 64, column_w)
    assert launcher_top > below_text + 40, f"the panels run into the tagline ({launcher_top} <= {below_text + 40})"
    return canvas


def save_png(image, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, optimize=True)
    print(f"{path.relative_to(ROOT)}  {image.width}×{image.height}")


def main():
    renders = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build" / "promo"
    shots = {name: Image.open(renders / f"{name}.png").convert("RGB") for name in SHOTS}
    settings = Image.open(TV / "lg-settings.jpg").convert("RGB")
    launcher = Image.open(TV / "launcher.jpg").convert("RGB")
    save_png(hero(settings, launcher.crop(LAUNCHER_CROP), shots["quick-settings"].crop(QUICK_CROP)), OUT / "hero.png")
    for name, image in shots.items():
        save_png(image, OUT / "screenshots" / f"{name}.png")


if __name__ == "__main__":
    main()
