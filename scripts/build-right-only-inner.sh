#!/usr/bin/env bash
# Build only the right-half UF2 (Cirque tracking / power helper iterates).
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
echo "==== Building right half (tracking fix) ===="
west build -s zmk/app -d "$build_dir" -b "xiao_ble//zmk" -S nrf52840-nosd -- \
  -DZMK_CONFIG="${BASE_DIR}/config" \
  -DSHIELD="toucan_right rgbled_adapter" \
  -DZMK_EXTRA_MODULES="${REPO}"

cp "$build_dir/zephyr/zmk.uf2" "$OUT/toucan_right rgbled_adapter-xiao_ble__zmk-zmk.uf2"
cp "$build_dir/zephyr/zmk.elf" "$OUT/toucan_right-prod.elf"
ls -la "$OUT"/toucan_right*
rm -rf "$build_dir"
echo "==== Done ===="
