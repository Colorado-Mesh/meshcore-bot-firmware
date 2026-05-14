# Research Synthesis

## Status
- Files synthesized: stack.md, pitfalls.md, architecture.md, prior-art.md, codex-analysis.md, PROJECT.md
- Files missing: none
- Overall confidence: HIGH

## Executive Summary
This project is now a firmware-only MeshCore companion bot, not a host-side bot permission layer and not a VPS coordinator replacement by emulation. The proven path is to keep upstream `meshcore-dev/MeshCore` as the firmware/build substrate, use Arduino C++/PlatformIO exactly as upstream does, and add a compact Colorado Mesh bot runtime at the companion application layer. `agessaman/meshcore-bot`, the Colorado VPS/community coordinator, and stale Codex coordinator-first notes are useful only as behavioral/prior-art references; they must not drive the runtime architecture.

The recommended implementation is a small embedded command responder plus passive response coordinator inside `examples/companion_radio/MyMesh`: receive DMs and allowed channel messages, classify commands, enforce channel policy, compute semantic fingerprints, schedule bounded delayed responses, cancel when a trusted known bot has already answered, and send via existing MeshCore `sendMessage()` / `sendGroupMessage()` APIs. Normal bot traffic should be limited to private DMs, `#bot`, and `#testing`; `#emergency` should use a dedicated Public forwarding path with the exact required emergency announcement semantics and strict dedup/rate limiting. Do not repurpose MeshCore `txdelay`, `direct.txdelay`, or `rxdelay` for bot election; use bot-specific role/delay/suppression settings.

The top risks are RAK4631 flash/RAM limits, heap fragmentation, packet-pool exhaustion, unsafe text parsing, wrong channel routing, spoofed suppression, emergency amplification, hidden nodes, and companion protocol breakage. Mitigate by designing to RAK4631 first, keeping v1 commands deterministic and single-packet, using fixed static state with no hot-path heap allocation, keeping runtime suppression state in RAM, persisting only a small versioned `BotPrefs`, and measuring PlatformIO size deltas in CI. The user's plugged-in Heltec v3 is the correct first hardware smoke target because it is available and has generous flash, but RAK4631 BLE remains the release gate.

## Key Decisions (resolved by research)

1. **Build in MeshCore's firmware stack.** Use upstream MeshCore Arduino C++/PlatformIO and current upstream dependency pins; do not introduce ESP-IDF-only, Zephyr, Rust, embedded Python/JS, MQTT firmware logic, SQLite, or a custom radio stack. Source refs: ITEM-stack-1, ITEM-stack-4, ITEM-prior-art-1.
2. **Use a submodule plus patch/overlay workflow.** Keep `meshcore-dev/MeshCore` pinned as an upstream submodule and maintain Colorado bot code as a small patch/overlay series applied during builds. Source refs: ITEM-stack-2, ITEM-architecture-14.
3. **Implement a firmware-resident bot, not host-side coordination.** Phase 1 should generate selected bot responses directly in firmware; `meshcore-bot` is a behavior oracle only. Source refs: ITEM-stack-6, ITEM-prior-art-3, ITEM-pitfalls-2, ITEM-architecture-8.
4. **Place bot logic at the companion application layer.** Integrate with `MyMesh` callbacks and loop; leave `Dispatcher`, `Mesh::routeRecvPacket()`, ACKs, path returns, retransmit timing, and packet duplicate tables untouched. Source refs: ITEM-architecture-1, ITEM-stack-5.
5. **Use strict channel policy.** Normal bot handling is DMs, `#bot`, and `#testing`; Public is ignored except for the dedicated `#emergency` forwarding path. Source refs: ITEM-architecture-3, ITEM-stack-7, ITEM-prior-art-12, ITEM-pitfalls-7.
6. **Forward emergencies explicitly and safely.** `#emergency` should produce Public text beginning `EMERGENCY MESSAGE FROM <user>` and preserve/truncate original text with bounded one- or two-packet formatting, deduplication, loop prevention, and rate limiting. Source refs: ITEM-architecture-4, ITEM-prior-art-12, ITEM-pitfalls-8.
7. **Coordinate with passive listen-before-answer first.** Use semantic fingerprints, bounded delayed sends, and cancellation on trusted known-bot responses. Do not add claim frames in v1. Source refs: ITEM-architecture-5, ITEM-architecture-6, ITEM-pitfalls-9, ITEM-pitfalls-10.
8. **Trust known bot identities only.** Suppression must be keyed to known bot public keys where possible; plain group text names are spoofable hints and must not suppress emergency forwarding. Source refs: ITEM-architecture-7, ITEM-pitfalls-11.
9. **Design storage and RAM for RAK4631.** Heltec v3 is comfortable, but the nRF52840 RAK4631 app/RAM limits define the portable v1 budget. Source refs: ITEM-stack-9, ITEM-stack-10, ITEM-stack-11, ITEM-stack-12, ITEM-architecture-10, ITEM-pitfalls-1.
10. **Instrument and size-gate from the start.** CI should build Heltec v3 USB/BLE and RAK4631 USB/BLE, capture PlatformIO size output, and enforce bot delta thresholds after baseline. Source refs: ITEM-stack-8, ITEM-stack-13, ITEM-architecture-13, ITEM-pitfalls-1.
11. **Start hardware smoke tests on the plugged-in Heltec v3.** Use the local Heltec v3 for first flash/build smoke only after the implementation plan is approved; expand to RAK4631 before release. Source refs: PROJECT.md, ITEM-stack-8, ITEM-stack-9.

