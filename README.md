# ZMK config for beekeeb Toucan

Wireless split 42-key with nice!view + Cirque trackpad:
https://beekeeb.com/toucan-keyboard/

Fork of [kalbasit/zmk-keyboard-toucan](https://github.com/kalbasit/zmk-keyboard-toucan)
(upstream [beekeeb](https://github.com/beekeeb/zmk-keyboard-toucan)).

| Branch | ZMK | Notes |
|--------|-----|--------|
| `main` | v0.3 | Stable |
| `nightly` | ZMK `main` / Zephyr 4.1 | Active development |

CI builds UF2s on push — grab the `firmware` artifact from
[Actions](https://github.com/AndroidCloned/zmk-keyboard-toucan/actions).

## Local build

```powershell
.\scripts\build-nightly.ps1
```

Needs Docker. Output: `../toucan-nightly-firmware/firmware/`.

## Flash

Double-tap reset on the XIAO → copy the matching UF2:

| Half | Artifact |
|------|----------|
| Left (daily) | `toucan_left_no_studio.uf2` |
| Left + Studio | `toucan_left_with_studio.uf2` |
| Right | `toucan_right rgbled_adapter-xiao_ble__zmk-zmk.uf2` |
| Settings wipe | `settings_reset-xiao_ble__zmk-zmk.uf2` |

Flash `settings_reset` when switching to/from `nrf52840-nosd`, or to clear BT pairings.

## `nightly` deltas vs stock

- Board: `xiao_ble//zmk`, all targets use `nrf52840-nosd`
- Cirque via Zephyr `cirque,pinnacle` (no out-of-tree module)
- Right: Sleep Enable on ZMK idle, SHUTDOWN before System OFF, boot
  `FORCE_WAKEUP` so the pad comes back after deep sleep
  (`boards/shields/toucan/cirque_deep_sleep.c`)
- Left daily driver: `toucan_left_no_studio` (Studio + sleep ≈ zmk#3195)
- nice_view_gem on LVGL 9 (`L8` canvas, serial VCOM)

## License

MIT. `nice_view_gem` from https://github.com/M165437/nice-view-gem (MIT).
QuinqueFive font by GGBotNet (SIL OFL 1.1).
