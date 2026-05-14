# Step 2 Execution Plan: Add representative build and size-report tooling

## Goal
Create local and CI tooling that applies the MeshCore patch queue, builds the four representative companion environments, captures per-environment logs/artifacts, and emits a deterministic size JSON summary.

## Current Code Observations
- `vendor/MeshCore/build.sh` provides `build-firmware <target>` and copies artifacts to its own `out/` directory after each build.
- `vendor/MeshCore/build.sh` removes `vendor/MeshCore/out` at startup, so wrapper tooling must copy artifacts after each environment build before starting the next one.
- `vendor/MeshCore/build.sh` requires `FIRMWARE_VERSION` in the environment and uses the current MeshCore git SHA in artifact filenames.
- Upstream CI installs Python 3.11 and PlatformIO with `pip install --upgrade platformio`, then runs `build.sh build-companion-firmwares`.
- Representative envs exist in `variants/heltec_v3/platformio.ini` and `variants/rak4631/platformio.ini`; RAK4631 companion envs set `board_upload.maximum_size = 712704`.
- The wrapper currently has no `.gitignore`, no `.github/workflows/`, and no build output tooling.

## Files to Change
- `scripts/build-representative.sh` — new wrapper build script for patch application, four-env builds, logs, copied artifacts, and size summary generation.
- `scripts/parse-size-report.py` — new parser for PlatformIO RAM/flash output plus artifact byte sizes, with optional baseline/compare support.
- `.github/workflows/firmware-build.yml` — new GitHub Actions workflow using the same representative build script as local development.
- `.gitignore` — ignore generated `out/` and local PlatformIO/cache artifacts if needed.
- `README.md` — add the local representative build command and output locations.

## Ordered Implementation Checklist
1. Add output ignores for wrapper-level generated build artifacts.
2. Implement `scripts/parse-size-report.py` with ANSI stripping, PlatformIO RAM/flash line parsing, artifact byte discovery, deterministic JSON output, and optional `--baseline` / `--compare <json>` arguments.
3. Implement `scripts/build-representative.sh` with `--help`, `--baseline`, `--compare <baseline-json>`, PlatformIO availability checks, patch application, per-env `build.sh build-firmware` invocation, per-env log capture, artifact copying, and parser invocation.
4. Ensure `build-representative.sh` uses `REPRESENTATIVE_ENVS` from `scripts/meshcore-env.sh`, sets a default `FIRMWARE_VERSION` when absent, and preserves each env result rather than hiding failures.
5. Add `.github/workflows/firmware-build.yml` to check out submodules, install Python 3.11 and PlatformIO, run `bash scripts/build-representative.sh --baseline`, and upload `out/size` plus `out/firmware` artifacts.
6. Update `README.md` with the representative build command and generated output paths.
7. Run shell syntax and Python compile checks, then run script help and the parser against a small synthetic log fixture without requiring PlatformIO.
8. If PlatformIO is available locally, run the full baseline build; otherwise record that full build verification is deferred to CI or a local PlatformIO install.

## Interfaces and Data Contracts
- `scripts/build-representative.sh [--baseline|--compare <baseline-json>]` builds all envs from `REPRESENTATIVE_ENVS` and writes outputs under wrapper-level `out/`.
- `scripts/build-representative.sh --help` prints usage and exits successfully without requiring PlatformIO.
- `scripts/parse-size-report.py --logs <log-dir> --artifacts <artifact-dir> [--baseline|--compare <baseline-json>] [--output <json>]` emits JSON containing:
  - `mode`
  - `generated_at`
  - `environments[]`
  - per-env `env`, `ram_used`, `ram_total`, `flash_used`, `flash_total`, `artifact_bytes`, `log`, `metrics_found`, and optional `delta`
- Missing RAM/flash metrics are represented as `null` and `metrics_found: false`; the parser must not invent successful measurements.
- CI must use the same local scripts instead of duplicating build logic.

## Verification Plan
- Automated:
  - `bash -n scripts/build-representative.sh scripts/meshcore-env.sh`
  - `python3 -m py_compile scripts/parse-size-report.py`
  - `bash scripts/build-representative.sh --help`
  - Run `scripts/parse-size-report.py` against a synthetic log/artifact directory.
  - If PlatformIO is installed: `bash scripts/build-representative.sh --baseline`
- Manual:
  - Review generated JSON shape from the synthetic parser run.
  - Confirm workflow env list comes from the wrapper script, not a duplicated matrix.
- Regression:
  - `bash scripts/apply-patches.sh` still succeeds with an empty patch queue.
  - Step 1 patch scripts remain unchanged in behavior.

## Stop Conditions
- Pause if upstream build env names differ from the planned four representative environments.
- Pause if implementing baseline comparison would require committing generated firmware artifacts.
- Pause if local PlatformIO installation or firmware build requires destructive cleanup outside wrapper-level `out/` or upstream `.pio` build directories.
