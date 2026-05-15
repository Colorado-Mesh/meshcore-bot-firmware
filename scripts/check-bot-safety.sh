#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/meshcore-env.sh"

bot_sources=(
  "${MESHCORE_DIR}/examples/companion_radio/BotTypes.h"
  "${MESHCORE_DIR}/examples/companion_radio/FirmwareBot.h"
  "${MESHCORE_DIR}/examples/companion_radio/FirmwareBot.cpp"
  "${MESHCORE_DIR}/examples/companion_radio/BotCommands.h"
  "${MESHCORE_DIR}/examples/companion_radio/BotCommands.cpp"
  "${MESHCORE_DIR}/examples/companion_radio/BotPolicy.h"
  "${MESHCORE_DIR}/examples/companion_radio/BotPolicy.cpp"
  "${MESHCORE_DIR}/examples/companion_radio/BotPrefs.h"
  "${MESHCORE_DIR}/examples/companion_radio/BotPrefs.cpp"
  "${MESHCORE_DIR}/examples/companion_radio/EmergencyForwarder.h"
  "${MESHCORE_DIR}/examples/companion_radio/EmergencyForwarder.cpp"
  "${MESHCORE_DIR}/examples/companion_radio/KnownBotRegistry.h"
  "${MESHCORE_DIR}/examples/companion_radio/KnownBotRegistry.cpp"
  "${MESHCORE_DIR}/examples/companion_radio/ResponseCoordinator.h"
  "${MESHCORE_DIR}/examples/companion_radio/ResponseCoordinator.cpp"
)

missing=0
for file in "${bot_sources[@]}"; do
  if [ ! -f "$file" ]; then
    echo "Missing bot source: ${file#${MESHCORE_FW_ROOT}/}" >&2
    missing=1
  fi
done
if [ "$missing" -ne 0 ]; then
  exit 1
fi

forbidden='\bString\b|std::|\bnew[[:space:]]|\bdelete[[:space:]]|\b(malloc|calloc|realloc|free)[[:space:]]*\(|ArduinoJson|JsonDocument|HTTPClient|WiFiClient|WebServer|AsyncWebServer|socket[[:space:]]*\('
if grep -n -E "$forbidden" "${bot_sources[@]}"; then
  echo "Forbidden dynamic allocation, container, JSON, or network API found in firmware bot sources." >&2
  exit 1
fi

forbidden_timing='\b(txdelay|rxdelay)\b|direct[._-]?tx[._-]?delay|airtime_factor|rx_delay_base'
if grep -n -E -i "$forbidden_timing" "${bot_sources[@]}"; then
  echo "Forbidden lower-layer timing dependency found in firmware bot sources." >&2
  exit 1
fi

require_pattern() {
  local pattern="$1"
  shift
  local label="$1"
  shift
  if ! grep -q -E "$pattern" "$@"; then
    echo "Missing safety marker: $label" >&2
    exit 1
  fi
}

require_pattern 'BOT_POLICY_IGNORE' 'normal Public bot traffic is ignored silently' \
  "${MESHCORE_DIR}/examples/companion_radio/BotPolicy.cpp" \
  "${MESHCORE_DIR}/examples/companion_radio/BotTypes.h"
require_pattern 'BOT_POLICY_EMERGENCY_FORWARD' 'emergency traffic forwarding policy exists' \
  "${MESHCORE_DIR}/examples/companion_radio/BotPolicy.cpp" \
  "${MESHCORE_DIR}/examples/companion_radio/BotPolicy.h"
require_pattern 'recordBotObservation\(|sendQueuedEmergencyForwards\(|tickBot\(' 'emergency path is wired in MyMesh' \
  "${MESHCORE_DIR}/examples/companion_radio/MyMesh.cpp"
require_pattern '_prefs\.path_hash_mode[[:space:]]*=[[:space:]]*1' 'bot firmware defaults to two-byte path hashes' \
  "${MESHCORE_DIR}/examples/companion_radio/MyMesh.cpp"
require_pattern 'CMESH_BOT_ENABLED=1' 'production bot build flag is enabled' \
  "${MESHCORE_DIR}/platformio.ini"
require_pattern 'ENABLE_PRIVATE_KEY_IMPORT=0' 'private key import disabled in production bot flags' \
  "${MESHCORE_DIR}/platformio.ini"
require_pattern 'ENABLE_PRIVATE_KEY_EXPORT=0' 'private key export disabled in production bot flags' \
  "${MESHCORE_DIR}/platformio.ini"
require_pattern '\$\{cmesh_bot_production\.build_flags\}' 'representative envs include production bot flags' \
  "${MESHCORE_DIR}/variants/heltec_v3/platformio.ini" \
  "${MESHCORE_DIR}/variants/rak4631/platformio.ini"

count="$(grep -h -E '\$\{cmesh_bot_production\.build_flags\}' \
  "${MESHCORE_DIR}/variants/heltec_v3/platformio.ini" \
  "${MESHCORE_DIR}/variants/rak4631/platformio.ini" | wc -l | tr -d ' ')"
if [ "$count" -lt 4 ]; then
  echo "Expected production bot flags in four representative envs, found $count." >&2
  exit 1
fi

echo "Firmware bot safety checks passed."
