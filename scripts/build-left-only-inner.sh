#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/lib/west-build.sh"

toucan_west_prepare
toucan_west_build "xiao_ble//zmk" "toucan_left rgbled_adapter nice_view_gem" \
  "nrf52840-nosd" "" "toucan_left_no_studio.uf2"
ls -la "${OUT:-/out}"/toucan_left_no_studio.uf2
echo "==== Done ===="
