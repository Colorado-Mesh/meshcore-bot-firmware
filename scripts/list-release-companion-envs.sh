#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MESHCORE_DIR="${MESHCORE_DIR:-${ROOT_DIR}/vendor/MeshCore}"

is_excluded() {
  case "$1" in
    Heltec_v2_companion_radio_ble|\
    LilyGo_TLora_V2_1_1_6_companion_radio_ble|\
    Station_G2_companion_radio_ble|\
    Tbeam_SX1262_companion_radio_ble|\
    Tbeam_SX1276_companion_radio_ble)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

grep -rhE '^\[env:' "${MESHCORE_DIR}/variants/" \
  | sort -u \
  | grep -E '_companion_radio_(usb|ble)\]$' \
  | sed -e 's/\[env://' -e 's/\]//' \
  | while IFS= read -r env; do
      if ! is_excluded "$env"; then
        printf '%s\n' "$env"
      fi
    done
