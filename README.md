# Colorado Mesh Bot Firmware

[![Firmware Build](https://github.com/Colorado-Mesh/meshcore-bot-firmware/actions/workflows/firmware-build.yml/badge.svg)](https://github.com/Colorado-Mesh/meshcore-bot-firmware/actions/workflows/firmware-build.yml)
[![Latest Release](https://img.shields.io/github/v/release/Colorado-Mesh/meshcore-bot-firmware?include_prereleases&sort=semver)](https://github.com/Colorado-Mesh/meshcore-bot-firmware/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Companion-radio firmware for the Colorado Mesh community, built on top of
[MeshCore](https://github.com/meshcore-dev/MeshCore). It adds an in-firmware
"firmware bot" that responds to chat commands like `ping`, `trace`, `path`,
`status`, `neighbors`, `sig`, and `magic8`, with multi-bot response
coordination so nearby Colorado bots don't all reply at once.

The bot runs entirely on-device: no internet, no companion-app bridge, no
cloud. Useful as a low-friction probe / utility node on the Colorado mesh.

## What's different from upstream MeshCore

The release-built firmware applies a patch queue (`patches/meshcore/`) on top
of a pinned MeshCore commit. The patches add:

- A bot command registry (`help`, `cmd`, `ping`, `test`, `hello`, `about`,
  `roll`, `dice`, `coin`, `status`, `channels`, `version`, `stats`, `magic8`,
  `path`, `trace`, `tracer`, `prefix`, `time`, `lora`, `id`, `neighbors`,
  `sig`, `air`)
- Channel-aware response policy (`#bot`, `#testing` for normal commands;
  `#emergency` for emergency forwards to `Public`; all other channels ignored)
- Multi-bot response coordinator with hop-aware delay, bounded TTL, and
  fingerprint-based suppression so duplicates are avoided across the mesh
- Direct-message (DM) prefixless command parsing
- Two-byte path hash default for compact traces
- Private-key import/export disabled in release builds
- Host-side bot prefs (CLI tunable channels, delay, advert intervals,
  known-bot list)

See `patches/meshcore/` for the full set; each patch is a standalone commit
re-exported from the submodule.

## Supported boards

The release workflow builds **every** `*_companion_radio_usb` and
`*_companion_radio_ble` PlatformIO environment exposed by upstream MeshCore,
minus a short exclusion list of BLE variants that don't fit flash (see
`scripts/list-release-companion-envs.sh`). That is 139 boards at the current
pin, across ESP32, NRF52, RP2040, and STM32 platforms (Heltec V3, RAK 4631,
LilyGo T-Echo, T-Beam, T-Deck, Xiao S3, Nano G2, ThinkNode, Meshtiny, Pico W,
and many more). See the
[latest release assets](https://github.com/Colorado-Mesh/meshcore-bot-firmware/releases/latest)
for the full list.

PR CI builds a smaller "representative" matrix (Heltec V3 USB+BLE, RAK 4631
USB+BLE) on every push, so the most common boards get fast feedback.

## Install

Three ways to flash:

### MeshCore web flasher (easiest)

1. Open [flasher.meshcore.io](https://flasher.meshcore.io) in Chrome/Edge.
2. Pick "Custom firmware" and drag in the `*-merged.bin` (ESP32) or `*.uf2`
   (NRF52) file from the
   [latest release](https://github.com/Colorado-Mesh/meshcore-bot-firmware/releases/latest)
   for your board.
3. Plug the board in, click "Connect", and flash.

### PlatformIO (from source)

```sh
git clone --recurse-submodules https://github.com/Colorado-Mesh/meshcore-bot-firmware.git
cd meshcore-bot-firmware
python3 -m venv .venv && .venv/bin/python -m pip install --upgrade platformio
bash scripts/apply-patches.sh
cd vendor/MeshCore
pio run -e Heltec_v3_companion_radio_usb -t upload --upload-port /dev/cu.usbserial-0001
```

Swap the env name for your board (see
`grep -rE '^\[env:' vendor/MeshCore/variants/`).

### esptool / nrfutil (manual)

ESP32 boards: `esptool.py write_flash 0x0 <board>-merged.bin`
NRF52 boards: drag the `.uf2` onto the bootloader mass-storage volume, or
use `adafruit-nrfutil dfu serial -pkg <board>.zip -p <port>`.

## Using the bot

Once flashed, the bot joins the channels configured in its prefs. By default
those are `#bot`, `#testing`, `#emergency`, and `Public`. Send commands on
`#bot` (or DM the bot directly) without any prefix, for example:

| Command | Response |
|---|---|
| `ping` | `Pong` |
| `hello` | `Hello @[<your-name>], from <bot-name>` |
| `path` | Compact route summary back to you, e.g. `Path 3 hops, 2-byte hashes, SNR -6.25 \| 2751 -> ea4d -> 430d` |
| `trace` | Active trace request along your reverse path |
| `sig` | How the bot heard you: request SNR, plus local RSSI and noise floor |
| `status` | Bot uptime, battery, storage, send counters |
| `neighbors` | Nodes heard directly within the last hour |
| `air` | TX/RX airtime and flood/direct packet counters |
| `version` | Firmware version + build date |
| `magic8 <question>` | Classic 8-ball answer |
| `roll`, `dice`, `coin` | Random numbers, D&D dice, coin flips |
| `help` | List chat commands (`cmd diag` lists the diagnostic set) |

In any other channel the bot stays silent (except `#emergency`, which it
re-forwards to `Public`).

If you're testing alongside other Colorado bots, the response coordinator
adds a hop-aware delay (~1.5 s per hop, capped at 8 s) so the closest bot
wins the race and others suppress. This avoids spam on the channel.

## Layout

- `vendor/MeshCore/`: pinned upstream MeshCore submodule.
- `patches/meshcore/`: ordered patch queue applied to the submodule.
- `colorado/`: Colorado Mesh overlay files, fixtures, and notes.
- `scripts/`: wrapper scripts for patch, build, verify, and size-report
  workflows.
- `tests/firmware_bot/`: host-side C++ unit tests for the bot code
  (compile and run on your machine, no hardware required).
- `.github/workflows/`: PR CI (`firmware-build.yml`) and tag-driven release
  (`release.yml`).

## Local development

Initialize the submodule and apply patches:

```sh
git submodule update --init --recursive
bash scripts/apply-patches.sh
```

Make firmware changes inside `vendor/MeshCore`, commit them in the submodule
worktree, then export the patch queue:

```sh
bash scripts/export-patches.sh origin/main
```

Build the four representative companion environments and write size reports:

```sh
bash scripts/build-representative.sh --baseline
```

Run host tests + safety checks:

```sh
python3 tests/firmware_bot/run_tests.py
bash scripts/check-bot-safety.sh
# Or all in one go:
MESHCORE_SKIP_APPLY_PATCHES=1 bash scripts/verify.sh --no-build
```

## Releases

Releases are cut by pushing a `cmesh-bot-vX.Y.Z` tag. See
[RELEASE.md](RELEASE.md) for the full procedure.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). The short version: edit inside
`vendor/MeshCore`, re-export the patch queue, run `verify.sh`, open a PR.
Patches stay in `patches/meshcore/` so reviewers can see exactly what
deviates from upstream.

## License

MIT, including upstream MeshCore. See [LICENSE](LICENSE) and
[vendor/MeshCore/license.txt](vendor/MeshCore/license.txt).