## Questions for User

### Q-1: Which exact firmware MVP commands should ship first?

- **Category:** scope
- **Why it matters:** Every command adds flash, response strings, parser cases, tests, and possible airtime. The v1 command set determines whether the feature remains firmware-feasible on RAK4631.
- **Default recommendation:** Ship `ping`, `test`, `help/cmd`, `about`, `status` with battery/storage, simple `dice/roll`, DM replies, `#bot`/`#testing` replies, and `#emergency` forwarding. Defer weather, AQI, sports, jokes via web APIs, satellite/solar forecasts, Discord/web viewer, SQLite stats, and repeater-management workflows.
- **Source refs:** ITEM-stack-6, ITEM-prior-art-3, ITEM-architecture-8, ITEM-pitfalls-2
- **Priority:** HIGH

### Q-2: Should the bot be enabled by default after flashing, or require explicit enable/configuration?

- **Category:** ux
- **Why it matters:** A firmware bot that responds immediately after flashing can surprise users or create channel noise if channels/known bots/tier are not configured correctly.
- **Default recommendation:** Default to enabled for DMs and local smoke-test commands only, but require explicit channel policy resolution and role configuration before responding on `#bot`/`#testing`; always require explicit `#emergency` source/destination channel validation before Public forwarding.
- **Source refs:** ITEM-architecture-3, ITEM-architecture-9, ITEM-pitfalls-7, ITEM-pitfalls-16
- **Priority:** HIGH

### Q-3: What should the default response role/tier be for the user's plugged-in Heltec v3 bot node?

- **Category:** ux
- **Why it matters:** The first hardware smoke target is also intended to become the user's bot node. Its default role affects response latency, suppression behavior, and whether it defers to fixed infrastructure.
- **Default recommendation:** Configure the plugged-in Heltec v3 as `LOCAL` or `SUBURBAN` for smoke tests, with a conservative 300-1500 ms response window; change role later based on deployment location.
- **Source refs:** PROJECT.md, ITEM-stack-9, ITEM-architecture-12, ITEM-pitfalls-14
- **Priority:** HIGH

### Q-4: What duplicate-response and latency targets are acceptable for v1?

- **Category:** constraints
- **Why it matters:** Passive suppression can reduce noise but cannot guarantee exactly-once behavior in hidden-node topologies. The target determines delay windows and whether claim frames become necessary later.
- **Default recommendation:** Accept rare duplicates in v1. Target p95 added latency under 3 seconds for normal channel commands and duplicate bot replies under 5-10% in field scenarios, with metrics before adding claim frames.
- **Source refs:** ITEM-architecture-6, ITEM-pitfalls-9, codex-analysis
- **Priority:** HIGH

### Q-5: How should known bot identities be provisioned and maintained?

