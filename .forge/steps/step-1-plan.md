# Step 1 Execution Plan: Bootstrap wrapper repository, upstream submodule, and patch workflow

## Goal
Create a reproducible Colorado Mesh firmware wrapper that pins upstream MeshCore and can apply/export Colorado bot patches without relying on untracked submodule edits.

## Current Code Observations
- Local MeshCore exists at `/Users/cjvana/Documents/GitHub/MeshCore` commit `6b52fb32301c273fc78d96183501eb23ad33c5bb`, but research recommends pinning the wrapper submodule independently.
- MeshCore builds from its repo root with `build.sh build-firmware <env>` and expects `FIRMWARE_VERSION` in the environment before building.
- MeshCore `platformio.ini` includes all variant configs via `variants/*/platformio.ini` and currently enables `ENABLE_PRIVATE_KEY_IMPORT=1` / `ENABLE_PRIVATE_KEY_EXPORT=1`; key hardening is a later step.
- Representative environments exist in the local checkout: `Heltec_v3_companion_radio_usb`, `Heltec_v3_companion_radio_ble`, `RAK_4631_companion_radio_usb`, and `RAK_4631_companion_radio_ble`.
- PlatformIO is not installed locally right now, so Step 1 verification must be limited to git/submodule/script behavior and shell syntax.

## Files to Change
- `.gitmodules` — created by `git submodule add`.
- `vendor/MeshCore` — submodule pointer to upstream MeshCore.
- `scripts/meshcore-env.sh` — shared wrapper environment variables and helper functions.
- `scripts/apply-patches.sh` — verify submodule state and apply sorted patch queue.
- `scripts/export-patches.sh` — regenerate patch queue from a named submodule branch/range.
- `patches/meshcore/.gitkeep` — keep empty patch queue directory tracked.
- `colorado/README.md` — short notes for Colorado overlay intent.
- `README.md` — wrapper workflow instructions for future contributors.

## Ordered Implementation Checklist
1. Add `vendor/MeshCore` as a git submodule from `https://github.com/meshcore-dev/MeshCore.git` and leave it pinned at the resolved commit.
2. Create `scripts/`, `patches/meshcore/`, and `colorado/` directories.
3. Implement `scripts/meshcore-env.sh` with repo-root detection, `MESHCORE_DIR`, `PATCH_DIR`, representative env list, and `meshcore_commit()` helper.
4. Implement `scripts/apply-patches.sh` with strict shell settings, submodule existence check, dirty submodule check, empty-patch success path, sorted patch application, and clear status output.
5. Implement `scripts/export-patches.sh` with strict shell settings, dirty submodule guidance, configurable base ref defaulting to `origin/main`, deterministic patch regeneration, and refusal to export when there are no commits beyond the base.
6. Add minimal README/development notes describing submodule init, patch apply, patch export, and representative env names.
7. Run shell syntax checks and `scripts/apply-patches.sh` with the empty patch queue.
8. Stage only Step 1 files for review.

## Interfaces and Data Contracts
- `scripts/meshcore-env.sh` can be sourced by other scripts and must not execute builds itself.
- `scripts/apply-patches.sh` exits 0 with an empty patch queue and exits non-zero if the submodule is missing, dirty, or patch application fails.
- `scripts/export-patches.sh [base-ref]` regenerates `patches/meshcore/*.patch` from commits after `base-ref`; it does not export uncommitted submodule edits.
- Representative env names are exposed as the shell array `REPRESENTATIVE_ENVS`.

## Verification Plan
- Automated:
  - `git submodule status`
  - `bash -n scripts/meshcore-env.sh scripts/apply-patches.sh scripts/export-patches.sh`
  - `bash scripts/apply-patches.sh`
  - `git status --short`
- Manual:
  - Confirm `vendor/MeshCore` points to a real upstream commit.
  - Confirm an empty patch queue leaves the submodule unchanged.
- Regression:
  - No firmware source files should be modified in Step 1.
  - `.forge/` artifacts are not staged as product source except required Forge documents remain available for review.

## Stop Conditions
- Stop and ask if `git submodule add` fails due to network/auth issues.
- Stop and ask if `vendor/MeshCore` already exists with unexpected contents.
- Stop and ask before deleting or overwriting any existing non-Forge files.
- Stop if the submodule checkout has unexpected dirty changes immediately after adding it.
