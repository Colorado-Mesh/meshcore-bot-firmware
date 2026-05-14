#!/usr/bin/env bash

meshcore_fw_repo_root() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  (cd "${script_dir}/.." && pwd)
}

MESHCORE_FW_ROOT="${MESHCORE_FW_ROOT:-$(meshcore_fw_repo_root)}"
MESHCORE_DIR="${MESHCORE_DIR:-${MESHCORE_FW_ROOT}/vendor/MeshCore}"
PATCH_DIR="${PATCH_DIR:-${MESHCORE_FW_ROOT}/patches/meshcore}"

REPRESENTATIVE_ENVS=(
  Heltec_v3_companion_radio_usb
  Heltec_v3_companion_radio_ble
  RAK_4631_companion_radio_usb
  RAK_4631_companion_radio_ble
)

meshcore_commit() {
  git -C "${MESHCORE_DIR}" rev-parse HEAD
}