- **Category:** technical
- **Why it matters:** Suppression should trust known bot identities only, but firmware needs a bounded, field-serviceable way to store and update public keys.
- **Default recommendation:** Store 16 known bot full public keys plus optional labels/capability flags in versioned `BotPrefs`; expose rescue CLI commands and later companion protocol commands to add/remove/list keys.
- **Source refs:** ITEM-architecture-7, ITEM-architecture-9, ITEM-architecture-10, ITEM-pitfalls-11
- **Priority:** HIGH

### Q-6: Should unauthenticated group-text responses ever suppress a local pending response?

- **Category:** risk
- **Why it matters:** MeshCore group text names are spoofable. Suppressing based on text alone can silence the real bot or block emergency forwarding.
- **Default recommendation:** For v1, allow weak group-text suppression only for low-risk normal `#bot`/`#testing` replies that match known bot labels and fingerprints; never use weak hints for `#emergency` forwarding or safety-critical behavior.
- **Source refs:** ITEM-architecture-7, ITEM-pitfalls-11, ITEM-pitfalls-8
- **Priority:** HIGH

### Q-7: What exact `#emergency` behavior should occur when the original text is too long?

- **Category:** ux
- **Why it matters:** MeshCore text/frame limits mean the required emergency header plus original text may not fit in one message. Truncation policy affects safety and airtime.
- **Default recommendation:** Use a two-stage formatter when needed: first Public packet `EMERGENCY MESSAGE FROM <user>`, second Public packet containing the original text truncated to fit with an explicit truncation marker. Do not send unlimited multipart emergency messages in v1.
- **Source refs:** ITEM-architecture-4, ITEM-pitfalls-5, ITEM-pitfalls-8, ITEM-prior-art-2
- **Priority:** HIGH

### Q-8: What channel names and matching rules should be canonical?

- **Category:** technical
- **Why it matters:** Channel indexes are local and can change; substring matching could make the bot respond on unintended channels such as `#botnet`.
- **Default recommendation:** Use exact normalized channel names `Public`, `#bot`, `#testing`, and `#emergency`; resolve to channel details at boot and after channel changes; refuse channel operation if names are missing, duplicated, or ambiguous.
- **Source refs:** ITEM-architecture-3, ITEM-pitfalls-7
- **Priority:** HIGH

### Q-9: What persistent configuration surface is acceptable for v1?

- **Category:** technical
- **Why it matters:** Appending to `NodePrefs` can break existing binary layout, but compile-time-only config is hard to field-tune.
- **Default recommendation:** Add a separate versioned `/bot_prefs_v1` file with magic/version/length/CRC for enable flag, tier, delay/jitter, cooldowns, allowed channels, emergency mapping, known bots, command bitset, and small stats reset marker. Do not insert fields into existing `NodePrefs` offsets.
- **Source refs:** ITEM-architecture-9, ITEM-pitfalls-15, ITEM-pitfalls-16, ITEM-prior-art-10
- **Priority:** HIGH

### Q-10: What are the hard size budgets for accepting v1?

- **Category:** constraints
- **Why it matters:** Research estimates are feasible but unmeasured locally because PlatformIO was unavailable. The plan needs explicit thresholds to prevent feature creep.
- **Default recommendation:** Treat RAK4631 BLE as the gate: aim for <=25 KB incremental app flash and <=2 KB static RAM initially; hard-review anything above 40-60 KB flash or 4 KB RAM. Persistent bot config should stay under 4 KB. Capture exact deltas in CI.
- **Source refs:** ITEM-stack-10, ITEM-stack-11, ITEM-stack-12, ITEM-stack-13, ITEM-architecture-10, ITEM-architecture-11, ITEM-pitfalls-1
- **Priority:** HIGH

### Q-11: Should private key import/export be disabled in production bot builds?

- **Category:** risk
- **Why it matters:** Known-bot trust makes bot identities valuable. If private keys remain exportable/importable, a compromised host or BLE session can clone a trusted bot identity.
- **Default recommendation:** Disable private key export/import for production bot firmware. Use a separate provisioning build or a physical-button-gated temporary window if identity migration is required.
- **Source refs:** ITEM-pitfalls-18
- **Priority:** HIGH

