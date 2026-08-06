#!/usr/bin/env bash
# Build only the daily left UF2 (pointer scaler / display / central iterates).
set -euo pipefail

REPO="${REPO:-/workdir}"
OUT="${OUT:-/out}"
BASE_DIR="${BASE_DIR:-/tmp/zmk-workspace}"

mkdir -p "$OUT" "${BASE_DIR}/config"
cp -a "${REPO}/config/." "${BASE_DIR}/config/"

cd "$BASE_DIR"
if [ ! -f .west/config ]; then
  west init -l config
fi
if [ "${SKIP_UPDATE:-1}" != "1" ]; then
  west update --fetch-opt=--filter=tree:0
fi
west zephyr-export

build_dir="$(mktemp -d)"
echo "==== Building left half (no_studio) ===="
west build -s zmk/app -d "$build_dir" -b "xiao_ble//zmk" -S nrf52840-nosd -- \
  -DZMK_CONFIG="${BASE_DIR}/config" \
  -DSHIELD="toucan_left rgbled_adapter nice_view_gem" \
  -DZMK_EXTRA_MODULES="${REPO}"

cp "$build_dir/zephyr/zmk.uf2" "$OUT/toucan_left_no_studio.uf2"
ls -la "$OUT"/toucan_left_no_studio.uf2
rm -rf "$build_dir"
echo "==== Done ===="
