# Forge Implementation Plan

## Overview
Build a Colorado Mesh firmware-only bot as a wrapper repository around upstream `meshcore-dev/MeshCore`, pinned as a submodule and modified through a deterministic patch queue. The firmware bot will live inside MeshCore companion firmware, handle lightweight fun + utility commands directly on-device, keep normal bot traffic off Public, allow DMs plus `#bot`/`#testing`, forward `#emergency` to Public, coordinate/suppress normal duplicate bot replies with passive listen-before-answer, expose runtime CLI/config controls, harden production key handling, measure Heltec v3 and RAK4631 build sizes, and flash the plugged-in Heltec v3 after verification passes.

## Technical Decisions

- **Firmware-first architecture:** Implement the bot as embedded C++ in MeshCore companion firmware; use `meshcore-bot` and Colorado community bot behavior as references only. Trace: ITEM-stack-1, ITEM-stack-5, ITEM-stack-6, ITEM-architecture-1, ITEM-prior-art-3.
- **Repository shape:** Keep upstream MeshCore as a pinned submodule under `vendor/MeshCore`; keep Colorado modifications as `colorado/` overlay files plus `patches/meshcore/*.patch` and scripts that apply/export patches deterministically. Trace: ITEM-stack-2, ITEM-stack-3, ITEM-architecture-14.
- **Runtime placement:** Integrate at `examples/companion_radio/MyMesh` callback/loop level and do not touch MeshCore routing, dispatcher, ACKs, packet duplicate tables, or lower-layer `txdelay`/`rxdelay` behavior. Trace: ITEM-architecture-1, ITEM-architecture-12, ITEM-pitfalls-12.
- **Traffic policy:** Normal bot responses are allowed only in DMs, `#bot`, and `#testing`; Public channel command traffic is ignored silently; `#emergency` is a special route-to-Public path. Trace: ITEM-architecture-3, ITEM-prior-art-12, PROJECT.md.
- **Emergency policy:** `#emergency` messages must be posted to Public as `EMERGENCY MESSAGE FROM <user>` followed by the original text, may be multipart, and must never be suppressed; loop prevention and rate limits bound amplification. Trace: ITEM-architecture-4, ITEM-pitfalls-8, PROJECT.md.
- **Command scope:** Implement more bot parity than the compact minimum, but only firmware-feasible fun + utility commands: no HTTP/TLS/API feeds, SQLite/history DB, Discord/web viewer, dynamic plugins, or large text catalogs until RAK4631 size evidence proves headroom. Trace: ITEM-stack-6, ITEM-prior-art-3, ITEM-prior-art-11, ITEM-pitfalls-2.
- **Coordinator:** Use passive listen-before-answer for normal traffic with semantic fingerprints, known-bot trust, bounded pending/recent tables, and no explicit on-air claim frames in v1. Trace: ITEM-architecture-5, ITEM-architecture-6, ITEM-architecture-7, ITEM-pitfalls-9, ITEM-pitfalls-11.
- **Runtime config:** Add compact bot runtime CLI/config controls in Phase 1, persisted in a separate versioned bot prefs file rather than changing `NodePrefs` binary layout. Trace: ITEM-architecture-9, ITEM-pitfalls-16.
- **Resource budget:** RAK4631 USB/BLE is the hard release gate. Aim for <=25 KB incremental app flash, <=2 KB static RAM initially, hard-review above 40-60 KB flash or 4-8 KB RAM, and keep persistent bot config under 4 KB. Trace: ITEM-stack-10, ITEM-stack-11, ITEM-stack-12, ITEM-pitfalls-1.
- **Build and hardware:** CI and local scripts must build Heltec v3 USB/BLE and RAK4631 USB/BLE with size reports. The plugged-in Heltec v3 is the first hardware smoke target after builds and reviews pass. Trace: ITEM-stack-8, ITEM-stack-13, PROJECT.md.
- **Production key hardening:** Production bot firmware disables upstream private key import/export build flags by default; a separate provisioning/dev path can be added later if needed. Trace: ITEM-pitfalls-18, PROJECT.md.

## Implementation Steps

### Step 1: Bootstrap wrapper repository, upstream submodule, and patch workflow

**Goal:** Create a reproducible Colorado Mesh firmware wrapper that pins upstream MeshCore and can apply/export Colorado bot patches without relying on untracked submodule edits.

**Why now:** All later source changes must have a stable upstream base and a clean path for CI and review.

**Dependencies:** Empty local repo initialized by Forge; git available locally; upstream MeshCore reachable; user selected submodule strategy.

**Files:**
- `.gitmodules`
- `vendor/MeshCore` submodule pointer
- `scripts/apply-patches.sh`
- `scripts/export-patches.sh`
- `scripts/meshcore-env.sh`
- `patches/meshcore/.gitkeep`
- `colorado/README.md`
- `README.md` or `docs/development.md` only if needed for operator build instructions

**Existing code to inspect first:**
- `/Users/cjvana/Documents/GitHub/MeshCore/build.sh`
- `/Users/cjvana/Documents/GitHub/MeshCore/platformio.ini`
- `/Users/cjvana/Documents/GitHub/MeshCore/variants/heltec_v3/platformio.ini`
- `/Users/cjvana/Documents/GitHub/MeshCore/variants/rak4631/platformio.ini`

**Implementation plan:**
1. Add `vendor/MeshCore` as a submodule from `https://github.com/meshcore-dev/MeshCore.git` and pin the exact upstream commit used for this run.
2. Create `patches/meshcore/` as the source-of-truth patch series directory, initially empty except for `.gitkeep`.
3. Write `scripts/meshcore-env.sh` to define `MESHCORE_DIR`, representative environment names, output paths, and common helper variables without modifying shell global state.
4. Write `scripts/apply-patches.sh` to verify the submodule exists, fail if it has unexpected uncommitted changes, apply `patches/meshcore/*.patch` in sorted order when present, and print the pinned upstream SHA.
5. Write `scripts/export-patches.sh` to export any committed Colorado changes from the submodule or a temporary working branch back to `patches/meshcore/` in deterministic order.
6. Add a small development note explaining that implementation edits happen in `vendor/MeshCore` for buildability, then patches are exported before review/commit.
7. Verify the script behavior with an empty patch queue and no source edits.