### Q-12: What host/app protocol extensions are needed in v1, if any?

- **Category:** technical
- **Why it matters:** New companion protocol frames can break apps if they collide with existing commands or appear unexpectedly. But field config and stats need some access path.
- **Default recommendation:** Keep v1 usable with rescue CLI plus minimal queried stats/config frames only. Gate all new protocol commands by firmware capability/version and never change existing frame semantics.
- **Source refs:** ITEM-architecture-9, ITEM-architecture-13, ITEM-pitfalls-13
- **Priority:** MEDIUM

### Q-13: Which hardware targets define “done” for the first release candidate?

- **Category:** constraints
- **Why it matters:** The project wants all companion device types eventually, but first release quality needs a focused representative set.
- **Default recommendation:** Inner-loop builds: Heltec v3 USB, Heltec v3 BLE, RAK4631 USB, RAK4631 BLE. First hardware smoke: the plugged-in Heltec v3. Release gate: RAK4631 BLE builds and passes size/stress tests. Expand all companion builds after representative success.
- **Source refs:** PROJECT.md, ITEM-stack-8, ITEM-stack-9, ITEM-stack-10, ITEM-pitfalls-17
- **Priority:** MEDIUM

### Q-14: How much telemetry should be exposed to users versus debug tooling?

- **Category:** ux
- **Why it matters:** Operators need visibility into suppression and hidden nodes, but normal users should not see noisy internal state over LoRa.
- **Default recommendation:** Expose compact local stats through CLI/protocol (`observed`, `sent`, `suppressed`, `duplicates`, `emergency forwards`, queue failures, storage used/total). Do not post coordinator diagnostics into normal channels except explicit debug commands.
- **Source refs:** ITEM-architecture-13, ITEM-pitfalls-12, ITEM-pitfalls-4
- **Priority:** MEDIUM

### Q-15: Should future explicit claim frames be planned in the protocol now, even if disabled in v1?

- **Category:** technical
- **Why it matters:** Planning the fingerprint/state model now can avoid repainting the architecture later, but implementing claims immediately adds airtime and spoofing risks.
- **Default recommendation:** Reserve state-machine hooks and stats for `known_claim_seen`, but do not transmit claim frames in v1. If later added, claims must be compact, versioned, length-checked, fingerprint-bound, scoped, and authenticated by known bot identity.
- **Source refs:** ITEM-architecture-6, ITEM-architecture-7, ITEM-pitfalls-9, ITEM-pitfalls-11
- **Priority:** MEDIUM

## Technical Direction

### Stack
- **Firmware/runtime:** C++ in upstream MeshCore companion firmware, Arduino framework, PlatformIO, upstream dependency pins preserved. Source refs: ITEM-stack-1, ITEM-stack-4.
- **Repository shape:** wrapper repo with pinned `upstream/MeshCore` submodule, Colorado overlay files, and deterministic patch queue applied into a build worktree. Source refs: ITEM-stack-2, ITEM-architecture-14.
- **Target baseline:** current MeshCore main pinned intentionally, not stale local-only assumptions. Source refs: ITEM-stack-3.
- **Build matrix:** use upstream `build.sh`; inner loop builds `Heltec_v3_companion_radio_usb`, `Heltec_v3_companion_radio_ble`, `RAK_4631_companion_radio_usb`, and `RAK_4631_companion_radio_ble`; later run all companion builds. Source refs: ITEM-stack-8.
- **Firmware coding style:** fixed-size arrays/ring buffers, compile-time flags, no heap allocation in receive/parse/schedule/send paths, bounded `char[]` parsing, terse single-packet responses. Source refs: ITEM-stack-5, ITEM-pitfalls-3, ITEM-pitfalls-5, ITEM-pitfalls-6.
- **Host-side tooling:** Python only for tests, fixtures, worktree/patch tooling, simulations, and size-report parsing. No Python runtime dependency in deployed firmware. Source refs: ITEM-stack-14.

### Architecture
The bot should be a `FirmwareBot`/`BotRuntime` owned by companion `MyMesh`. `MyMesh` adapts MeshCore callbacks into compact `BotMessage` inputs, invokes policy/classification/execution/coordination, and sends approved `BotResponse` outputs through existing MeshCore send APIs. Core modules should be:

