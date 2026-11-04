# Changelog

## Unreleased

- The launcher fits one screen: a big Continue tile, a grid of favourites, recent apps and then the TV's others ending
  in All apps, and a dock with LG Home, LG Settings, Quick TV settings, the remote and Setup. Favourites past the grid's
  seven places stay in All apps.
- Title and hints shrink to one compact line each, so pages have about a third more room. All apps shows four whole
  rows of six, and never cuts a row. Setup lists its rows in two columns, so they all show at once.
- A beam at the foot of the screen shows where the controller goes, the PS5 or the LG, beside the same thing in words.
- Themes in Setup: Refract (new default; a soft light follows the focused tile in that app's colour), OLED Night, Dusk,
  Daylight, Aurora and Phosphor. Setup → Dusk after sunset switches to Dusk when Home Assistant's sun has set.
- Quick TV settings is a sheet over the launcher, with all seven settings on screen.
- Digits are drawn in Lexend, as on Android TV: Atkinson's slashed zero read as "Ø" from the sofa.
- LG Settings, Quick TV settings and Setup each have their own icon.
- Hints only offer what the buttons really do on the focused item. Options is labelled for where it goes.
- Failures read as failures, in the error colour. Square only acts on All apps, where the star shows that it worked.
- Disconnect and forget token takes two presses.
- The digits lost the seams that showed inside some of them, and Favourites use a star in place of On and Off.

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
