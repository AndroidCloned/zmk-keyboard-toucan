#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/lib/west-build.sh"

toucan_west_prepare
toucan_west_build "xiao_ble//zmk" "toucan_right rgbled_adapter" \
  "nrf52840-nosd" "" "toucan_right rgbled_adapter-xiao_ble__zmk-zmk.uf2" \
  "toucan_right-prod.elf"
ls -la "${OUT:-/out}"/toucan_right*
echo "==== Done ===="