1. `BotPolicy`: enabled state, exact channel policy, known bots, command bitset, emergency rules, rate limits.
2. `CommandClassifier`: bounded normalization, prefix/verb matching, semantic fingerprint inputs.
3. `CommandExecutor`: fixed C++ handlers for v1 commands with short response buffers.
4. `ResponseCoordinator`: passive listen-before-answer FSM and fixed pending/recent tables.
5. `EmergencyForwarder`: dedicated `#emergency` to Public path with dedup, loop prevention, and bounded formatting.
6. `BotPrefs` / `KnownBotRegistry` / `BotStats`: small persisted config, explicit trusted keys, and metrics.

Recommended v1 data flow:

1. MeshCore receives/decrypts a DM or group message and invokes `MyMesh` callbacks.
2. `BotPolicy` rejects all normal Public traffic and only allows DMs, `#bot`, and `#testing`; `#emergency` is diverted to `EmergencyForwarder`.
3. `CommandClassifier` parses `(const char*, length)`, normalizes only safe ASCII command verbs, and derives a 64-bit semantic fingerprint from stable application fields.
4. `CommandExecutor` creates a short candidate response without allocating a MeshCore packet.
5. `ResponseCoordinator` schedules the response in a fixed table using role/delay/jitter/score. It does not allocate a packet until the timer wins.
6. If a trusted known bot response for the same fingerprint is heard first, the pending response is suppressed and stats are updated.
7. When due, `MyMesh` sends a single concise DM/group message via existing APIs.
8. Runtime fingerprints, suppression windows, emergency recent cache, and counters remain bounded and mostly volatile.

### Storage / Flash / RAM Estimates

| Target | Hardware/app region facts | Current signal from research | Recommended v1 bot budget | Implication |
|---|---|---:|---:|---|
| Heltec WiFi LoRa 32 V3 | ESP32-S3, 8 MB flash, 512 KB SRAM, no PSRAM; PlatformIO default 8 MB partition gives ~3,342,336 bytes per OTA app slot and ~1,572,864 bytes SPIFFS | v1.15 release app signals around 615 KB USB / ~1.2 MB BLE non-merged; Heltec has wide flash margin | 10-25 KB flash target, hard review above 40-60 KB; 0.5-4 KB RAM; 0-4 KB persistent bot prefs | Comfortable first smoke target. Do not optimize for Heltec alone because it hides RAK constraints. Source refs: ITEM-stack-9, ITEM-stack-11, ITEM-stack-12, ITEM-pitfalls-1 |
| RAK4631 | nRF52840, 1 MB flash, 256 KB RAM; MeshCore companion app capped at 712,704 bytes and RAM region ~237,568 bytes after SoftDevice/linker reservations | v1.15 UF2 assets ~933-949 KB container / ~467-475 KB zip proxy; exact ELF headroom requires local PlatformIO build | Aim <=25 KB additional flash and <=2 KB static RAM initially; design under 4 KB runtime RAM and under 4 KB filesystem storage; hard review above 40-60 KB flash or 4-8 KB RAM | Release gate and limiting target. No large help text, APIs, databases, logs, dynamic plugins, or persistent histories. Source refs: ITEM-stack-10, ITEM-stack-11, ITEM-stack-12, ITEM-architecture-10, ITEM-architecture-11, ITEM-pitfalls-1 |

Concrete compact state target: 8-12 pending coordinator entries, 16-24 recent fingerprint entries, 16 known bot keys, counters under 256 B, and one shared 160-byte response scratch buffer. Persistent storage should contain only versioned config and known bot keys; suppression history and emergency caches should be RAM-only.

