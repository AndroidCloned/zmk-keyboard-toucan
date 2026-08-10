#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/lib/west-build.sh"

REPO="${REPO:-/workdir}"
BASE_DIR="${BASE_DIR:-/tmp/zmk-workspace}"

toucan_west_prepare
# Extra conf fragment (shield toucan_right.conf still loads from the module).
cp "${REPO}/boards/shields/toucan/toucan_right_rtt.conf" \
  "${BASE_DIR}/config/toucan_right.conf"

toucan_west_build "xiao_ble//zmk" "toucan_right rgbled_adapter" \
  "nrf52840-nosd" \
  "-DCONFIG_USE_SEGGER_RTT=y -DCONFIG_LOG_BACKEND_RTT=y -DCONFIG_LOG=y" \
  "toucan_right_rtt.uf2" "toucan_right_rtt.elf"
ls -la "${OUT:-/out}"/toucan_right_rtt*
echo "==== Done ===="
