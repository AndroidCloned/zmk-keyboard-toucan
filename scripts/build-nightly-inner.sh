#!/usr/bin/env bash
set -euo pipefail
# shellcheck source=lib/west-build.sh
source "$(dirname "$0")/lib/west-build.sh"

toucan_west_prepare

toucan_west_build "xiao_ble//zmk" "toucan_left rgbled_adapter" \
  "nrf52840-nosd studio-rpc-usb-uart" "-DCONFIG_ZMK_STUDIO=y" "toucan_left_bare_studio.uf2"
toucan_west_build "xiao_ble//zmk" "toucan_left rgbled_adapter nice_view_gem" \
  "nrf52840-nosd" "" "toucan_left_no_studio.uf2"
toucan_west_build "xiao_ble//zmk" "toucan_left rgbled_adapter nice_view_gem" \
  "nrf52840-nosd zmk-usb-logging" \
  "-DCONFIG_LOG_PROCESS_THREAD_STARTUP_DELAY_MS=8000 -DCONFIG_ZMK_LOG_LEVEL_DBG=y -DCONFIG_DISPLAY_LOG_LEVEL_DBG=y" \
  "toucan_left_usb_logging.uf2"
toucan_west_build "xiao_ble//zmk" "toucan_left rgbled_adapter nice_view_gem" \
  "nrf52840-nosd studio-rpc-usb-uart" "-DCONFIG_ZMK_STUDIO=y" "toucan_left_with_studio.uf2"
toucan_west_build "xiao_ble//zmk" "toucan_right rgbled_adapter" \
  "nrf52840-nosd" "" "toucan_right rgbled_adapter-xiao_ble__zmk-zmk.uf2"
toucan_west_build "xiao_ble//zmk" "toucan_right rgbled_adapter" \
  "nrf52840-nosd zmk-usb-logging" \
  "-DCONFIG_INPUT_EVENT_DUMP=y -DCONFIG_ZMK_LOG_LEVEL_DBG=y -DCONFIG_INPUT_LOG_LEVEL_DBG=y" \
  "toucan_right_debug.uf2"
toucan_west_build "xiao_ble//zmk" "settings_reset" \
  "nrf52840-nosd" "" "settings_reset-xiao_ble__zmk-zmk.uf2"

echo "==== Done ===="
ls -la "${OUT:-/out}"/*.uf2
