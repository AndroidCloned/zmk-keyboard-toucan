#!/usr/bin/env bash
# Build right half with SEGGER RTT logging for probe-rs attach.
set -euo pipefail

REPO="${REPO:-/workdir}"
OUT="${OUT:-/out}"
BASE_DIR="${BASE_DIR:-/tmp/zmk-workspace}"

mkdir -p "$OUT" "${BASE_DIR}/config"
cp -a "${REPO}/config/." "${BASE_DIR}/config/"
# Ephemeral merge for this build only (west volume config/ is shared)
cat "${REPO}/boards/shields/toucan/toucan_right_rtt.conf" \
  > "${BASE_DIR}/config/toucan_right.conf"

cd "$BASE_DIR"
if [ ! -f .west/config ]; then
  west init -l config
fi
if [ "${SKIP_UPDATE:-1}" != "1" ]; then
  west update --fetch-opt=--filter=tree:0
fi
west zephyr-export

build_dir="$(mktemp -d)"
echo "==== Building right half (RTT debug) ===="
west build -s zmk/app -d "$build_dir" -b "xiao_ble//zmk" -S nrf52840-nosd -- \
  -DZMK_CONFIG="${BASE_DIR}/config" \
  -DSHIELD="toucan_right rgbled_adapter" \
  -DZMK_EXTRA_MODULES="${REPO}" \
  -DCONFIG_LOG=y \
  -DCONFIG_ZMK_LOG_LEVEL_INF=y \
  -DCONFIG_LOG_DEFAULT_LEVEL=2 \
  -DCONFIG_USE_SEGGER_RTT=y \
  -DCONFIG_LOG_BACKEND_RTT=y \
  -DCONFIG_SEGGER_RTT_BUFFER_SIZE_UP=8192 \
  -DCONFIG_BT_LOG_LEVEL_WRN=y

cp "$build_dir/zephyr/zmk.uf2" "$OUT/toucan_right_rtt.uf2"
cp "$build_dir/zephyr/zmk.elf" "$OUT/toucan_right_rtt.elf"
ls -la "$OUT"/toucan_right_rtt*
# Confirm RTT actually enabled
grep -E 'CONFIG_LOG=|CONFIG_LOG_BACKEND_RTT=|CONFIG_USE_SEGGER_RTT=' "$build_dir/zephyr/.config" || true
rm -rf "$build_dir"
echo "==== Done ===="
