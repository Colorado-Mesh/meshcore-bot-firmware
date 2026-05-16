# Contributing to Colorado Mesh Bot Firmware

Thanks for your interest. This repo is a thin wrapper around upstream
MeshCore: most firmware code lives in `vendor/MeshCore/` (a git submodule)
and is applied via the patch queue in `patches/meshcore/`. Colorado-only
overlay files, tests, and tooling live at the wrapper level.

## Quick reference

| Task | Command |
|---|---|
| Bootstrap | `git submodule update --init --recursive && bash scripts/apply-patches.sh` |
| Run host tests + safety checks | `MESHCORE_SKIP_APPLY_PATCHES=1 bash scripts/verify.sh --no-build` |
| Build representative firmwares | `bash scripts/build-representative.sh --baseline` |
| Export patch queue after edits | `bash scripts/export-patches.sh origin/main` |

## Development workflow

The submodule is a real git repo. Iterate inside it like normal:

1. **Bootstrap once** (clone with `--recurse-submodules`, or run
   `git submodule update --init --recursive`).
2. **Apply patches** with `bash scripts/apply-patches.sh`. This commits the
   patch queue on top of `origin/main` in the submodule, leaving you with
   a clean tree pinned to a known state.
3. **Make changes inside `vendor/MeshCore/`** — edit files, run
   `pio run -e <env>` to compile, commit your changes in the submodule
   with descriptive messages.
4. **Re-export the patch queue** with
   `bash scripts/export-patches.sh origin/main`. This deletes
   `patches/meshcore/*.patch` and regenerates it from the submodule
   commits since `origin/main`. Each patch corresponds to one submodule
   commit, so keep commits small and focused.
5. **Run host tests** (`python3 tests/firmware_bot/run_tests.py`) — fast,
   no hardware needed.
6. **Run safety checks** (`bash scripts/check-bot-safety.sh`) — verifies
   the firmware doesn't expose private keys, network bridges, or other
   debug-only features in release builds.
7. **(Optional) Build representative envs** with
   `bash scripts/build-representative.sh --baseline` to confirm the
   binaries still link and to inspect size impact in `out/size/summary.json`.
8. **Commit at the wrapper level** with the regenerated patches, any
   test/script changes, and the updated submodule pointer. PR-ready.

## What goes where

- **Firmware/library code that MUST live inside MeshCore** for PlatformIO
  builds — develop inside `vendor/MeshCore/`, export as a patch.
- **Test code (host-side C++ tests)** — `tests/firmware_bot/` at the
  wrapper level. They `#include` MeshCore headers via the script's `-I`
  flag.
- **Tooling, build scripts, CI** — `scripts/` and `.github/` at the
  wrapper level.
- **Colorado-specific docs, fixtures, board notes** — `colorado/`.

## Patch hygiene

- One conceptual change per patch / submodule commit.
- Prefer additive changes (`#if CMESH_BOT_ENABLED` guards, new files in
  `examples/companion_radio/`) over edits to broadly-used upstream code.
  This keeps merges with upstream MeshCore manageable.
- When changing a stored format (e.g., bot prefs), bump
  `BOT_PREFS_VERSION` so deployed bots reset to the new defaults
  instead of loading broken legacy state.
- Add a host test for new behavior whenever it's testable without
  hardware. The bot has 48+ host tests covering the registry, parser,
  policy, coordinator, and command output strings.

## CI

PRs run `.github/workflows/firmware-build.yml`, which:

1. Runs host tests + safety checks (`scripts/verify.sh --no-build`).
2. Builds the four representative companion environments (Heltec V3 and
   RAK 4631, USB + BLE).
3. Uploads firmware + size reports as workflow artifacts.

Releases run `.github/workflows/release.yml` on `cmesh-bot-v*` tags and
build all 133 companion USB+BLE environments — see [RELEASE.md](RELEASE.md).

## Pull request expectations

- One PR per logical change. Bundle related patches; keep unrelated work
  in separate PRs.
- Describe the **why** as well as the what.
- If you touched response timing, coordination, or anything that could
  silently drop messages, include a host test that exercises the
  failure mode and explain how you verified on hardware (which boards,
  what commands, what hop counts).
- CI must be green.
- Don't commit `.forge/` workflow artifacts, the `.venv/` directory, or
  PlatformIO build output. They're gitignored.

## Reporting bugs

[Open an issue](https://github.com/Colorado-Mesh/meshcore-bot-firmware/issues/new/choose).
For firmware bugs, include the output of `bot stats` (over USB CLI rescue),
the firmware version, board, and a minimal reproduction.

## Code of conduct

Be respectful, be patient, assume good faith. We're all volunteers building
infrastructure for a mesh community.
