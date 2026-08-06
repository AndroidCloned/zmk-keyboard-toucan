#!/usr/bin/env bash
# Inner build script — invoked by scripts/build-nightly.ps1 inside zmk-build-arm.
# Mirrors zmkfirmware/zmk build-user-config.yml: when zephyr/module.yml exists,
# west runs in an isolated temp workspace so it does not overwrite the shield module.
set -euo pipefail

REPO="${REPO:-/workdir}"
OUT="${OUT:-/out}"
SKIP_UPDATE="${SKIP_UPDATE:-0}"
BASE_DIR="${BASE_DIR:-/tmp/zmk-workspace}"

mkdir -p "$OUT"

build_one() {
  local board="$1"
  local shield="$2"
  local snippet="$3"
  local cmake_extra="$4"
  local artifact="$5"

  local build_dir
  build_dir="$(mktemp -d)"
  local west_snippet=()
  if [ -n "$snippet" ]; then
    # west accepts one -S per snippet; matrix values may list several space-separated.
    local s
    for s in $snippet; do
      west_snippet+=(-S "$s")
    done
  fi

  echo "==== Building: ${artifact:-$shield on $board} ===="
  # shellcheck disable=SC2086
  west build -s zmk/app -d "$build_dir" -b "$board" "${west_snippet[@]}" -- \
    -DZMK_CONFIG="${BASE_DIR}/config" \
    -DSHIELD="$shield" \
    -DZMK_EXTRA_MODULES="${REPO}" \
    $cmake_extra

  local uf2_src="$build_dir/zephyr/zmk.uf2"
  if [ ! -f "$uf2_src" ]; then
    echo "ERROR: missing $uf2_src"
    exit 1
  fi

  local out_name
  if [ -n "$artifact" ]; then
    out_name="${artifact}.uf2"
  else
    local safe_board
    safe_board="$(echo "$board" | tr '/' '_')"
    out_name="${shield}-${safe_board}-zmk.uf2"
  fi

  cp "$uf2_src" "$OUT/$out_name"
  echo "Wrote $OUT/$out_name ($(stat -c%s "$uf2_src") bytes)"
  rm -rf "$build_dir"
}

# Isolated west workspace (do not pollute REPO/zephyr module marker)
mkdir -p "${BASE_DIR}/config"
cp -a "${REPO}/config/." "${BASE_DIR}/config/"

cd "$BASE_DIR"
if [ ! -f .west/config ]; then
  west init -l config
fi
if [ "$SKIP_UPDATE" != "1" ]; then
  west update --fetch-opt=--filter=tree:0
fi
west zephyr-export

build_one "xiao_ble//zmk" "toucan_left rgbled_adapter" "nrf52840-nosd studio-rpc-usb-uart" "-DCONFIG_ZMK_STUDIO=y" "toucan_left_bare_studio"
build_one "xiao_ble//zmk" "toucan_left rgbled_adapter nice_view_gem" "nrf52840-nosd" "" "toucan_left_no_studio"
build_one "xiao_ble//zmk" "toucan_left rgbled_adapter nice_view_gem" "nrf52840-nosd zmk-usb-logging" "-DCONFIG_LOG_PROCESS_THREAD_STARTUP_DELAY_MS=8000 -DCONFIG_ZMK_LOG_LEVEL_DBG=y -DCONFIG_DISPLAY_LOG_LEVEL_DBG=y" "toucan_left_usb_logging"
build_one "xiao_ble//zmk" "toucan_left rgbled_adapter nice_view_gem" "nrf52840-nosd studio-rpc-usb-uart" "-DCONFIG_ZMK_STUDIO=y" "toucan_left_with_studio"
build_one "xiao_ble//zmk" "toucan_right rgbled_adapter" "nrf52840-nosd" "" ""
build_one "xiao_ble//zmk" "toucan_right rgbled_adapter" "nrf52840-nosd zmk-usb-logging" "-DCONFIG_INPUT_EVENT_DUMP=y -DCONFIG_ZMK_LOG_LEVEL_DBG=y -DCONFIG_INPUT_LOG_LEVEL_DBG=y" "toucan_right_debug"
build_one "xiao_ble//zmk" "settings_reset" "nrf52840-nosd" "" "settings_reset-xiao_ble__zmk-zmk"

echo "==== Done. Artifacts in $OUT ===="
ls -la "$OUT"/*.uf2
