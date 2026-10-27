# Changelog

## 1.0.1 (27 October 2026)

- The remote screen names Game Optimizer, and any other LG system screen without a name of its own is "LG screen",
  instead of showing its app ID.
- The README and its pictures show Prism on the TV: screenshots the TV took of LG Settings and Game Optimizer
  open over Prism.

## 1.0.0 (27 October 2026)

The first release: Prism's LG remote and launcher as a native PS5 title, run on one console (system software 7.20)
with a 2021 LG C1. See [console validation](docs/console-validation.md) for what was checked there.

- The DualSense drives the LG while an LG app, LG menu or the LG keyboard is on screen. Hold Circle to switch the TV
  back to the PS5.
- A launcher for the LG: favourite apps with their own icons, recently used apps, Continue on the LG, all apps, LG
  Home and LG Settings.
- Quick TV settings: input, picture mode, OLED brightness, energy saving, sound mode, sleep timer and logo dimming.
- When LG Settings opens another LG screen by way of the PS5's input (Game Optimizer, the User Guide), the
  controller follows it.
- Controller vibration is a setting, off by default.
- Works with large Home Assistant installs: registry messages up to 16 MB, and registry changes that don't touch
  the TV no longer interrupt the remote.
- Decimal numbers from Home Assistant are parsed by Prism itself; the console's C library ended the app on them.
