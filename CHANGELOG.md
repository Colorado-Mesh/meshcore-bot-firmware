# Changelog

All notable changes to this firmware are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versioning
follows the `cmesh-bot-vX.Y.Z` scheme described in [RELEASE.md](RELEASE.md).

## [Unreleased]

## [cmesh-bot-v0.2.3] - 2026-10-12

### Changed

- Updated the MeshCore backend from 1.16.0 (`bbb58cce`) to the stable
  companion 1.17.1 release (`d9296435`), preserving bot preferences and
  adapting the patch queue to the new preference storage and USB/routing APIs.
- Release coverage increased to 153 companion USB/BLE environments.
- Replaced the upstream float formatter's `ltoa` call with portable integer
  formatting and pinned the compatible STM32 19.7.1 platform/Arduino 2.12 core.
  The newer unpinned core 3.0 changes GPIO types used by the pinned RadioLib.
- Enabled STM32 link-time optimization to retain the full bot command set
  within the Wio E5 Mini flash budget; separated companion preference types
  from the common CLI types to avoid conflicting definitions during linking.
- Enabled compilation and linking with LTO for Station G3 BLE to fit the
  existing application partition while retaining its features and storage layout.
- Updated GitHub Actions and pinned PlatformIO 6.2.0; disabled debug logging
  while retaining the NRF52 framework's required `CFG_DEBUG` definition.

### Fixed

- Scoped bot response coordination to its source channel and isolated direct
  messages from group response tokens.
- Validated encrypted packet lengths, returned path bounds, and companion
  response frame sizes before processing; rejected unsolicited zero-tag responses.
- Rejected overlapping channel roles that differ only by case.
- Production flag injection now honors customized MeshCore checkout paths,
  with effective bot/private-key flags checked for every companion environment.
- Release builds now fail on missing or empty target artifacts and generate
  a manifest and SHA-256 checksums before creating the draft release. DFU ZIP
  requirements follow the configured upload protocol; J-Link targets also
  include their HEX image for debugger programming.
- Corrected channel provisioning, response delay, and patch development docs.

### Validation

- Representative CI now covers all four platforms across eight environments,
  including the flash-constrained Station G3 BLE and Wio E5 Mini targets.
- Host bot regressions include channel isolation and overlapping channel roles.
- Native packet regressions use the real Crypto library and production response
  handler under AddressSanitizer and UndefinedBehaviorSanitizer.
- Hardware flashing, over-the-air behavior, and bootloader policies require
  device verification; source/host/build validation does not cover those checks.

## [cmesh-bot-v0.2.2] - 2026-10-12

Unpublished build tag. The full matrix exposed a Station G3 BLE application
size overflow, fixed with LTO in v0.2.3 while preserving its partition layout.

## [cmesh-bot-v0.2.1] - 2026-10-12

Unpublished build tag. Superseded after correcting the artifact checker's
DFU ZIP requirement for J-Link targets.

## [cmesh-bot-v0.2.0] - 2026-10-12

Unpublished build tag. The full matrix exposed an upstream STM32 compilation
error; no release artifacts were published. Its changes are included in v0.2.3.

## [cmesh-bot-v0.1.0] - 2026-07-10

First tagged release.

### Added

- Signal report command `sig` (aliases `snr`, `rssi`, `signal`): shows the
  SNR the bot heard your request at, plus its last RSSI and noise floor.
  Made for range testing.
- Airtime command `air` (alias `airtime`): TX/RX airtime and flood/direct
  packet counters.
- Coin flip command `coin` (aliases `flip`, `coinflip`).
- `cmd diag` lists the diagnostic command set separately, since the full
  command list no longer fits one group-channel message.
- Hop-aware response coordination: bot delays its reply proportional to
  the sender's hop count (2 s per hop plus a quadratic term, wide caps)
  so the closest bot wins; far bots suppress via response fingerprints
  and request tokens.
- Utility commands: `time`, `lora`, `id`, `neighbors`.
- Neighbor tracking: bot remembers nodes heard directly within the last
  hour for the `neighbors` command.
- `received_at_timestamp` field on bot messages; personalized ack target
  in command context.
- Full repository scaffolding: README, CONTRIBUTING, LICENSE, CHANGELOG,
  RELEASE doc, issue/PR templates, release workflow building every
  non-excluded companion USB+BLE environment.

### Changed

- MeshCore base updated from `910b1bee` to `bbb58cce` (upstream v1.16.0,
  272 commits: 5-byte ACKs, flood scope keys, NRF52 power saving, RP2040
  build fixes, contacts sync fix, and more).
- `help` and `cmd` listings are now derived from the command registry
  instead of hand-maintained lists that had drifted (`magic8` was missing
  from both).
- `BOT_PREFS_VERSION` bumped to 7 so deployed bots re-default their
  command mask and pick up the commands added in this release.
- Re-included six boards that build again on the new base after passing
  local test builds: Pico W, RAK 11310, Waveshare RP2040 LoRa, Xiao
  RP2040, and Nibble Screen Connect (USB and BLE). The remaining
  exclusions are BLE variants that don't fit flash.

### Fixed

- Release env enumeration now strips CR line endings, so companion
  environments defined in variant files with CRLF (Minewsemi ME25LS01,
  Wio WM1110, Nibble) are no longer silently missing from releases.
- Multi-hop senders on `#bot` no longer get silently ignored. Previously
  3+ hop responses scheduled past the 15 s TTL and were dropped by the
  coordinator's `poll()` as `READY_EXPIRED`. Verified on a Heltec V3
  bench bot replying at 2, 4, and 6 hops.