### Prior Art to Leverage
- **Upstream MeshCore companion firmware:** patch base, transport, storage, send APIs, callbacks, and multi-target build system. Source refs: ITEM-prior-art-1.
- **MeshCore companion protocol:** short-message and storage-report constraints; useful for future stats/config access, not a runtime bot API. Source refs: ITEM-prior-art-2.
- **agessaman meshcore-bot:** command names, channel policy concepts, cooldown/rate-limit behavior, and response wording. Do not port HTTP/API/SQLite/plugins. Source refs: ITEM-prior-art-3.
- **Colorado Mesh community/VPS bot:** observable coordination semantics: DMs bypass, channel responses coordinated, local scoring, fallback delays. Preserve concepts without preserving host infrastructure. Source refs: ITEM-prior-art-4.
- **Meshtastic firmware modules:** proof that small fixed embedded modules are the right pattern; use bounded config, static messages, rate limits, and compile-time flags. Source refs: ITEM-prior-art-5.
- **disaster.radio console:** practical model for bounded firmware command parsing. Source refs: ITEM-prior-art-6.
- **Cyclenerd meshcore-bot:** conservative scope lesson: DM/private/opt-in channels are safer than Public. Source refs: ITEM-prior-art-8.
- **LoRa APRS and MESH-API/MESH-AI:** conceptual lessons for duplicate buffers, origin markers, and loop prevention, but do not reuse GPL/heavy host code in firmware. Source refs: ITEM-prior-art-7, ITEM-prior-art-9.

## Detailed Planning Implications

1. **Repository initialization:** create wrapper repo structure, add/pin MeshCore submodule, define overlay/patch application script, and document exact upstream SHA. Verify upstream builds before bot patches. Source refs: ITEM-stack-2, ITEM-stack-3, ITEM-architecture-14.
2. **Build/tooling gate:** add `scripts/build-representative.sh` to apply patches, build Heltec v3 USB/BLE and RAK4631 USB/BLE, parse PlatformIO size output, and archive artifacts. Source refs: ITEM-stack-8, ITEM-stack-13, ITEM-pitfalls-1.
3. **First smoke target:** after plan approval, use the plugged-in Heltec v3 for first firmware smoke because it is locally available and has generous flash. Keep flashing steps explicit and reversible. Source refs: PROJECT.md, ITEM-stack-9.
4. **RAK4631 release gate:** do not declare firmware feasibility complete until RAK4631 BLE builds with measured app/RAM headroom and passes stress tests. Source refs: ITEM-stack-10, ITEM-pitfalls-1.
5. **Patch boundaries:** keep patches small: bot source files, `MyMesh` hook/adapters, `BotPrefs`/DataStore integration, optional CLI/protocol constants, and CI/build flags. Source refs: ITEM-architecture-14.
6. **Parser first:** implement bounded `(ptr,len)` parser/normalizer with unit/fuzz cases for max-length messages, missing colon, repeated colon, UTF-8, NUL, controls, empty commands, and exact channel names. Source refs: ITEM-pitfalls-6.
7. **Policy before execution:** implement exact channel resolution and disabled/conservative defaults before adding command handlers so Public noise is structurally impossible. Source refs: ITEM-architecture-3, ITEM-pitfalls-7.
8. **Emergency path as its own step:** implement `EmergencyForwarder` separately from normal command execution, with dedup TTL, loop prevention, formatting tests, and rate-limit counters. Source refs: ITEM-architecture-4, ITEM-pitfalls-8.
9. **Command MVP:** add short deterministic command handlers and golden behavior fixtures from `meshcore-bot`; ensure every output fits MeshCore text constraints with node-name prefix. Source refs: ITEM-stack-6, ITEM-pitfalls-5, ITEM-prior-art-3.
10. **Coordinator FSM:** implement fixed-size passive suppression after basic command handling works; defer any claim frames. Verify all state transitions, TTL expiry, queue-full, and cancel-on-known-response paths. Source refs: ITEM-architecture-6, ITEM-pitfalls-4, ITEM-pitfalls-9.
11. **Known bot registry:** implement full-key storage, labels, bounded capacity, and trust decisions before allowing suppression across nodes. Source refs: ITEM-architecture-7, ITEM-pitfalls-11.
12. **Persistence isolation:** add versioned `BotPrefs`; do not mutate existing `NodePrefs` binary layout. Default disabled/safe on missing, corrupt, or incompatible prefs. Source refs: ITEM-architecture-9, ITEM-pitfalls-16.
13. **Instrumentation:** add `BotStats` from day one, including observed, eligible, scheduled, sent, suppressed, weak-suppressed, emergency forwarded, duplicate emergency, queue/pool failures, and storage totals. Source refs: ITEM-architecture-13.
14. **No hot-path heap:** add review/static checks forbidding `String`, `malloc/new`, `std::vector`, maps, JSON builders, and variable heap strings in bot receive/parse/schedule/send code. Source refs: ITEM-pitfalls-3.
15. **Protocol compatibility:** keep new companion protocol frames optional/query-only in v1; no unsolicited new frames to old apps unless version/capability-gated. Source refs: ITEM-pitfalls-13.
16. **Security/release flags:** decide production key export/import policy before distributing trusted bot firmware. Source refs: ITEM-pitfalls-18.