**Contracts and interfaces:**
- `scripts/apply-patches.sh` exits non-zero if patches fail or the submodule is missing.
- `scripts/export-patches.sh` never silently overwrites patches without regenerating the full ordered patch queue.
- Representative env names are: `Heltec_v3_companion_radio_usb`, `Heltec_v3_companion_radio_ble`, `RAK_4631_companion_radio_usb`, `RAK_4631_companion_radio_ble`.

**State/data changes:** Git submodule metadata and wrapper scripts only; no firmware behavior changes.

**Edge cases:** Missing submodule init, stale local submodule changes, empty patch directory, upstream branch default not named `main`, patch filenames with spaces.

**Acceptance criteria:**
- `git submodule status` shows a pinned MeshCore commit.
- `scripts/apply-patches.sh` succeeds with no patches.
- Wrapper scripts do not modify firmware files when the patch queue is empty.

**Verification commands:**
- `git submodule status`
- `bash scripts/apply-patches.sh`
- `bash -n scripts/apply-patches.sh scripts/export-patches.sh scripts/meshcore-env.sh`

**Manual validation:** Inspect the printed upstream SHA and confirm it matches the submodule pointer in git status.

**Risks:**
- Patch workflow drift can make builds non-reproducible. Mitigated by sorted patch application and failing on dirty submodule state. Trace: ITEM-stack-2, ITEM-architecture-14.
- A stale local MeshCore checkout may differ from upstream; the submodule pin becomes the authoritative source. Trace: ITEM-stack-3.

**Out of scope for this step:** Bot code, CI, PlatformIO installation, firmware build fixes.

### Step 2: Add representative build and size-report tooling

**Goal:** Create local/CI build tooling that applies patches, builds the four representative companion environments, captures artifact sizes, and reports flash/RAM usage so storage estimates can be replaced by measurements.

**Why now:** The user explicitly asked how much storage the bot firmware will take; build reporting must exist before feature growth.

**Dependencies:** Step 1 scripts; upstream MeshCore `build.sh`; PlatformIO may need installation in CI/local environment.

**Files:**
- `scripts/build-representative.sh`
- `scripts/parse-size-report.py`
- `.github/workflows/firmware-build.yml`
- `.gitignore` entries for build outputs if needed
- `colorado/size-baseline/README.md` or generated JSON baseline location

**Existing code to inspect first:**
- `vendor/MeshCore/build.sh`
- `vendor/MeshCore/.github/workflows/build-companion-firmwares.yml`
- `vendor/MeshCore/.github/actions/setup-build-environment/action.yml`
- `vendor/MeshCore/variants/heltec_v3/platformio.ini`
- `vendor/MeshCore/variants/rak4631/platformio.ini`

**Implementation plan:**
1. Write `scripts/build-representative.sh` to run `scripts/apply-patches.sh`, then invoke upstream `build.sh build-firmware` for each representative env from `vendor/MeshCore`.
2. Capture stdout/stderr per environment into `out/size/<env>.log` and copy generated `.bin`/`.uf2` artifacts into wrapper-level `out/firmware/` when present.
3. Write `scripts/parse-size-report.py` to parse PlatformIO RAM/flash usage lines and artifact byte sizes into a JSON summary.
4. Add a baseline mode that records current no-bot submodule size outputs when builds first succeed, and a compare mode that reports deltas after bot patches exist.
5. Add a GitHub Actions workflow that checks out submodules, installs PlatformIO using upstream setup guidance, runs the representative build script, uploads logs/artifacts, and prints the JSON summary.
6. Make the initial workflow warn/report rather than enforce deltas until a measured baseline exists.
7. Document the exact local command to run before hardware flashing.

**Contracts and interfaces:**
- `scripts/build-representative.sh [--baseline|--compare]` builds all four representative envs.
- `scripts/parse-size-report.py <log-dir> <artifact-dir>` emits deterministic JSON with `env`, `ram_used`, `ram_total`, `flash_used`, `flash_total`, `artifact_bytes`, and optional delta fields.
- CI uses the same scripts as local development.

**State/data changes:** Build artifacts under ignored `out/`; optional committed baseline JSON only after first successful measured baseline.

**Edge cases:** PlatformIO missing locally, upstream `build.sh` output format changes, no artifact produced for failed build, UF2 size not equal to raw app flash, logs containing ANSI color codes.

**Acceptance criteria:**
- CI workflow syntax is valid.
- Local scripts fail clearly if PlatformIO is unavailable.
- Size parser handles missing metrics without pretending the build passed.
- Representative env list matches the plan and research.

**Verification commands:**
- `bash -n scripts/build-representative.sh`
- `python3 -m py_compile scripts/parse-size-report.py`
- `bash scripts/build-representative.sh --help`
- If PlatformIO is installed by this step or already available: `bash scripts/build-representative.sh --baseline`

**Manual validation:** Review `out/size/*.log` and JSON summary after the first successful build.

**Risks:**
- RAK4631 BLE is the limiting target and may fail before bot code if local toolchain differs. Mitigate by using upstream CI setup and preserving dependency pins. Trace: ITEM-stack-4, ITEM-stack-10, ITEM-pitfalls-1.
- GitHub asset/UF2 sizes are not exact app flash, so parser must prefer PlatformIO section usage over artifact byte size. Trace: ITEM-architecture-11, ITEM-prior-art-11.

**Out of scope for this step:** Enforcing size limits, bot behavior, flashing hardware.

### Step 3: Introduce the firmware bot core module and host-side unit harness

**Goal:** Add platform-neutral firmware bot C++ source files with bounded data structures plus a host-side unit harness for parser, policy, fingerprints, and response formatting before wiring into `MyMesh`.

**Why now:** Parser and policy bugs are high-risk; testing core logic outside hardware shortens iteration.

**Dependencies:** Step 1 patch workflow; Step 2 build script for later firmware builds.

**Files:**
- `vendor/MeshCore/examples/companion_radio/FirmwareBot.h`
- `vendor/MeshCore/examples/companion_radio/FirmwareBot.cpp`
- `vendor/MeshCore/examples/companion_radio/BotPolicy.h`
- `vendor/MeshCore/examples/companion_radio/BotPolicy.cpp`
- `vendor/MeshCore/examples/companion_radio/BotTypes.h`
- `vendor/MeshCore/examples/companion_radio/BotStats.h`
- `tests/firmware_bot/` host harness files
- `patches/meshcore/0001-companion-firmware-bot-core.patch` after export

