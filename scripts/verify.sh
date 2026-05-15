#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/meshcore-env.sh"

usage() {
  cat <<'EOF'
Usage: scripts/verify.sh [--no-build|--build]

Runs local pre-review checks. By default, this applies the MeshCore patch queue
when needed, runs host tests, runs firmware bot safety checks, and compiles
Python helper scripts. Use --build to also run representative firmware builds.
EOF
}

run_build=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --help|-h)
      usage
      exit 0
      ;;
    --no-build)
      run_build=0
      shift
      ;;
    --build)
      run_build=1
      shift
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [ ! -d "${MESHCORE_DIR}/.git" ] && [ ! -f "${MESHCORE_DIR}/.git" ]; then
  echo "MeshCore submodule is missing at ${MESHCORE_DIR}" >&2
  echo "Run: git submodule update --init --recursive" >&2
  exit 1
fi

if ! git -C "${MESHCORE_DIR}" diff --quiet --cached || ! git -C "${MESHCORE_DIR}" diff --quiet || [ -n "$(git -C "${MESHCORE_DIR}" ls-files --others --exclude-standard)" ]; then
  echo "MeshCore submodule is dirty; verify existing patched tree without applying patches."
else
  "${MESHCORE_FW_ROOT}/scripts/apply-patches.sh"
fi

python3 "${MESHCORE_FW_ROOT}/tests/firmware_bot/run_tests.py"
bash "${MESHCORE_FW_ROOT}/scripts/check-bot-safety.sh"
python3 -m py_compile \
  "${MESHCORE_FW_ROOT}/scripts/parse-size-report.py" \
  "${MESHCORE_FW_ROOT}/tests/firmware_bot/run_tests.py"

if [ "$run_build" -eq 1 ]; then
  MESHCORE_SKIP_APPLY_PATCHES=1 bash "${MESHCORE_FW_ROOT}/scripts/build-representative.sh" --baseline
fi

echo "Verification complete."