## Risk Register

| Priority | Risk | Impact | Mitigation | Source refs |
|---|---|---|---|---|
| CRITICAL | RAK4631 BLE exceeds flash/RAM | Build failure or unstable constrained target | Treat RAK4631 BLE as gate; CI size deltas; cap v1 at compact command set | ITEM-pitfalls-1, ITEM-stack-10, ITEM-stack-12 |
| CRITICAL | Python bot feature creep | Firmware bloat, off-grid failure, unmaintainable partial clone | Firmware MVP only; Python as behavior oracle; defer API/web/SQLite features | ITEM-pitfalls-2, ITEM-stack-6, ITEM-prior-art-3 |
| CRITICAL | Heap fragmentation | Long-running crashes, packet allocation failures, BLE instability | Fixed arrays/rings, no hot-path heap, bounded buffers, no `String`/JSON | ITEM-pitfalls-3 |
| CRITICAL | Packet pool/offline queue exhaustion | Dropped user traffic and failed sends | Keep pending responses separate from packets; allocate only when timer wins; small fixed pending table | ITEM-pitfalls-4 |
| CRITICAL | Unsafe parsing | Buffer errors, command misfires, corrupted output | `(ptr,len)` parser, explicit NUL bounds, `snprintf`, fuzz/unit tests | ITEM-pitfalls-6 |
| CRITICAL | Wrong channel routing | Bot answers on Public or misses emergency policy | Exact channel name/hash resolution, diagnostics, refuse ambiguous config | ITEM-pitfalls-7, ITEM-architecture-3 |
| CRITICAL | Emergency forwarding loops/amplification | Public flood during emergencies | Separate emergency path, idempotency TTL, no recursive forwarding, rate limits | ITEM-pitfalls-8, ITEM-architecture-4 |
| CRITICAL | Spoofed suppression | Malicious/mistaken node silences real bots | Known bot public keys; weak hints only for low-risk channels; no weak emergency suppression | ITEM-pitfalls-11, ITEM-architecture-7 |
| CRITICAL | Blocking companion loop | BLE/serial/radio/display stalls | Timestamp FSM in `loop()`, no `delay()`, no busy waits | ITEM-pitfalls-12, ITEM-architecture-6 |
| CRITICAL | NodePrefs layout breakage | Existing settings corrupted | Separate versioned `BotPrefs`; default safe on invalid prefs | ITEM-pitfalls-16, ITEM-architecture-9 |
| CRITICAL | Target-specific APIs break portability | Works on Heltec only; fails nRF/RP2040/STM32 | Platform-neutral bot core behind `MyMesh`/DataStore abstractions; representative build matrix | ITEM-pitfalls-17 |
| CRITICAL | Production key export/import remains enabled | Trusted bot identity can be cloned | Disable export/import in production bot firmware or gate provisioning | ITEM-pitfalls-18 |
| MODERATE | Text/frame limits truncate replies | Lost context and high airtime from multipart chatter | Single-packet command responses; emergency two-message max when needed | ITEM-pitfalls-5, ITEM-prior-art-2 |
| MODERATE | Passive suppression cannot solve hidden nodes | Duplicate replies still occur | Set expectations, measure, deterministic jitter, optional authenticated claims later | ITEM-pitfalls-9 |
| MODERATE | Bad semantic fingerprint | False suppression or missed duplicates | Fingerprint stable app fields, not raw packet hash or text alone; tests | ITEM-pitfalls-10, ITEM-architecture-5 |
| MODERATE | RSSI/SNR overused as “closest” signal | Wrong bot wins | Use SNR/RSSI only as weak zero-hop/direct hint; tier/health/path/tiebreaker dominate | ITEM-pitfalls-14 |
| MODERATE | Flash wear/storage fill | DataStore corruption or lost contacts/channels | Persist only config; runtime caches in RAM; lazy coarse counters only | ITEM-pitfalls-15, ITEM-prior-art-10 |

