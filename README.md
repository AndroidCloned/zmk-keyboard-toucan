# ZMK config for beekeeb Toucan

Wireless split 42-key with nice!view + Cirque trackpad:
https://beekeeb.com/toucan-keyboard/

Fork of [kalbasit/zmk-keyboard-toucan](https://github.com/kalbasit/zmk-keyboard-toucan)
(upstream [beekeeb](https://github.com/beekeeb/zmk-keyboard-toucan)).

| Branch | ZMK | Notes |
|--------|-----|--------|
| `main` | v0.3 | Stable |
| `nightly` | **Pinned** ZMK `f2fa390` / Zephyr 4.1 | PROD candidate (see below) |

CI builds UF2s on push — grab the `firmware` artifact from
[Actions](https://github.com/AndroidCloned/zmk-keyboard-toucan/actions).

## PROD daily pair

Supported daily flash (both halves, `nrf52840-nosd`):

| Half | Artifact |
|------|----------|
| Left | `toucan_left_no_studio.uf2` |
| Right | `toucan_right rgbled_adapter-xiao_ble__zmk-zmk.uf2` |
| Settings wipe | `settings_reset-xiao_ble__zmk-zmk.uf2` |

**Not PROD:** `with_studio` / `bare_studio` (zmk#3195), RTT, USB logging.

`config/west.yml` pins ZMK + rgbled-widget SHAs — do not float `main` for release builds.

## Local build

```powershell
.\scripts\build-nightly.ps1
# or half-only:
# docker … bash scripts/build-left-only-inner.sh
# docker … bash scripts/build-right-only-inner.sh
```

Needs Docker. Output: `../toucan-nightly-firmware/firmware/`.

## Flash

Double-tap reset on the XIAO → copy the matching UF2 from the table above.

Flash `settings_reset` when switching to/from `nrf52840-nosd`, or to clear BT pairings.
Power-cycle after flash if the half looks frozen.

## `nightly` layout

| Side | Policy |
|------|--------|
| Left | 15 min deep sleep, PM runtime, display blank-on-idle + VCOM pause, Cirque listener 3× + NAV/SYM scroll, BLE Realtek quirks, battery FETCHING (PROXY off) |
| Right | 5 min deep sleep, Cirque sensing while awake / SHUTDOWN before System OFF, boot FORCE_WAKEUP (+ follow-up feed), pointing TX gated until split up, VBUS charge beacon (`boards/shields/toucan/cirque/`) |

## License

MIT. `nice_view_gem` from https://github.com/M165437/nice-view-gem (MIT).
QuinqueFive font by GGBotNet (SIL OFL 1.1).
