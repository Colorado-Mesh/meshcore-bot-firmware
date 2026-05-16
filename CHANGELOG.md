# Changelog

All notable changes to this firmware are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versioning
follows the `cmesh-bot-vX.Y.Z` scheme described in [RELEASE.md](RELEASE.md).

## [Unreleased]

### Added

- Hop-aware response coordination: bot delays its reply proportional to
  the sender's hop count so the closest bot wins; far bots suppress.
- Utility commands: `time`, `lora`, `id`, `neighbors`.
- Neighbor tracking: bot remembers nodes heard directly within the last
  hour for the `neighbors` command.
- `received_at_timestamp` field on bot messages; personalized ack target
  in command context.
- Comprehensive repository scaffolding: README, CONTRIBUTING, LICENSE,
  CHANGELOG, RELEASE doc, issue/PR templates, release workflow building
  all 133 companion USB+BLE environments.

### Changed

- `BOT_HOP_STEP_MILLIS_DEFAULT` lowered from 5000 to 1500 ms per hop.
- Added `BOT_HOP_BIAS_MAX_MILLIS` cap of 8000 ms on the total hop bias.
- `BOT_RESPONSE_PENDING_TTL_MILLIS` raised from 15000 to 45000 ms.
- `BOT_PREFS_VERSION` bumped 2 → 3 to force re-default of in-the-wild
  bots that saved the broken 5000 ms hop-step value.

### Fixed

- Multi-hop senders on `#bot` no longer get silently ignored. Previously
  3+ hop responses scheduled past the 15 s TTL and were dropped by the
  coordinator's `poll()` as `READY_EXPIRED`. Verified on a Heltec V3
  bench bot replying at 2, 4, and 6 hops.
