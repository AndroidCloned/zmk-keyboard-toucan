# ZMK config for beekeeb Toucan Keyboard

[The beekeeb Toucan Keyboard](https://beekeeb.com/toucan-keyboard/) is a wireless split 42-key column-stagger keyboard with a display and a trackpad, with an aggressive stagger on the pinky columns.

## Branches

| Branch | ZMK version | Status |
|--------|-------------|--------|
| `main` | v0.3 (pinned) | Stable builds. Use this for everyday firmware. |
| `nightly` | `main` (Zephyr 4.1) | Experimental. See [Dragons ahead](#dragons-ahead-nightly) below. |

Firmware builds automatically on push via GitHub Actions. Download the `firmware` artifact from the [Actions tab](https://github.com/AndroidCloned/zmk-keyboard-toucan/actions) on the branch you want.

## Flashing

1. Double-tap the reset button on the Seeed XIAO nRF52840 — it appears as a USB drive (e.g. `E:`).
2. Copy the matching `.uf2` file to that drive. The board reboots when the copy finishes.
3. Repeat for the other half.

| Half | UF2 file |
|------|----------|
| Left (display, no Studio — try first) | `toucan_left_no_studio.uf2` (or similar name from the artifact) |
| Left (display + Studio) | `toucan_left_with_studio.uf2` |
| Right (trackpad, peripheral) | `toucan_right rgbled_adapter-xiao_ble__zmk-zmk.uf2` |
| Settings reset (optional) | `settings_reset-xiao_ble__zmk-zmk.uf2` |

Flash **settings reset** first if you need to wipe stored Bluetooth pairings and ZMK settings before installing new firmware.

## Dragons ahead (`nightly`)

The `nightly` branch tracks upstream ZMK `main`. It builds and produces UF2s, but treat it as **beta firmware** until you have verified it on your own keyboard.

### What changed from `main`

- Board target migrated to `xiao_ble//zmk` (required for Zephyr 4.1).
- Removed the external `cirque-input-module`; the trackpad now uses Zephyr's built-in `cirque,pinnacle` driver with updated devicetree properties.
- `nice_view_gem` updated for LVGL 9 (display drawing APIs).
- `zmk-rgbled-widget` bumped to its `main` branch.

### Known build warnings (non-fatal)

- `Deprecated symbol KSCAN is enabled` — expected during the ZMK/Zephyr migration.
- `ZMK_USB` assigned but disabled on the **right** half — expected; USB is central-only on split builds.
- Left-half RAM usage is high with Studio enabled. Prefer the `toucan_left_no_studio` artifact first when validating display/boot.

### Display / LVGL notes (`nightly`)

The nice!view is a 1-bit Sharp LS0XX panel. On ZMK main (LVGL 9) the status canvas must use `LV_COLOR_FORMAT_L8` with a properly sized `LV_CANVAS_BUF_SIZE` buffer — matching upstream `nice_view`. Using `LV_COLOR_FORMAT_NATIVE` with an `lv_color_t[]` buffer can compile but fail to render (blank display / unstable boot).

Artifacts:

- `toucan_left_no_studio` — left half without ZMK Studio (recommended first flash for display bring-up)
- `toucan_left_with_studio` — left half with Studio RPC over USB

### What CI does **not** test

A green GitHub Actions build only proves compile/link success. It does **not** verify:

- Trackpad movement, scrolling, or sleep/wake behavior with the new upstream Cirque driver
- Display rendering (battery icons, layer name, sleep screen) after the LVGL 9 migration
- BLE split pairing between left and right
- ZMK Studio over USB on the left half

### Hardware checklist after flashing `nightly`

- [ ] Both halves pair over BLE and register as one keyboard
- [ ] All keys on left and right register correctly
- [ ] Trackpad pointer and scroll layers work on the right half
- [ ] Display shows battery, layer, profile, and connection status on the left half
- [ ] Sleep screen appears on the display when the keyboard idles
- [ ] ZMK Studio connects (left half, USB)

If something fails, flash firmware from the `main` branch to get back to the known-good v0.3 toolchain.

## License

The code in this repo is available under the MIT license.

The included shield nice_view_gem is modified from https://github.com/M165437/nice-view-gem licensed under the MIT License.

ZMK code snippets are taken from the ZMK documentation under the MIT license.

The embedded font QuinqueFive is designed by GGBotNet, licensed under the SIL Open Font License, Version 1.1.
