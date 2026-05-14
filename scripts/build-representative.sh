#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/meshcore-env.sh"

usage() {
  cat <<'EOF'
Usage: scripts/build-representative.sh [--baseline|--compare <baseline-json>]

Applies the MeshCore patch queue, builds the representative companion firmware
environments, copies firmware artifacts to out/firmware, and writes a size
summary to out/size/summary.json.
EOF
}

mode="summary"
compare_file=""
mode_selected=0

while [ "$#" -gt 0 ]; do
  case "$1" in
    --help|-h)
      usage
      exit 0
      ;;
    --baseline)
      if [ "$mode_selected" -eq 1 ]; then
        echo "Choose only one of --baseline or --compare." >&2
        exit 2
      fi
      mode="baseline"
      mode_selected=1
      shift
      ;;
    --compare)
      if [ "$mode_selected" -eq 1 ]; then
        echo "Choose only one of --baseline or --compare." >&2
        exit 2
      fi
      if [ "$#" -lt 2 ]; then
        echo "--compare requires a baseline JSON path" >&2
        exit 2
      fi
      mode="compare"
      mode_selected=1
      compare_file="$2"
      shift 2
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [ "$mode" = "compare" ] && [ ! -f "$compare_file" ]; then
  echo "Baseline JSON not found: $compare_file" >&2
  exit 1
fi

compare_tmp=""
if [ "$mode" = "compare" ]; then
  compare_tmp="$(mktemp)"
  cp "$compare_file" "$compare_tmp"
fi

cleanup() {
  if [ -n "$compare_tmp" ]; then
    rm -f "$compare_tmp"
  fi
}
trap cleanup EXIT

if ! command -v pio >/dev/null 2>&1; then
  if [ -x "${MESHCORE_FW_ROOT}/.venv/bin/pio" ]; then
    export PATH="${MESHCORE_FW_ROOT}/.venv/bin:${PATH}"
  else
    echo "PlatformIO is required to build representative firmware." >&2
    echo "Install it with pipx or a local venv, for example: python3 -m venv .venv && .venv/bin/python -m pip install --upgrade platformio" >&2
    exit 1
  fi
fi

if [ ! -f "${MESHCORE_DIR}/build.sh" ]; then
  echo "MeshCore build script is missing at ${MESHCORE_DIR}/build.sh" >&2
  exit 1
fi

"${MESHCORE_FW_ROOT}/scripts/apply-patches.sh"

OUT_DIR="${MESHCORE_FW_ROOT}/out"
LOG_DIR="${OUT_DIR}/size"
ARTIFACT_DIR="${OUT_DIR}/firmware"
SUMMARY_PATH="${LOG_DIR}/summary.json"

rm -rf "$OUT_DIR"
mkdir -p "$LOG_DIR" "$ARTIFACT_DIR"

export FIRMWARE_VERSION="${FIRMWARE_VERSION:-local}"

failed_envs=()
for env in "${REPRESENTATIVE_ENVS[@]}"; do
  log_path="${LOG_DIR}/${env}.log"
  echo "Building ${env}"
  if (cd "$MESHCORE_DIR" && /usr/bin/env bash build.sh build-firmware "$env") >"$log_path" 2>&1; then
    if compgen -G "${MESHCORE_DIR}/out/${env}-*" >/dev/null; then
      cp "${MESHCORE_DIR}/out/${env}-"* "$ARTIFACT_DIR"/
    fi
  else
    failed_envs+=("$env")
    echo "Build failed for ${env}; see ${log_path}" >&2
  fi
done

parser_args=(
  --logs "$LOG_DIR"
  --artifacts "$ARTIFACT_DIR"
  --output "$SUMMARY_PATH"
)
for env in "${REPRESENTATIVE_ENVS[@]}"; do
  parser_args+=(--env "$env")
done

case "$mode" in
  baseline)
    parser_args+=(--baseline)
    ;;
  compare)
    parser_args+=(--compare "$compare_tmp")
    ;;
esac

python3 "${MESHCORE_FW_ROOT}/scripts/parse-size-report.py" "${parser_args[@]}"

if [ "${#failed_envs[@]}" -gt 0 ]; then
  echo "Representative build failed for: ${failed_envs[*]}" >&2
  exit 1
fi

echo "Representative build complete. Summary: ${SUMMARY_PATH#${MESHCORE_FW_ROOT}/}"
