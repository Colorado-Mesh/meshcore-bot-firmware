# Changelog

All notable changes to this firmware are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versioning
follows the `cmesh-bot-vX.Y.Z` scheme described in [RELEASE.md](RELEASE.md).

## [Unreleased]

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
