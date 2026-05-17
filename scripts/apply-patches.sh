#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/meshcore-env.sh"

if [ ! -d "${MESHCORE_DIR}/.git" ] && [ ! -f "${MESHCORE_DIR}/.git" ]; then
  echo "MeshCore submodule is missing at ${MESHCORE_DIR}" >&2
  echo "Run: git submodule update --init --recursive" >&2
  exit 1
fi

if [ -n "$(git -C "${MESHCORE_DIR}" status --porcelain)" ]; then
  echo "MeshCore submodule has uncommitted or untracked changes; commit, export, or reset them before applying patches." >&2
  exit 1
fi

shopt -s nullglob
patches=("${PATCH_DIR}"/*.patch)
shopt -u nullglob

if [ "${#patches[@]}" -eq 0 ]; then
  echo "No MeshCore patches to apply. MeshCore commit: $(meshcore_commit)"
  exit 0
fi

for patch in "${patches[@]}"; do
  echo "Applying ${patch#${MESHCORE_FW_ROOT}/}"
  git -C "${MESHCORE_DIR}" apply --index "$patch"
done

echo "Applied ${#patches[@]} MeshCore patch(es) to $(meshcore_commit)"

python3 "${MESHCORE_FW_ROOT}/scripts/enable-bot-on-companion-envs.py"