**Existing code to inspect first:**
- `vendor/MeshCore/src/MeshCore.h` for message size constants
- `vendor/MeshCore/src/helpers/BaseChatMesh.h`
- `vendor/MeshCore/src/helpers/BaseChatMesh.cpp`
- `vendor/MeshCore/examples/companion_radio/MyMesh.h`
- `vendor/MeshCore/examples/companion_radio/MyMesh.cpp`
- `vendor/MeshCore/README.md` for allocation guidance

**Implementation plan:**
1. Define `BotMessage`, `BotResponse`, `BotChannelKind`, `BotCommand`, `BotFingerprint`, `BotPrefs`, and `BotStats` structs with fixed-size fields and explicit size comments verified by static assertions.
2. Implement bounded input normalization over `(const char*, size_t)` with no reliance on untrusted `strlen` and no hot-path heap allocation.
3. Implement exact channel policy helpers for DM, `#bot`, `#testing`, `#emergency`, and Public ignore behavior.
4. Implement a 64-bit semantic fingerprint helper over stable application fields using an upstream-available hash primitive or a small deterministic local hash if SHA-256 is not already convenient in this layer.
5. Implement response buffer helpers that write into caller-provided fixed buffers and return truncation status.
6. Add a host-side C++/Python-driven unit harness that compiles the bot core with stubs and tests max-length messages, UTF-8/control chars, empty commands, colons, Public ignore, allowed channels, and fingerprint stability.
7. Export the resulting submodule changes into the first patch file.

**Contracts and interfaces:**
- Bot core exposes no Arduino `String`, `malloc`, `new`, `std::vector`, `std::map`, or JSON dependency in receive/parse/schedule paths.
- `BotPolicy::classifyChannel()` returns explicit `allow_normal`, `emergency_forward`, or `ignore` decisions.
- All output APIs require destination buffer and length.

**State/data changes:** No persistent device state yet; only in-memory structs and tests.

**Edge cases:** Embedded NULs, non-ASCII characters, messages with no command prefix, command aliases with trailing punctuation, channel names with missing leading `#`, channel names like `#botnet`, long sender names.

**Acceptance criteria:**
- Host tests pass for parser/policy/fingerprint behavior.
- Bot files compile in host harness without Arduino-specific includes except where guarded.
- Static assertions keep key structs within the planned RAM budget.
- Patch exports cleanly and reapplies from a clean submodule.

**Verification commands:**
- `bash scripts/apply-patches.sh`
- Host test command selected during implementation, e.g. `python3 tests/firmware_bot/run_tests.py`
- `grep -R "String\|malloc\|new \|std::vector\|std::map" vendor/MeshCore/examples/companion_radio/FirmwareBot* vendor/MeshCore/examples/companion_radio/Bot*` should only show allowed false positives if any.
- `bash scripts/export-patches.sh`
- `bash scripts/apply-patches.sh`

**Manual validation:** Read generated patch and confirm it only adds bot core/test support without touching routing code.

**Risks:**
- Unsafe parsing can cause buffer errors or command misfires. Mitigated by `(ptr,len)` tests and bounded output. Trace: ITEM-pitfalls-6.
- Heap fragmentation from convenience APIs can destabilize long-running nodes. Mitigated by grep/review and fixed buffers. Trace: ITEM-pitfalls-3.

**Out of scope for this step:** `MyMesh` integration, actual command handlers beyond parser stubs, persistence, emergency sending.

### Step 4: Wire BotRuntime into MyMesh callbacks and enforce channel policy

**Goal:** Instantiate the bot runtime inside companion `MyMesh`, feed it DMs/channel messages, tick it from the main loop, and enforce silent Public ignore plus allowed DM/`#bot`/`#testing` routing without sending real bot responses yet.

**Why now:** Policy must be structurally correct before command execution or emergency forwarding can create on-air traffic.

**Dependencies:** Step 3 bot core.

**Files:**
- `vendor/MeshCore/examples/companion_radio/MyMesh.h`
- `vendor/MeshCore/examples/companion_radio/MyMesh.cpp`
- `vendor/MeshCore/examples/companion_radio/FirmwareBot.*`
- `patches/meshcore/0002-wire-firmware-bot-runtime.patch`
- Host tests updated for adapter behavior if feasible

**Existing code to inspect first:**
- `vendor/MeshCore/examples/companion_radio/MyMesh.h:124` and nearby receive declarations
- `vendor/MeshCore/examples/companion_radio/MyMesh.cpp` `onMessageRecv`, `onChannelMessageRecv`, constructor, `loop`, `handleCmdFrame`
- `vendor/MeshCore/src/helpers/BaseChatMesh.cpp` group message parsing and send behavior
- `vendor/MeshCore/src/helpers/ChannelDetails.h`

**Implementation plan:**
1. Add `FirmwareBot` member storage to `MyMesh` behind `CMESH_BOT_ENABLED` compile flag so stock builds can compile it out if needed.
2. Initialize bot runtime with node name, RNG/time accessors, channel resolver, DataStore/storage pointers where needed, and a send adapter that is initially disabled or dry-run for normal commands.
3. In `onMessageRecv`, convert private message inputs into `BotMessage` with contact/key/timestamp metadata after existing UI/app notification behavior remains intact.
4. In `onChannelMessageRecv`, resolve channel name/index and pass only application-level channel messages into `BotPolicy`, preserving existing companion notifications.
5. Add a `MyMesh::loop()` tick call that lets the bot runtime process timers without blocking.
6. Add debug/stat counters for observed/ignored/eligible/emergency-classified messages without sending normal responses.
7. Verify Public channel normal commands are ignored silently by policy before any response path is enabled.

**Contracts and interfaces:**
- Existing companion app/BLE/serial callbacks still receive messages as before.
- Bot runtime does not allocate MeshCore packets during receive callbacks.
- Bot runtime tick is non-blocking and does not call `delay()`.

**State/data changes:** In-memory counters only; no persistent config yet unless default compile-time prefs are introduced.

