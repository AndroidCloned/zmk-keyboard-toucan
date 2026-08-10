#!/usr/bin/env bash
# Shared west workspace helpers for Toucan Docker builds.
# shellcheck shell=bash

toucan_west_prepare() {
  local repo="${REPO:-/workdir}"
  local base="${BASE_DIR:-/tmp/zmk-workspace}"

  mkdir -p "${OUT:-/out}" "${base}/config"
  rm -rf "${base}/config"
  mkdir -p "${base}/config"
  cp -a "${repo}/config/." "${base}/config/"

  cd "$base"
  if [ ! -f .west/config ]; then
    west init -l config
  fi
  if [ "${SKIP_UPDATE:-1}" != "1" ]; then
    west update --fetch-opt=--filter=tree:0
  fi
  west zephyr-export
}

# Args: board shield snippets cmake_extra artifact_uf2 [extra_copy_elf_path]
toucan_west_build() {
  local board="$1"
  local shield="$2"
  local snippet="$3"
  local cmake_extra="${4:-}"
  local artifact="$5"
  local elf_out="${6:-}"
  local base="${BASE_DIR:-/tmp/zmk-workspace}"
  local out="${OUT:-/out}"
  local build_dir
  local west_snippet=()
  local s

  build_dir="$(mktemp -d)"
  if [ -n "$snippet" ]; then
    for s in $snippet; do
      west_snippet+=(-S "$s")
    done
  fi

  echo "==== Building: ${artifact} ===="
  # shellcheck disable=SC2086
  west build -s zmk/app -d "$build_dir" -b "$board" "${west_snippet[@]}" -- \
    -DZMK_CONFIG="${base}/config" \
    -DSHIELD="$shield" \
    -DZMK_EXTRA_MODULES="${REPO:-/workdir}" \
    $cmake_extra

  cp "$build_dir/zephyr/zmk.uf2" "$out/$artifact"
  echo "Wrote $out/$artifact"
  if [ -n "$elf_out" ]; then
    cp "$build_dir/zephyr/zmk.elf" "$out/$elf_out"
    echo "Wrote $out/$elf_out"
  fi
  rm -rf "$build_dir"
}
