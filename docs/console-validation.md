# Console validation

Host tests cannot establish controller focus, firmware-specific system keyboard behaviour, presentation performance or suspension policy, so these checks are done on a console. The results so far are at the end.

Do not deploy or launch a console title without the owner's permission. Keep the LG remote available during the first run. Use the normal homebrew loader; do not kill a rendering title or change console system settings as part of this test.

## First run

1. Install `dist/PPSA99483.zip` under its own title ID. Open Prism and confirm the launcher renders and the controller works.
2. In Setup, enter credentials, connect, choose the TV and capture the PS5 input. Wait for **Saved on this PS5**. Restart Prism normally and confirm it restores the connection and input.
3. Enable **Input diagnostics**. This writes `/download0/prism/status.json` once per second from the storage worker. It contains a monotonic timestamp, screen number, controller-focus flag, count of frames containing button presses, forwarded command count and connection status. It contains no token, address or typed text.
4. Read it over FTP while Prism is running:

   ```sh
   python3 tools/probe.py --host <console-address> --seconds 60
   ```

   The default FTP port is `2121`; override with `--port`. If your FTP server requires credentials, set `PS5_FTP_USER` and `PS5_FTP_PASSWORD` in your local environment. The tool only reads `/mnt/sandbox/PPSA99483_000/download0/prism/status.json`; it does not upload, launch or stop anything.

## Foreground remote checks

- Open an LG app through Prism. With that app visible on the TV, press several D-pad directions and Cross. Check that the LG responds and the diagnostic press/forward counts advance. This is the critical input-retention test.
- Tap Circle: exactly one Back. Hold Circle for at least 0.8 seconds: return to the PS5 without a short Back first. Check the same return while LG Home, LG Settings and an LG keyboard overlay the PS5's HDMI input.
- Open the system UI with PS. Confirm Prism's diagnostic focus flag stays false and forwarding stops for the entire overlay, including while the stick is held. Close the overlay and release all controls before pressing again. Prism must not replay a press.
- Disconnect and reconnect the controller. Confirm the same release-before-resume rule.
- Briefly disconnect Home Assistant or stop the test server. Press buttons while disconnected. Reconnect it: no old presses should arrive at the TV. Check that a newly pressed button works.
- Launch an app whose service fails: Prism reports failure and does not pretend the LG switched.
- Change quick settings, then verify the physical TV values and Home Assistant state. Check the brightness endpoints. Screen-off and sound-output must not appear.
- Open an LG text field, use Touchpad for text entry, and verify cancellation sends nothing. This requires Glasshouse's keyboard entities and `keyboard_visible` attribute.
- Leave an LG app visible for 20–30 minutes with Prism still foreground on the PS5. Record whether diagnostic timestamps keep advancing and whether the first new input still works. Repeat after whatever sleep/suspend conditions you normally use, without changing system settings for the test.
- Return to the PS5 and close Prism through Setup → Close Prism. Check that it exits normally and saved favourites survive the next launch.

Record firmware, homebrew loader/version, TV model, Glasshouse version, test duration, package SHA-256, and observed results. Console validation is complete only when these observations exist; a successful compile or preview is not hardware evidence.

## Results

### 27 October 2026, 1.0.0

PS5 on system software 7.20 with ShadowMount Plus 1.7-beta5fix1 and ftpsrv 0.21.1; a 2021 LG C1 (OLED55C14LB) with Glasshouse; Home Assistant with about 3,100 entities. The PS5 is on HDMI 1. Setup came from a bootstrap `prism.conf`.

Seen working, from the screen and from Home Assistant's history of the TV's active app and button presses:

- Prism starts, connects and stays connected (one websocket for over 25 minutes, no reconnects).
- The launcher, all LG apps and the app icons.
- Opening LG apps (Afterglow, House, 2048), LG Home and LG Settings from Prism.
- D-pad, Cross and Back reach the LG within a second of each press.
- LG Settings → Game Optimizer: the TV shows the PS5's input for about 3.7 s in between, and the controller follows Game Optimizer afterwards.

Found on the console and fixed before release:

- Home Assistant's entity registry arrives as one 2.8 MB message; the websocket limit was 2 MB, so Prism reconnected forever. The limit is now 16 MB, and socket failures are logged.
- The console's C library left a JSON decimal (a registry timestamp) unparsed and the JSON parser's assertion ended the app two seconds after starting. Decimals are now parsed by `src/prism/decimal.hpp`.
- Screens LG Settings opens by way of the PS5's input (Game Optimizer, the User Guide) returned Prism to its launcher, so the controller stopped driving the LG. Prism now follows an LG screen that appears while its launcher is up.
- `status.json` was written with mode `0600`, which this FTP server could not read; it is now `0644`.

Not yet checked: holding Circle to return and the dismissal of LG menus on the way, touchpad text entry, the system menu and controller reconnection rules, a dropped Home Assistant connection, quick settings against the TV, long idle sessions and rest mode.