**Edge cases:** Missing `#bot` or `#testing` channels, channel index changes, DMs from unknown contacts, signed message variants, channel messages with spoofed sender prefixes.

**Acceptance criteria:**
- Firmware still builds for representative envs or fails only because PlatformIO is unavailable locally.
- Host/unit tests show Public normal traffic is ignored, `#bot`/`#testing` are eligible, DMs are eligible, and `#emergency` is diverted.
- No routing/Dispatcher/Mesh core files are modified.

**Verification commands:**
- `python3 tests/firmware_bot/run_tests.py`
- `grep -R "delay(" vendor/MeshCore/examples/companion_radio/FirmwareBot* vendor/MeshCore/examples/companion_radio/Bot* vendor/MeshCore/examples/companion_radio/MyMesh.cpp`
- If PlatformIO available: `bash scripts/build-representative.sh --compare`
- `bash scripts/export-patches.sh && bash scripts/apply-patches.sh`

**Manual validation:** Inspect the `MyMesh` diff and confirm hooks are narrow and existing companion notifications remain.

**Risks:**
- Blocking the companion loop can break BLE/serial/radio responsiveness. Mitigated by FSM tick and no delays. Trace: ITEM-pitfalls-12.
- Wrong channel routing can make the bot talk on Public. Mitigated by central policy before command execution. Trace: ITEM-architecture-3, ITEM-pitfalls-7.

**Out of scope for this step:** Actual command response text, emergency Public sending, persistent runtime config, suppression across bots.

### Step 5: Add firmware command executor for fun + utility command parity

**Goal:** Implement firmware-feasible fun + utility commands directly in C++ with bounded single-packet responses and output caps.

**Why now:** With policy enforced, normal bot behavior can be added safely before duplicate coordination.

**Dependencies:** Step 4 message routing; command scope decisions.

**Files:**
- `vendor/MeshCore/examples/companion_radio/BotCommands.h`
- `vendor/MeshCore/examples/companion_radio/BotCommands.cpp`
- `vendor/MeshCore/examples/companion_radio/FirmwareBot.*`
- `tests/firmware_bot/commands.*` or equivalent fixtures
- `patches/meshcore/0003-add-firmware-bot-commands.patch`

**Existing code to inspect first:**
- `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/commands/*` for command names and style
- `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/command_manager.py`
- `vendor/MeshCore/src/helpers/BaseChatMesh.h` text constants
- `vendor/MeshCore/docs/payloads.md`

**Implementation plan:**
1. Implement a fixed command table for `ping`, `test`, `hello`, `help`, `cmd`, `about`, `dice`, `roll`, `status`, `channels`, and a minimal `path`/heard diagnostic if the needed metadata is already available in `BotMessage`.
2. Add aliases that are inexpensive and useful from existing bot behavior, avoiding any API/network/database-backed commands.
3. Keep help/cmd output compact and possibly split by category only if the user explicitly requests; default response must fit within MeshCore text constraints.
4. Implement dice/roll parsing with bounded numeric ranges to prevent long output or overflow.
5. Implement status using existing battery/storage/stat values available through `MyMesh`/DataStore, not new filesystem scans or heavy telemetry.
6. Add per-command cooldown/rate-limit fields in runtime state, with low-risk defaults and no persistent history.
7. Add golden tests for each command, output length caps, disallowed Public behavior, and malformed arguments.
8. Wire `MyMesh` send adapter to transmit approved normal responses to DMs or allowed group channels.

**Contracts and interfaces:**
- Every command writes into a fixed response buffer and returns `handled`, `not_handled`, or `error_short_response`.
- Normal group replies use existing `sendGroupMessage(timestamp, channel, node_name, text, text_len)`.
- DM replies use existing `sendMessage()` with expected ACK/timeout handling.
- No command uses network, filesystem logs, dynamic plugins, SQLite, JSON, or heap containers.

**State/data changes:** Runtime cooldown counters in RAM; no persistent command history.

**Edge cases:** `roll 0d0`, huge dice counts, negative sides, empty help, unknown commands, long user args, message exactly at max length, output truncation.

**Acceptance criteria:**
- Commands respond in DMs, `#bot`, and `#testing` only.
- Public channel commands produce no reply.
- All outputs fit configured safe text limits.
- Firmware builds remain within measured or estimated size budget.

**Verification commands:**
- `python3 tests/firmware_bot/run_tests.py`
- `grep -R "http\|sqlite\|requests\|JSON\|String\|std::vector\|malloc\|new " vendor/MeshCore/examples/companion_radio/Bot* vendor/MeshCore/examples/companion_radio/FirmwareBot*`
- If PlatformIO available: `bash scripts/build-representative.sh --compare`

**Manual validation:** Review command output copy for LoRa-appropriate brevity and match it against `meshcore-bot` behavior where feasible.

**Risks:**
- Python bot feature creep can exceed firmware budgets. Mitigated by excluding API/database/dynamic commands. Trace: ITEM-pitfalls-2, ITEM-prior-art-3.
- Text/frame limits can truncate replies. Mitigated by explicit output caps and tests. Trace: ITEM-pitfalls-5.

**Out of scope for this step:** Emergency forwarding, duplicate suppression, runtime CLI persistence, key import/export hardening.

### Step 6: Implement #emergency to Public forwarding with bounded multipart and loop prevention

**Goal:** Implement the required emergency bridge from `#emergency` to Public using the exact prefix, preserving the original text across bounded multipart messages, never suppressing emergency forwarding, and preventing recursive loops/amplification.

**Why now:** Emergency behavior is distinct from normal commands and should be isolated before normal duplicate suppression is added.

**Dependencies:** Step 4 channel routing; Step 5 send adapter.

**Files:**
- `vendor/MeshCore/examples/companion_radio/EmergencyForwarder.h`
- `vendor/MeshCore/examples/companion_radio/EmergencyForwarder.cpp`
- `vendor/MeshCore/examples/companion_radio/FirmwareBot.*`
- `vendor/MeshCore/examples/companion_radio/MyMesh.*` if Public channel lookup/send adapter needs additions
- `tests/firmware_bot/emergency.*`
- `patches/meshcore/0004-add-emergency-forwarder.patch`

