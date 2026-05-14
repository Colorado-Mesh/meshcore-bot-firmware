#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/meshcore-env.sh"

base_ref="${1:-origin/main}"

if [ ! -d "${MESHCORE_DIR}/.git" ] && [ ! -f "${MESHCORE_DIR}/.git" ]; then
  echo "MeshCore submodule is missing at ${MESHCORE_DIR}" >&2
  echo "Run: git submodule update --init --recursive" >&2
  exit 1
fi

if ! git -C "${MESHCORE_DIR}" rev-parse --verify "${base_ref}" >/dev/null 2>&1; then
  echo "Base ref '${base_ref}' is not available in the MeshCore submodule." >&2
  exit 1
fi

if [ -n "$(git -C "${MESHCORE_DIR}" status --porcelain)" ]; then
  echo "MeshCore submodule has uncommitted or untracked changes; commit them in the submodule before exporting patches." >&2
  exit 1
fi

commit_count="$(git -C "${MESHCORE_DIR}" rev-list --count "${base_ref}..HEAD")"
if [ "${commit_count}" -eq 0 ]; then
  echo "No MeshCore commits to export after ${base_ref}."
  exit 1
fi

mkdir -p "${PATCH_DIR}"
rm -f "${PATCH_DIR}"/*.patch
git -C "${MESHCORE_DIR}" format-patch --zero-commit --no-signature --output-directory "${PATCH_DIR}" "${base_ref}..HEAD" >/dev/null

echo "Exported ${commit_count} MeshCore patch(es) to ${PATCH_DIR#${MESHCORE_FW_ROOT}/}"