## Conflicts & Tradeoffs

1. **Heltec-first smoke vs RAK-first design.** The plugged-in Heltec v3 is the right first hardware smoke target, but its 8 MB flash can hide the real release constraint. Resolution: smoke on Heltec, budget and release-gate on RAK4631 BLE. Source refs: PROJECT.md, ITEM-stack-9, ITEM-stack-10, ITEM-pitfalls-1.
2. **Full firmware bot vs Python feature parity.** Firmware-only is the project goal, but Python bot parity is not feasible on RAK4631-class MCUs. Resolution: firmware-native MVP with deterministic local commands; host/Python remains only a behavior reference and tooling source. Source refs: ITEM-stack-6, ITEM-pitfalls-2, ITEM-prior-art-3.
3. **Passive suppression vs exactly-once responses.** Passive suppression saves airtime and avoids new frames but cannot guarantee exactly-once under hidden nodes. Resolution: passive first with metrics; future authenticated claims only if field data demands. Source refs: ITEM-architecture-6, ITEM-pitfalls-9, codex-analysis.
4. **Known identity security vs group-channel realities.** Strong suppression wants full public-key identity, but plain group text carries spoofable sender names. Resolution: full-key registry for authoritative trust; weak hints only for low-risk channels; no weak emergency suppression. Source refs: ITEM-architecture-7, ITEM-pitfalls-11.
5. **Config flexibility vs storage/migration safety.** Field operators need tuning, but `NodePrefs` binary layout is fragile and storage is finite. Resolution: separate compact versioned `BotPrefs`, CLI rescue, optional protocol config later. Source refs: ITEM-architecture-9, ITEM-pitfalls-16.
6. **Emergency reliability vs airtime conservation.** Public emergency forwarding may duplicate under hidden nodes, but suppressing too aggressively can drop critical alerts. Resolution: short idempotent forwards, longer emergency dedup TTL, no recursive forwarding, tolerate identical duplicates over missed emergencies. Source refs: ITEM-architecture-4, ITEM-pitfalls-8.
7. **Topology tier policy vs actual RF quality.** LOCAL/SUBURBAN/HILLTOP/MOBILE is useful operator intent but not true proximity. Resolution: role is a delay/score bias, not the sole winner; combine with directness, path freshness, queue health, and deterministic tiebreaking. Source refs: ITEM-architecture-12, ITEM-pitfalls-14.
8. **Codex supplemental analysis vs refreshed research.** Codex recommended useful general cautions, but it was coordinator-first/stale relative to the firmware-only pivot. Resolution: use Codex only where it aligns with refreshed Claude research on passive soft coordination, hidden nodes, airtime, and embedded constraints; ignore host/coordinator-first conclusions. Source refs: codex-analysis, ITEM-stack-6, ITEM-architecture-1.

## Confidence Assessment

| Dimension | Status | Confidence | Notes |
|-----------|--------|------------|-------|
| stack | complete | HIGH | Clear firmware stack: upstream MeshCore Arduino C++/PlatformIO, submodule/patch workflow, representative builds, storage budget estimates. |
| pitfalls | complete | HIGH | Strong local-code-backed risks for RAK4631 limits, heap, queues, parsing, channel routing, emergency loops, protocol compatibility, and keys. |
| architecture | complete | HIGH | Clear companion-layer module architecture, policy/classifier/executor/coordinator split, BotPrefs, known bot registry, stats, passive FSM. |
| prior-art | complete | HIGH | Strong prior art from upstream MeshCore, Python/community bots, Meshtastic modules, disaster.radio, and host-bot examples. |
| codex-analysis | complete | MEDIUM | Optional supplemental research only; stale coordinator-first framing must not override refreshed firmware-only research. Useful only for general embedded cautions. |