**Existing code to inspect first:**
- `vendor/MeshCore/src/helpers/BaseChatMesh.cpp` group send prefixing and max text behavior
- `vendor/MeshCore/examples/companion_radio/MyMesh.cpp` channel lookup and send frame logic
- `vendor/MeshCore/docs/payloads.md`
- `/Users/cjvana/Documents/GitHub/meshcore-community-bot/community/message_interceptor.py` for existing emergency intent

**Implementation plan:**
1. Implement a dedicated `EmergencyForwarder` path triggered only by exact `#emergency` channel policy classification.
2. Format Public output beginning with `EMERGENCY MESSAGE FROM <user>` followed by the original text, preserving exact user text as much as MeshCore limits allow.
3. Support bounded multipart output with a compile-time max part count and per-part length caps; include part numbering or continuation only if needed and short enough.
4. Add loop prevention: ignore bot-originated Public messages beginning with the emergency prefix, ignore messages already forwarded by this node, and do not reprocess Public as an emergency source.
5. Add rate limiting and queue checks so a malicious or accidental flood cannot monopolize packet pools; failed forwards increment stats.
6. Do not apply normal duplicate suppression to emergency forwarding; multiple bots may forward the same emergency if hidden nodes exist.
7. Add tests for short, exactly-fit, long, very long, prefix-spoofed, Public-origin, and repeated emergency messages.

**Contracts and interfaces:**
- Emergency source channel is exact `#emergency`; destination channel is exact `Public`.
- Emergency forwarding never waits for normal coordinator suppression.
- Emergency formatter returns bounded parts and never emits unbounded multipart output.
- Public normal bot commands still remain silent.

**State/data changes:** RAM-only emergency rate-limit/loop-prevention state; no persistent emergency history.

**Edge cases:** Missing Public channel, missing #emergency channel, long sender names, long original text, text already starting with emergency prefix, multiple bots forwarding concurrently, packet pool unavailable.

**Acceptance criteria:**
- `#emergency` input produces Public message parts with required prefix and original content.
- Public input with the same text does not re-forward.
- Emergency forwarding is never suppressed by normal known-bot response suppression.
- Multipart is bounded and rate-limited.

**Verification commands:**
- `python3 tests/firmware_bot/run_tests.py`
- If PlatformIO available: `bash scripts/build-representative.sh --compare`
- Manual grep confirming `EmergencyForwarder` does not call suppression APIs before sending.

**Manual validation:** Review sample emergency outputs for clarity, truncation behavior, and Public channel safety.

**Risks:**
- Emergency loops/amplification can flood Public. Mitigated by loop prevention and bounded multipart/rate limits while honoring the never-suppress decision. Trace: ITEM-pitfalls-8, PROJECT.md.
- Long emergency messages can consume airtime. Mitigated by max part count and concise formatting. Trace: ITEM-pitfalls-5.

**Out of scope for this step:** Normal command duplicate suppression, signed emergency authentication, Discord/webhook forwarding.

### Step 7: Add passive response coordinator and known-bot trust registry for normal traffic

**Goal:** Reduce duplicate normal bot replies with fixed-size passive listen-before-answer suppression that trusts known bot identities and never applies to emergency forwarding.

**Why now:** Command behavior exists; now it can be delayed/canceled safely for normal bot traffic.

**Dependencies:** Step 5 normal command responses; Step 6 emergency exemption.

**Files:**
- `vendor/MeshCore/examples/companion_radio/ResponseCoordinator.h`
- `vendor/MeshCore/examples/companion_radio/ResponseCoordinator.cpp`
- `vendor/MeshCore/examples/companion_radio/KnownBotRegistry.h`
- `vendor/MeshCore/examples/companion_radio/KnownBotRegistry.cpp`
- `vendor/MeshCore/examples/companion_radio/FirmwareBot.*`
- `tests/firmware_bot/coordinator.*`
- `patches/meshcore/0005-add-response-coordinator.patch`

**Existing code to inspect first:**
- `vendor/MeshCore/src/helpers/SimpleMeshTables.h`
- `vendor/MeshCore/src/Packet.h`
- `vendor/MeshCore/examples/companion_radio/MyMesh.cpp` message metadata and contact lookup
- `/Users/cjvana/Documents/GitHub/meshcore-community-bot/community/message_interceptor.py`

**Implementation plan:**
1. Implement fixed pending/recent fingerprint tables sized for RAK4631: initial target 8-12 pending and 16-24 recent entries.
2. Define normal-response FSM states: observed, eligible, pending self, sent self, suppressed by known bot, expired, failed.
3. Compute bot-specific response delay from channel priority, command priority, configured tier bias, queue health, deterministic bot-key/fingerprint tie-breaker, and bounded jitter; do not use MeshCore `txdelay`/`rxdelay` values directly.
4. Delay normal command send by scheduling `BotResponse` metadata only; allocate MeshCore packets only when the timer wins.
5. Detect known bot responses using key-backed identity when available; weak group-name hints may be recorded for stats but must not suppress safety-critical/emergency paths.
6. Add `KnownBotRegistry` fixed slots for public keys/labels/capability flags, initially populated by runtime config/CLI later or compile-time defaults for tests.
7. Add tests for timer ordering, cancellation, TTL expiry, table full, hidden-node duplicate tolerance, Public ignore, emergency bypass, and known/unknown bot response handling.

**Contracts and interfaces:**
- Coordinator applies only to normal DM/#bot/#testing responses.
- Emergency forwarding bypasses coordinator completely.
- Known-bot authoritative trust requires public key identity where MeshCore exposes it; weak text hints cannot suppress emergency and are disabled or low-risk only by default.
- No explicit claim frames are transmitted in v1.

**State/data changes:** RAM-only pending/recent suppression state and known-bot registry loaded from config when Step 8 lands.

**Edge cases:** Table full, timer wraparound, send failure, repeated command by same user, two users sending same command, unknown spoofed bot text, known bot response after local send, hidden nodes not hearing each other.

**Acceptance criteria:**
- Normal duplicate command tests show later local response is canceled when a trusted known bot response is observed first.
- Unknown/spoofed bot text does not authoritatively suppress.
- Emergency forwarding tests prove no suppression path is used.
- No MeshCore routing or packet duplicate logic is modified.

**Verification commands:**
- `python3 tests/firmware_bot/run_tests.py`
- `grep -R "txdelay\|rxdelay\|direct.txdelay" vendor/MeshCore/examples/companion_radio/*Bot* vendor/MeshCore/examples/companion_radio/ResponseCoordinator*` should show no direct dependency except comments/tests if any.
- If PlatformIO available: `bash scripts/build-representative.sh --compare`

**Manual validation:** Review FSM transition table in tests and confirm stats distinguish suppressed, sent, expired, and failed sends.

**Risks:**
- Passive suppression cannot guarantee exactly-once under hidden nodes. Mitigated by telemetry and acceptance of rare duplicates. Trace: ITEM-pitfalls-9.
- Spoofed group text can silence real bots if trusted. Mitigated by known-key trust and emergency exemption. Trace: ITEM-pitfalls-11, ITEM-architecture-7.

**Out of scope for this step:** Persistent config, CLI commands, explicit claim frames, cryptographic claim signing.

### Step 8: Add BotPrefs persistence and runtime CLI/config commands

**Goal:** Persist small bot settings separately from `NodePrefs` and expose runtime CLI/config controls for enablement, channels, tier/delay, known bots, command toggles, and stats.

**Why now:** The user wants runtime CLI config in Phase 1, and known-bot trust needs field configuration.

**Dependencies:** Steps 4-7 runtime pieces.

**Files:**
- `vendor/MeshCore/examples/companion_radio/BotPrefs.h`
- `vendor/MeshCore/examples/companion_radio/BotPrefs.cpp`
- `vendor/MeshCore/examples/companion_radio/DataStore.h`
- `vendor/MeshCore/examples/companion_radio/DataStore.cpp`
- `vendor/MeshCore/examples/companion_radio/MyMesh.h`
- `vendor/MeshCore/examples/companion_radio/MyMesh.cpp`
- `vendor/MeshCore/src/helpers/CommonCLI.*` only if rescue CLI integration belongs there after inspection
- `tests/firmware_bot/prefs.*`
- `patches/meshcore/0006-add-bot-prefs-and-cli.patch`

**Existing code to inspect first:**
- `vendor/MeshCore/examples/companion_radio/DataStore.cpp`
- `vendor/MeshCore/examples/companion_radio/DataStore.h`
- `vendor/MeshCore/examples/companion_radio/NodePrefs.h`
- `vendor/MeshCore/examples/companion_radio/MyMesh.cpp` `handleCmdFrame`, `enterCLIRescue`, `checkCLIRescueCmd`
- `vendor/MeshCore/src/helpers/CommonCLI.cpp`
- `vendor/MeshCore/docs/companion_protocol.md` if present, plus official docs for frame conventions

**Implementation plan:**
1. Define a compact versioned `BotPrefsV1` blob with magic, version, length, CRC/checksum, enable flag, tier/delay/jitter, cooldowns, channel names or IDs, emergency mapping, known-bot slots, command bitset, and production hardening flags.
2. Add DataStore load/save helpers for a separate bot prefs file such as `/bot_prefs_v1`, without modifying `NodePrefs` persisted offsets.
3. Default safely on missing/corrupt/incompatible bot prefs while preserving the user's desired behavior once configured: enabled bot runtime, Public ignored, `#bot`/`#testing` allowed, `#emergency` to Public.
4. Add rescue CLI commands or companion command-frame handlers for `bot enable|disable`, `bot channels`, `bot tier`, `bot delay`, `bot known add/remove/list`, `bot commands`, and `bot stats`.
5. Add minimal binary protocol/query support only if existing companion command-frame handling can safely allocate a new range; otherwise keep CLI-only for v1 and record protocol config as deferred.
6. Persist known bot public keys as full keys where available, with labels/capability flags bounded to fixed slots.
7. Add tests for serialization round-trip, corrupt CRC, version mismatch, full known-bot table, invalid channel names, invalid delay values, and stats output.

**Contracts and interfaces:**
- Bot prefs are independent from `NodePrefs`; no existing `NodePrefs` offsets change.
- CLI/config inputs are length-checked and reject malformed hex keys/delays/channels.
- Runtime config changes take effect without reboot where safe; persistence errors report via CLI/stat counters.

**State/data changes:** New small bot prefs file in MeshCore storage; known bot registry loaded from persisted config.

**Edge cases:** Missing filesystem, storage full, corrupt prefs, invalid CRC, channel renamed after prefs saved, duplicate known bot key, CLI command too long, unsupported companion app ignoring bot frames.

**Acceptance criteria:**
- Existing NodePrefs load/save code remains binary-compatible.
- Bot prefs round-trip tests pass.
- CLI can enable/disable bot and list stats/known bots in a bounded response.
- Corrupt bot prefs default safely and do not crash boot.

**Verification commands:**
- `python3 tests/firmware_bot/run_tests.py`
- `grep -n "loadPrefsInt\|savePrefs\|NodePrefs" vendor/MeshCore/examples/companion_radio/DataStore.cpp vendor/MeshCore/examples/companion_radio/NodePrefs.h` followed by diff review confirming existing offsets unchanged.
- If PlatformIO available: `bash scripts/build-representative.sh --compare`

**Manual validation:** Inspect CLI help/output strings for length and field clarity.

**Risks:**
- NodePrefs binary-layout changes can corrupt existing settings. Mitigated by separate versioned BotPrefs. Trace: ITEM-pitfalls-16.
- Persisting high-churn state can wear or fill flash. Mitigated by persisting config only, not runtime caches. Trace: ITEM-pitfalls-15.

**Out of scope for this step:** Companion mobile app UI, cloud provisioning, persistent command history, large stats database.

### Step 9: Add production bot build flags and private-key import/export hardening

**Goal:** Provide a production bot firmware build mode that disables private key import/export and keeps bot-related feature flags explicit across representative targets.

**Why now:** Known-bot trust makes bot identities more sensitive, and the user chose disabling key import/export by default for production bot firmware.

**Dependencies:** Bot runtime and config exist; representative build tooling exists.

**Files:**
- `vendor/MeshCore/platformio.ini`
- `vendor/MeshCore/variants/heltec_v3/platformio.ini` if target-specific flags are needed
- `vendor/MeshCore/variants/rak4631/platformio.ini` if target-specific flags are needed
- `scripts/build-representative.sh`
- `.github/workflows/firmware-build.yml`
- `patches/meshcore/0007-add-bot-build-flags-and-key-hardening.patch`

**Existing code to inspect first:**
- `vendor/MeshCore/platformio.ini` lines defining `ENABLE_PRIVATE_KEY_IMPORT` and `ENABLE_PRIVATE_KEY_EXPORT`
- `vendor/MeshCore/examples/companion_radio/MyMesh.cpp` private key import/export command handling
- Representative variant `.ini` files for build flag inheritance

**Implementation plan:**
1. Add explicit `CMESH_BOT_ENABLED` build flags for companion bot builds in the Colorado patch series.
2. Add production bot build flag behavior that omits or overrides `ENABLE_PRIVATE_KEY_IMPORT=1` and `ENABLE_PRIVATE_KEY_EXPORT=1` for bot firmware builds.
3. Preserve a clearly named development/provisioning build option only if necessary for local testing, and keep it opt-in.
4. Ensure representative Heltec v3 and RAK4631 USB/BLE builds receive consistent bot flags.
5. Add compile-time guards around private-key import/export command paths if upstream guards are not already sufficient.
6. Update build script to print whether bot and key-hardening flags are active per environment.
7. Add tests or grep-based verification that production bot build flags do not include key import/export.

**Contracts and interfaces:**
- Production bot builds disable private key import/export by default.
- Development/provisioning behavior is explicit and not the default path used for hardware flashing.
- Existing non-bot upstream builds can remain unchanged if the patch is scoped through bot flags.

**State/data changes:** Build configuration only; no runtime data changes.

**Edge cases:** PlatformIO `build_flags` inheritance not merging as expected, variant-specific flags overriding base flags, command code compiled without guarding, CI accidentally building provisioning mode.

**Acceptance criteria:**
- Representative production bot builds show `CMESH_BOT_ENABLED` active and private key import/export inactive.
- Any provisioning build is explicit and not used by default scripts.
- Firmware source still compiles without bot flag if stock compatibility is maintained.

**Verification commands:**
- `grep -R "ENABLE_PRIVATE_KEY_IMPORT\|ENABLE_PRIVATE_KEY_EXPORT\|CMESH_BOT_ENABLED" vendor/MeshCore/platformio.ini vendor/MeshCore/variants/heltec_v3/platformio.ini vendor/MeshCore/variants/rak4631/platformio.ini vendor/MeshCore/examples/companion_radio/MyMesh.cpp`
- If PlatformIO available: `bash scripts/build-representative.sh --compare`
- CI workflow dry syntax check if available via `actionlint` or manual YAML parse.

**Manual validation:** Review final build logs to confirm production flags are printed as intended.

**Risks:**
- Private key export/import enabled in trusted bot builds can allow cloned bot identities. Mitigated by default production hardening. Trace: ITEM-pitfalls-18.
- PlatformIO flag inheritance can surprise; inspect actual compile commands/build logs. Trace: ITEM-stack-8, ITEM-pitfalls-17.

**Out of scope for this step:** Full provisioning UX, physical-button-gated key migration, mobile app support.

### Step 10: Add CI enforcement, size thresholds, and resource regression checks

**Goal:** Turn the build and size reports into hard gates for the MVP and add simple static checks for firmware safety rules.

**Why now:** After bot features and build flags exist, the project needs automated protection against resource and style regressions before flashing/deployment.

**Dependencies:** Steps 2-9.

**Files:**
- `.github/workflows/firmware-build.yml`
- `scripts/check-bot-safety.sh`
- `scripts/parse-size-report.py`
- `colorado/size-baseline/*.json` if baseline is committed
- `patches/meshcore/` updated if CI-related firmware patches are needed

**Existing code to inspect first:**
- Output from first successful representative build logs
- `scripts/build-representative.sh`
- `.github/workflows/firmware-build.yml`

**Implementation plan:**
1. Capture a clean upstream/bot baseline size JSON once representative builds work.
2. Add threshold configuration for RAK4631 BLE/USB and Heltec v3 BLE/USB deltas, initially warning above 25 KB flash / 2 KB static RAM and failing above agreed hard-review thresholds unless no baseline exists.
3. Add `scripts/check-bot-safety.sh` to grep bot source for hot-path heap APIs, Arduino `String`, dynamic containers, JSON, network/API imports, and forbidden lower-layer timing dependency.
4. Wire CI to run host tests, safety checks, representative builds, size parsing, and artifact upload.
5. Ensure CI failure messages identify the env and resource that regressed.
6. Add a local `scripts/verify.sh` wrapper that runs the same non-flashing checks developers should run before review.
7. Document how to intentionally update baselines when upstream MeshCore changes.

**Contracts and interfaces:**
- `scripts/verify.sh` is the standard pre-review command.
- CI fails on host tests/safety checks/build failures.
- CI prints size deltas even when enforcement is in warn-only mode.

**State/data changes:** Optional committed size baseline; CI artifacts.

**Edge cases:** Upstream size changes without bot changes, missing baseline, parser unable to read PlatformIO output, false-positive grep hits in comments/tests, PlatformIO cache failures.

**Acceptance criteria:**
- `scripts/verify.sh` runs host tests and safety checks locally.
- CI workflow contains the representative build matrix and uploads logs/artifacts.
- Size report clearly identifies Heltec v3 and RAK4631 resource use.

**Verification commands:**
- `bash scripts/check-bot-safety.sh`
- `bash scripts/verify.sh` if PlatformIO is not required for local subset or after installing PlatformIO
- `python3 -m py_compile scripts/parse-size-report.py`
- If PlatformIO available: `bash scripts/build-representative.sh --compare`

**Manual validation:** Review CI output format and confirm a future maintainer can see exact RAK4631 headroom.

**Risks:**
- Build-only checks can miss runtime heap/queue issues. Mitigated by static safety checks and later hardware smoke. Trace: ITEM-pitfalls-3, ITEM-pitfalls-4.
- Enforcement too strict before measured baseline can block progress. Mitigated by warn-only until baseline exists. Trace: ITEM-stack-13.

**Out of scope for this step:** Full all-companion build matrix enforcement, long-duration soak tests, field RF testing.

### Step 11: Run representative builds and flash the plugged-in Heltec v3 smoke target

**Goal:** Build verified firmware artifacts, flash the connected Heltec v3, and perform the first hardware smoke test of the firmware-only bot.

**Why now:** The user authorized the plugged-in Heltec v3 as the bot node, but hardware should only be touched after code review/builds pass.

**Dependencies:** Steps 1-10 complete; PlatformIO installed locally or CI artifacts available; Heltec v3 connected by USB; build artifacts produced.

**Files:**
- No source files unless small build/flash script fixes are discovered
- `out/firmware/*` artifacts
- `out/size/*` logs
- Optional `scripts/flash-heltec-v3.sh`
- `.forge/steps/step-11-plan.md` will contain the exact port and flash command discovered at runtime

**Existing code to inspect first:**
- `vendor/MeshCore/build.sh` flash/upload behavior
- `vendor/MeshCore/variants/heltec_v3/platformio.ini`
- Current serial devices under `/dev/cu.*` and `/dev/tty.*`

**Implementation plan:**
1. Run `scripts/verify.sh` and representative builds, requiring Heltec v3 USB/BLE and RAK4631 USB/BLE success or explicitly recorded blockers.
2. Identify the connected Heltec v3 serial port using non-destructive local device listing.
3. Choose the correct artifact/env for the user's connected Heltec v3 mode, likely `Heltec_v3_companion_radio_usb` first unless BLE-specific flashing is requested.
4. Flash the Heltec v3 with the verified production bot build, using upstream PlatformIO/build tooling rather than ad-hoc binary writes.
5. Open a serial monitor or CLI only as needed to confirm boot, firmware version/build flags, bot config status, storage stats, and no immediate crash loop.
6. Perform safe smoke commands through the available interface: DM/private if possible, `#bot`/`#testing` if available, confirm Public normal command ignore, and test emergency forwarding only if it will not surprise real users on the live mesh.
7. Record hardware smoke outcomes and any limitations in the final Forge summary.

**Contracts and interfaces:**
- Flashing uses production bot build with key export/import disabled.
- No destructive device operations beyond firmware flashing authorized by the user.
- Emergency live test is skipped or simulated if it would broadcast unexpectedly on the real mesh.

**State/data changes:** The connected Heltec v3 firmware is overwritten with the new bot firmware; local build artifacts and logs are created.

**Edge cases:** Multiple serial devices, bootloader mode required, wrong env selected, PlatformIO upload failure, device already configured for live Public channel, emergency test could broadcast to real mesh.

**Acceptance criteria:**
- Representative builds pass or blockers are documented before flashing.
- Heltec v3 flashes successfully and boots.
- Firmware reports bot enabled/configurable and storage within budget.
- Safe smoke tests show DM/allowed-channel behavior and Public normal silence.

**Verification commands:**
- `bash scripts/verify.sh`
- `bash scripts/build-representative.sh --compare`
- `ls /dev/cu.* /dev/tty.*`
- Flash command selected by implementation after reading upstream tooling, likely `pio run -d vendor/MeshCore -e Heltec_v3_companion_radio_usb -t upload --upload-port <port>` or equivalent upstream `build.sh` workflow.
- Serial/CLI command selected after detecting the connected interface.

**Manual validation:** Observe device boot logs, confirm no crash loop, confirm bot command behavior through safe channels, and avoid live emergency broadcast unless deliberately intended.

**Risks:**
- Flashing overwrites the user's current Heltec firmware/config. User has authorized flashing after pass, but the step should still report the exact action before execution. Trace: PROJECT.md.
- Live emergency testing could broadcast to Public. Mitigated by simulation or explicit pre-test confirmation if connected to a real mesh. Trace: ITEM-pitfalls-8.

**Out of scope for this step:** Field deployment across multiple bots, RAK4631 hardware flashing, all companion device flashing.

## Cross-Step Integration Checks

- Apply patch queue from a clean submodule and verify all patches apply in order.
- Run host bot tests after every behavior step.
- Run `scripts/check-bot-safety.sh` after bot source is introduced.
- Run representative builds for Heltec v3 USB/BLE and RAK4631 USB/BLE once PlatformIO is available.
- Compare bot size deltas against RAK4631 and Heltec budgets.
- Confirm no modifications were made to MeshCore routing/dispatcher/ACK/path duplicate logic.
- Confirm Public normal command traffic remains silent after all command/coordinator/config steps.
- Confirm emergency forwarding bypasses normal suppression after coordinator integration.
- Confirm production bot build disables private key import/export before flashing.
- Confirm patch export/reapply works before each commit that modifies submodule content.

## Testing Strategy

- **Host unit tests:** Parser, command classification, channel policy, emergency formatting, fingerprinting, known bot registry, coordinator FSM, BotPrefs serialization, malformed inputs, and output length caps.
- **Static safety checks:** No hot-path heap allocation or dynamic containers in bot source; no direct lower-layer `txdelay`/`rxdelay` dependency; no API/network/SQLite/plugin imports.
- **Firmware builds:** Representative local/CI builds for `Heltec_v3_companion_radio_usb`, `Heltec_v3_companion_radio_ble`, `RAK_4631_companion_radio_usb`, and `RAK_4631_companion_radio_ble`.
- **Size reports:** PlatformIO RAM/flash section output plus artifact bytes, compared against baseline and thresholds.
- **Hardware smoke:** Flash connected Heltec v3 only after build/review pass; verify boot, storage stats, bot config, allowed-channel behavior, Public silence, and safe emergency formatting if not live-broadcasting.
- **Review gates:** Every implementation step is staged and reviewed by a Claude forge-reviewer before commit; final full-project review runs after all steps.

## Out of Scope

- Full Python `meshcore-bot` feature parity, especially HTTP/API/weather/AQI/sports/satellite/Discord/web viewer/SQLite/dynamic plugin features.
- VPS coordinator emulation or dependency on internet/IP connectivity.
- Explicit on-air claim frames in v1.
- Changes to MeshCore core routing, dispatcher, ACK/path maintenance, packet duplicate tables, or lower-layer TX/RX delay semantics.
- Mobile app UI changes for bot config.
- RAK4631 hardware flashing unless separately authorized and available.
- Full all-companion build matrix before representative Heltec v3 and RAK4631 gates pass.
