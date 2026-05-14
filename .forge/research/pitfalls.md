# Pitfalls Research: Firmware-only MeshCore Bot

Project: firmware-only embedded bot command handling in MeshCore companion firmware. Upstream MeshCore is the firmware base/submodule/patch base; `meshcore-bot` is a behavioral reference only. Normal bot traffic is limited to private DMs, `#bot`, and `#testing`; `#emergency` is forwarded to Public as an emergency announcement. Representative build targets: Heltec v3 and RAK4631.

Checked: 2026-05-14

Storage baseline used in this research:

| Target | Hardware/storage facts | Current firmware size signal | Headroom implication |
|---|---|---|---|
| Heltec WiFi LoRa 32 V3 | ESP32-S3FN8, 8 MB SiP flash, 512 KB SRAM, no external PSRAM, SX1262 | MeshCore v1.15.0 release assets: Heltec v3 USB app `.bin` 615 KB; BLE app `.bin` 1.2 MB; merged BLE 1.27 MB | Flash is comfortable for a small firmware bot, but RAM is still constrained and BLE/display/WiFi variants can consume much more than USB. |
| RAK4631 | nRF52840, 1 MB flash, 256 KB RAM, BLE 5.0, SX1262; local companion env caps app at 712,704 bytes and RAM region at 237,568 bytes after SoftDevice reservation | MeshCore v1.15.0 release assets: RAK4631 USB `.uf2` 933 KB / `.zip` 467 KB; BLE `.uf2` 949 KB / `.zip` 475 KB. UF2 includes container overhead, so zip size is the best visible proxy without a local ELF/map build. | RAK4631 is the limiting target. A compact parser/coordinator is likely feasible; Python-bot parity or large lookup tables are not. |

Estimated firmware-only bot budget, before measurement: a compact built-in bot should target **20-60 KB additional flash**, **2-8 KB static/RAM**, **0 heap allocation after setup**, and **<1 KB persisted prefs**. A fuller command suite with many string templates, JSON/API logic, large help text, or display telemetry can easily exceed this and should be gated per target. These estimates are MEDIUM confidence because no local PlatformIO build/map was available in this repo.

### ITEM-pitfalls-1: Underestimating RAK4631 as the storage/RAM limiting device

- **What goes wrong:** The bot fits and behaves on Heltec v3 ESP32-S3, then fails to link, crashes, or becomes unstable on RAK4631 BLE because the nRF52840 target has far less usable flash/RAM once SoftDevice, BLE, display, contact tables, channel tables, offline queue, packet pools, and filesystem regions are included.
- **Root cause:** Heltec v3 has 8 MB flash and 512 KB SRAM, while RAK4631 has 1 MB flash and 256 KB RAM. The local RAK4631 companion env further sets `board_upload.maximum_size = 712704`, and its linker script places application flash from `0x26000` to `0xD4000` with RAM from `0x20006000` to `0x20040000`. Current upstream release assets show RAK4631 companion firmware is already hundreds of KB before bot code, and the BLE variant carries additional queues/logging/display code.
- **Prevention:** Make RAK4631 BLE the hard budget target, not Heltec v3. Require every bot feature PR to report `.text/.rodata/.data/.bss` deltas from PlatformIO map/size output for `RAK_4631_companion_radio_ble`, `RAK_4631_companion_radio_usb`, `Heltec_v3_companion_radio_ble`, and `Heltec_v3_companion_radio_usb`. Keep Phase 1 to a compact command parser, passive duplicate suppression, emergency forwarding, and a few single-packet commands. Defer weather/AQI/sports/satpass/feed/web features unless a build map proves headroom.
- **Severity:** CRITICAL
- **Phase relevance:** Phase 0 feasibility and every CI build gate.
- **Confidence:** HIGH
- **Source:** Local code + official hardware docs + GitHub release assets — `/Users/cjvana/Documents/GitHub/MeshCore/variants/rak4631/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/boards/nrf52840_s140_v6_extrafs.ld`; https://heltec.org/project/wifi-lora-32-v3/ ; https://docs.rakwireless.com/product-categories/wisblock/rak4631/overview/ ; https://github.com/meshcore-dev/MeshCore/releases/expanded_assets/companion-v1.15.0
- **Checked:** 2026-05-14

### ITEM-pitfalls-2: Treating the Python bot as portable command code instead of a behavioral oracle

- **What goes wrong:** Firmware grows into an unmaintainable partial Python-bot clone: many commands require internet APIs, JSON parsing, caching, date/time libraries, HTTP/TLS, Discord/web hooks, SQLite-like history, or large text responses. The result exceeds MCU flash/RAM, cannot work off-grid, and behaves differently from the host bot anyway.
- **Root cause:** The Python bot depends on host resources and libraries (`requests`, `aiohttp`, Flask, feedparser, PyEphem, Open-Meteo clients, cryptography, `meshcore-cli`) and includes command modules for weather, AQI, satellite passes, feeds, sports, web viewer, Discord forwarding, stats databases, and contact management. Those are not firmware-native features.
- **Prevention:** Define a firmware-only minimum viable command set: `ping`, `help` with short static text, status/battery/storage, path/contact diagnostics from existing MeshCore state, and emergency forwarding. Treat Python modules as golden behavior for command names, channel policy, cooldowns, and output style only. Anything needing IP services should stay host-side or be replaced by a cached/static firmware-safe answer.
- **Severity:** CRITICAL
- **Phase relevance:** Scope definition and feature triage.
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/meshcore-bot/requirements.txt`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/commands/*`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/message_handler.py`
- **Checked:** 2026-05-14

### ITEM-pitfalls-3: Heap fragmentation from `String`, dynamic containers, JSON, and variable response construction

- **What goes wrong:** The bot passes bench tests but reboots or fails to allocate packets after hours/days of varied commands because small MCU heaps fragment. Failures look like random packet-pool exhaustion, BLE instability, or corrupted command output.
- **Root cause:** Firmware bot parsing tempts use of Arduino `String`, `std::vector`, maps, JSON builders, formatted heap strings, and per-message dynamic objects. Upstream MeshCore already warns contributors to avoid dynamic allocation except during setup/begin, and its packet manager uses fixed pools. The companion currently has only one explicit large heap path for signing (`malloc(MAX_SIGN_DATA_LEN)`), which should not become a model for bot logic.
- **Prevention:** Use fixed-size structs, ring buffers, and `char[]` parsing. Allocate coordinator tables statically; no `malloc/new/String` in receive, parse, schedule, or send paths. Keep command output templates in `const`/flash-friendly storage where supported, and always bound-copy into `MAX_TEXT_LEN`/`MAX_FRAME_SIZE` buffers. Add a static-analysis/code-review rule that bot files cannot use `String`, heap containers, or heap allocation after setup.
- **Severity:** CRITICAL
- **Phase relevance:** Firmware implementation standards and review checklist.
- **Confidence:** HIGH
- **Source:** Local code + ecosystem reference — `/Users/cjvana/Documents/GitHub/MeshCore/README.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/StaticPoolPacketManager.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; https://cpp4arduino.com/2018/11/06/what-is-heap-fragmentation.html
- **Checked:** 2026-05-14

### ITEM-pitfalls-4: Packet-pool and offline-queue exhaustion during delayed bot responses

- **What goes wrong:** A burst of commands creates many pending responses or emergency forwards. The 16-packet pool fills, outbound/inbound queues fill, MeshCore reports table-full/send failures, and normal ACK/path/adverts/routing maintenance are starved. Losing bot responses may still occupy packet slots until their timers fire.
- **Root cause:** Companion `MyMesh` constructs `StaticPoolPacketManager(16)`, and `StaticPoolPacketManager` uses fixed send/rx queues. BLE variants may set `OFFLINE_QUEUE_SIZE=256`, which is a large RAM commitment, while USB defaults to 16. Existing code drops oldest channel messages when offline queue is full, so a firmware bot can accidentally evict user-visible messages with its own generated traffic.
- **Prevention:** Keep pending-response state separate from allocated MeshCore packets. Do not call `createGroupDatagram()` until the response timer actually wins. Limit pending bot windows to a small fixed number, e.g. 8-16. If over capacity, drop low-priority bot replies before user messages. Emergency forwarding may preempt normal bot responses but must still respect packet-pool availability and report a dropped-forward counter.
- **Severity:** CRITICAL
- **Phase relevance:** Coordinator implementation and stress testing.
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/StaticPoolPacketManager.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/variants/heltec_v3/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/variants/rak4631/platformio.ini`
- **Checked:** 2026-05-14

### ITEM-pitfalls-5: Exceeding MeshCore text/frame limits with Python-style bot replies

- **What goes wrong:** Help, weather, emergency, sports, stats, or path responses are truncated, rejected, split by future code into multiple packets, or silently lose context. Duplicate suppression may suppress only the first fragment while later fragments still transmit.
- **Root cause:** MeshCore constants are small: `MAX_PACKET_PAYLOAD=184`, `MAX_FRAME_SIZE=172`, and `MAX_TEXT_LEN=160`. Companion protocol docs currently state datagram payload caps around 163 bytes and text messages around 133 characters. `BaseChatMesh::sendGroupMessage()` prefixes group messages with `"<sender>: "`, reducing available bot text. LoRa airtime makes multi-packet bot chatter expensive.
- **Prevention:** Design every firmware command as a single-packet, LoRa-native response. Enforce command-specific output caps before send; include a test that serializes exact channel output with the node name prefix. `#emergency` forwarding must be short enough to fit or use a deterministic two-message policy with bounded truncation: announcement plus truncated original text. Do not implement multipart public bot replies in Phase 1.
- **Severity:** MODERATE
- **Phase relevance:** UX copy, command implementation, emergency forwarding.
- **Confidence:** HIGH
- **Source:** Local code + official docs — `/Users/cjvana/Documents/GitHub/MeshCore/src/MeshCore.h`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseSerialInterface.h`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.h`; https://docs.meshcore.io/companion_protocol/
- **Checked:** 2026-05-14

### ITEM-pitfalls-6: Unsafe C++ command parsing and text normalization

- **What goes wrong:** A malformed frame or odd user message causes buffer over-read, unterminated strings, command misfire, or incorrect sender extraction. Inputs containing colons, empty names, control characters, UTF-8, long aliases, or embedded NULs may bypass filters or corrupt output.
- **Root cause:** Existing companion code commonly null-terminates `cmd_frame[len]`, uses `strlen`, `strcpy`, `sprintf`, and colon parsing conventions (`sender: message`) inherited from group text format. Python `meshcore-bot` sanitizes input with helper utilities, but firmware C++ must do this manually under smaller buffers.
- **Prevention:** Build a tiny parser that operates on `(const char*, length)` not untrusted `strlen` until after explicit NUL insertion inside a known spare byte. Strip C0 controls except newline-equivalent spaces; normalize only ASCII command prefixes in Phase 1; treat everything after the command verb as bounded opaque text. Use `snprintf`/bounded copy everywhere. Add fuzz/unit tests for maximum-length messages, no colon, leading colon, repeated colon, UTF-8, NUL, and all allowed channel names.
- **Severity:** CRITICAL
- **Phase relevance:** Command parser implementation and test harness.
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/security_utils.py`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/message_handler.py`
- **Checked:** 2026-05-14

### ITEM-pitfalls-7: Misidentifying channels by index instead of stable channel policy

- **What goes wrong:** The bot answers on Public during normal operation, ignores `#bot`, mishandles `#testing`, or forwards the wrong channel because channel indexes differ across devices. A user renames/reorders channels and the bot policy silently changes.
- **Root cause:** Companion receive frames identify channel by `channel_idx`; `BaseChatMesh` stores `ChannelDetails` in an array and `findChannelIdx()` matches channel secrets. Public is auto-added first, then persisted channels load. Indexes are local configuration, not global semantic names. MeshCore channel messages are group-key based and unverified by sender identity at the spec level.
- **Prevention:** Resolve policy by channel secret/hash plus configured display name at startup, then cache explicit channel slots for `Public`, `#bot`, `#testing`, and `#emergency`. Refuse to enable the bot if required channels are ambiguous, missing, or duplicate. Normal responses must be allowed only in DMs, `#bot`, and `#testing`; Public sends are allowed only from the emergency forwarder and self-tests. Expose a diagnostic command showing channel index/name/hash/policy.
- **Severity:** CRITICAL
- **Phase relevance:** Channel setup and command routing.
- **Confidence:** HIGH
- **Source:** Local code + official docs — `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/ChannelDetails.h`; https://docs.meshcore.io/companion_protocol/
- **Checked:** 2026-05-14

### ITEM-pitfalls-8: Emergency forwarding loops, amplification, and spoofed urgency

- **What goes wrong:** A message in `#emergency` is forwarded repeatedly by every bot to Public; bots then see the Public announcement and re-forward or respond; malicious users trigger panic banners; long emergency text consumes multiple high-priority packets during congestion.
- **Root cause:** The new rule intentionally bridges `#emergency` to Public. Without an idempotency key, TTL, and bot-origin detection, every firmware bot can act on the same emergency packet independently. Group messages identify the text sender string but are not a strong authenticated identity. Passive duplicate suppression without claim frames allows hidden bots to miss each other.
- **Prevention:** Treat emergency forwarding as a separate high-priority, idempotent action keyed by original channel hash, timestamp, normalized original text, and sender string/public key prefix where available. Keep an emergency-forward cache longer than normal bot duplicate windows, e.g. 10-30 minutes. Never forward bot-originated `EMERGENCY MESSAGE FROM` text. Use exactly the user-requested Public format but clamp length. Prefer one winner via the same passive suppression window, but if duplicates occur they must have identical text and no recursive trigger.
- **Severity:** CRITICAL
- **Phase relevance:** Emergency feature design before public testing.
- **Confidence:** HIGH
- **Source:** Project requirement + local code — `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/message_handler.py`
- **Checked:** 2026-05-14

### ITEM-pitfalls-9: Passive duplicate suppression without claim frames cannot guarantee single response

- **What goes wrong:** Two bots that cannot hear each other both answer after their local delay window. Bench tests with colocated radios look clean, but field deployments still produce duplicates across terrain, hidden nodes, and asymmetric paths.
- **Root cause:** Phase 1 intentionally defers explicit on-air claim frames. LoRa listen-before-talk/CAD reduces collisions but does not solve hidden-node consensus. MeshCore’s duplicate tables suppress identical packets, not independently generated bot responses. A final response can only suppress peers that hear it before their timer fires.
- **Prevention:** Set expectations: Phase 1 reduces noise, not guarantees exactly once. Use deterministic scoring plus jitter, short bounded windows, and cancel-on-heard-response. Record duplicate fingerprints in telemetry. Do not add claim frames until field captures prove passive suppression is insufficient; if claims are added, authenticate them and keep them zero-hop/scoped.
- **Severity:** MODERATE
- **Phase relevance:** Coordination algorithm and acceptance criteria.
- **Confidence:** HIGH
- **Source:** Local code + ecosystem docs/search — `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/SimpleMeshTables.h`; `/Users/cjvana/Documents/GitHub/MeshCore/src/Mesh.cpp`; https://meshtastic.org/docs/overview/mesh-algo/ ; https://scholarworks.gnu.ac.kr/item/ccb29254-804b-413d-adc5-853fe697241b
- **Checked:** 2026-05-14

### ITEM-pitfalls-10: Suppression keyed by raw packet hash or plain text creates false positives/negatives

- **What goes wrong:** Bots fail to suppress duplicate responses for the same command observed through different RF envelopes, or they suppress unrelated commands because two users typed the same text. Emergency forwarding may drop a real second emergency because the text happens to match.
- **Root cause:** MeshCore flood/direct paths, transport codes, and retry/ACK behavior can change packet bytes while the application-level command is the same. Conversely, text-only matching ignores sender, channel, timestamp, payload type, and DM vs channel semantics.
- **Prevention:** Define a firmware `BotMessageFingerprint` over stable application fields after decryption/parsing: payload kind, channel identity or DM peer identity, sender timestamp, normalized command text, sender public-key prefix/name when available, and a small packet hash tie-in for diagnostics. Store both request fingerprint and response fingerprint. Use a separate emergency fingerprint with a longer TTL.
- **Severity:** CRITICAL
- **Phase relevance:** Coordinator state model and test vectors.
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/MeshCore/src/Packet.h`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/SimpleMeshTables.h`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/message_handler.py`
- **Checked:** 2026-05-14

### ITEM-pitfalls-11: Trusting spoofable bot identities, response text, or control packets

- **What goes wrong:** A non-bot node suppresses real bots by sending fake claim/control data or a message that looks like a bot response. In emergency mode this can block forwarding or inject false Public emergency announcements.
- **Root cause:** MeshCore `PAYLOAD_TYPE_CONTROL` is a generic control/discovery packet path and companion currently forwards control data to the app without authentication. Group text is shared-key encrypted but the visible `name: message` prefix is not enough to prove a known bot identity. The project decision says suppression should trust known bot identities only.
- **Prevention:** In Phase 1, suppress only on final responses that match known bot public-key prefixes or an allowlisted bot identity table; do not suppress emergency forwarding based on unauthenticated claims. If control/claim packets are introduced later, include version/length/magic/fingerprint and require signature or known-key verification before they can suppress. Parse unknown control frames as untrusted hints or ignore them.
- **Severity:** CRITICAL
- **Phase relevance:** Security design and duplicate suppression.
- **Confidence:** HIGH
- **Source:** Local code/docs — `/Users/cjvana/Documents/GitHub/MeshCore/docs/payloads.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/Packet.h`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`
- **Checked:** 2026-05-14

### ITEM-pitfalls-12: Blocking the companion superloop with bot delays or slow display/BLE/serial work

- **What goes wrong:** While a bot waits to see if another node responds, BLE notifications stall, serial frames time out, display/UI feels frozen, RTC ticks lag, sensors are skipped, or radio RX/TX processing is delayed. The coordinator makes the node less reliable than stock companion firmware.
- **Root cause:** Companion firmware is a cooperative loop: `the_mesh.loop()`, serial/BLE/WiFi interface polling, sensors, display task, and RTC tick all depend on returning quickly. Protocol docs require one in-flight command and handling unsolicited notifications, while local display/UI code also updates from message paths.
- **Prevention:** Implement response delays as scheduled timestamps in a small FSM checked from `MyMesh::loop()`. Never use `delay()` or busy waits for bot coordination. Do not render long bot logs on display; only signal a compact bot event/counter. Respect `_serial->isWriteBusy()` before pushing optional stats. Emergency forwarding can preempt normal bot work but must still be non-blocking.
- **Severity:** CRITICAL
- **Phase relevance:** Main-loop integration and UI/BLE/serial testing.
- **Confidence:** HIGH
- **Source:** Local code + official docs — `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/main.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; https://docs.meshcore.io/companion_protocol/
- **Checked:** 2026-05-14

### ITEM-pitfalls-13: Breaking BLE/serial companion protocol compatibility with bot notifications

- **What goes wrong:** Existing apps and `meshcore-bot` clients misparse frames, lose messages, or deadlock because firmware emits new bot frames while a command response is expected, changes existing response codes, or overflows BLE MTU assumptions.
- **Root cause:** Companion protocol clients are instructed to send one command at a time, wait for a response, and also handle asynchronous notifications. Firmware version/capability fields vary by app target version. Current `MyMesh` has fixed command/response/push code ranges and a local `FIRMWARE_VER_CODE=8` in the read checkout.
- **Prevention:** Do not change existing frame semantics for stock commands. In firmware-only bot mode, prefer no new host-visible frames in Phase 1 except stats/capability behind explicit query. If new frames are required, park them in a new negotiated range, gate by firmware version/capability, keep every frame <= `MAX_FRAME_SIZE`, and ensure old clients safely ignore or never see them.
- **Severity:** MODERATE
- **Phase relevance:** Firmware API and compatibility testing.
- **Confidence:** HIGH
- **Source:** Local code + official docs — `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.h`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; https://docs.meshcore.io/companion_protocol/
- **Checked:** 2026-05-14

### ITEM-pitfalls-14: Misusing SNR/RSSI/path data to choose the “closest” bot

- **What goes wrong:** A bot with a strong last-hop signal wins even though it is not closest to the original sender, or a bot suppresses itself because a stale contact path suggests another node is better. Mobile or asymmetric links make the scoring erratic.
- **Root cause:** MeshCore path fields differ by route type. Flood path is historical route; direct path is routing instructions; TRACE path fields can contain SNR data. RSSI/SNR from the radio is for the last received hop, not end-to-end quality. Python bot code already carries complex RF correlation fallbacks because host-side events can be difficult to match.
- **Prevention:** Use SNR/RSSI only as a weak tie-breaker for zero-hop/direct observations. Primary eligibility should be command support, channel policy, known bot identity, recent directness/path length, queue health, and deterministic bot-id jitter. Keep topology tier as a small bias, not a winner-takes-all delay. Include test captures for multi-hop and TRACE packets.
- **Severity:** MODERATE
- **Phase relevance:** Scoring model and field validation.
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/MeshCore/src/Mesh.h`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/message_handler.py`
- **Checked:** 2026-05-14

### ITEM-pitfalls-15: Persisting high-churn bot state and wearing or filling filesystem storage

- **What goes wrong:** Duplicate windows, recent-response caches, bot logs, command history, or emergency history are written to flash repeatedly. Filesystem wear, corruption, or storage-full behavior then breaks identity, contacts, channels, and advert blobs.
- **Root cause:** Companion `DataStore` already persists identity, prefs, contacts, channels, and advert blobs using SPIFFS/LittleFS/InternalFS/ExtraFS. nRF52 targets preallocate advert blob records and may migrate contacts/channels to secondary FS. `CMD_GET_BATT_AND_STORAGE` reports storage but bot runtime state does not need persistence.
- **Prevention:** Persist only bot configuration: enabled flag, tier, channel policy hashes, known bot identity prefixes, cooldown settings, and emergency-forward enabled. Keep all request fingerprints, suppression windows, emergency recent cache, and counters in RAM with bounded TTL. If counters must survive reboot, save coarse totals lazily and rarely, never per message.
- **Severity:** MODERATE
- **Phase relevance:** Persistence design and DataStore changes.
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/DataStore.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/DataStore.h`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/NodePrefs.h`
- **Checked:** 2026-05-14

### ITEM-pitfalls-16: NodePrefs binary-layout migration breaks existing companion settings

- **What goes wrong:** Adding bot fields to `NodePrefs` changes struct layout expectations, causing old `/new_prefs` files to load wrong values for radio, BLE PIN, location policy, or buzzer settings. A field-unit mismatch could put bots on the wrong LoRa params or expose a stale BLE PIN.
- **Root cause:** `DataStore::loadPrefsInt()` and `savePrefs()` manually read/write specific offsets into `/new_prefs`; the persisted file is not a self-describing schema. App/device compatibility also depends on existing command frames for tuning and other params.
- **Prevention:** Do not insert fields into the middle of `NodePrefs` or change existing offsets. Add a separate bot prefs file with magic/version/length/CRC, or append only with explicit backwards-compatible load defaults. On first boot after update, validate radio params, channel policy, and bot enable flag; default bot disabled if config is invalid.
- **Severity:** CRITICAL
- **Phase relevance:** Configuration persistence implementation.
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/NodePrefs.h`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/DataStore.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`
- **Checked:** 2026-05-14

### ITEM-pitfalls-17: All-companion portability breaks through target-specific assumptions

- **What goes wrong:** Firmware bot code compiles for Heltec v3 but fails on nRF52, RP2040, STM32, USB-only, BLE, WiFi, or no-display companion variants. A command uses ESP32-only APIs, assumes SPIFFS paths, assumes BLE exists, or relies on display/UI classes not present on every target.
- **Root cause:** MeshCore supports many PlatformIO environments with variant overlays and different source filters. Companion `main.cpp` selects filesystem and serial interfaces by platform macros. Heltec v3 has USB/BLE/WiFi variants; RAK4631 has USB/BLE, display, sensors, ExtraFS, and nRF52 SoftDevice constraints.
- **Prevention:** Keep bot logic in platform-neutral companion C++ with no direct ESP32/nRF calls. Put target-specific storage, display, BLE, and WiFi behavior behind existing `MyMesh`, `DataStore`, and serial-interface abstractions. CI must build at least Heltec v3 USB/BLE and RAK4631 USB/BLE before any feature is considered portable; later expand to all companion suffixes.
- **Severity:** CRITICAL
- **Phase relevance:** Build system and portability implementation.
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/MeshCore/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/main.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/variants/heltec_v3/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/variants/rak4631/platformio.ini`
- **Checked:** 2026-05-14

### ITEM-pitfalls-18: Private key export/import remains enabled in bot firmware builds

- **What goes wrong:** A deployed bot node exposes identity private-key export/import over the companion protocol, increasing damage from a compromised host, BLE session, or physical access. Known-bot trust and suppression can then be subverted by cloned identities.
- **Root cause:** The upstream base build flags in the local checkout define `ENABLE_PRIVATE_KEY_IMPORT=1` and `ENABLE_PRIVATE_KEY_EXPORT=1` with a comment warning to disable them for more secure firmware. Bot identity becomes more valuable once other nodes trust known bot public keys for suppression and emergency behavior.
- **Prevention:** Disable private key export/import in production bot firmware unless a deliberate provisioning workflow requires it. If identity migration is needed, provide a separate provisioning build or physical-button-gated window. Known-bot identity lists should assume keys are long-lived and protected.
- **Severity:** CRITICAL
- **Phase relevance:** Release build flags and provisioning.
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/MeshCore/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`
- **Checked:** 2026-05-14

## Confidence Summary

| Item ID | Level | Source Type | URL/Reference |
|---------|-------|-------------|---------------|
| ITEM-pitfalls-1 | HIGH | Local code + official hardware docs + release assets | `/Users/cjvana/Documents/GitHub/MeshCore/variants/rak4631/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/boards/nrf52840_s140_v6_extrafs.ld`; https://heltec.org/project/wifi-lora-32-v3/ ; https://docs.rakwireless.com/product-categories/wisblock/rak4631/overview/ ; https://github.com/meshcore-dev/MeshCore/releases/expanded_assets/companion-v1.15.0 |
| ITEM-pitfalls-2 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/meshcore-bot/requirements.txt`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/commands/*` |
| ITEM-pitfalls-3 | HIGH | Local code + WebSearch | `/Users/cjvana/Documents/GitHub/MeshCore/README.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/StaticPoolPacketManager.cpp`; https://cpp4arduino.com/2018/11/06/what-is-heap-fragmentation.html |
| ITEM-pitfalls-4 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/StaticPoolPacketManager.cpp` |
| ITEM-pitfalls-5 | HIGH | Local code + official docs | `/Users/cjvana/Documents/GitHub/MeshCore/src/MeshCore.h`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.h`; https://docs.meshcore.io/companion_protocol/ |
| ITEM-pitfalls-6 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/security_utils.py` |
| ITEM-pitfalls-7 | HIGH | Local code + official docs | `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp`; https://docs.meshcore.io/companion_protocol/ |
| ITEM-pitfalls-8 | HIGH | Project requirement + local code | `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp` |
| ITEM-pitfalls-9 | HIGH | Local code + ecosystem docs/search | `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/SimpleMeshTables.h`; https://meshtastic.org/docs/overview/mesh-algo/ ; https://scholarworks.gnu.ac.kr/item/ccb29254-804b-413d-adc5-853fe697241b |
| ITEM-pitfalls-10 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/MeshCore/src/Packet.h`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/SimpleMeshTables.h` |
| ITEM-pitfalls-11 | HIGH | Local code/docs | `/Users/cjvana/Documents/GitHub/MeshCore/docs/payloads.md`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp` |
| ITEM-pitfalls-12 | HIGH | Local code + official docs | `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/main.cpp`; https://docs.meshcore.io/companion_protocol/ |
| ITEM-pitfalls-13 | HIGH | Local code + official docs | `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.h`; https://docs.meshcore.io/companion_protocol/ |
| ITEM-pitfalls-14 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/MeshCore/src/Mesh.h`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/message_handler.py` |
| ITEM-pitfalls-15 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/DataStore.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/NodePrefs.h` |
| ITEM-pitfalls-16 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/NodePrefs.h`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/DataStore.cpp` |
| ITEM-pitfalls-17 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/MeshCore/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/main.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/variants/heltec_v3/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/variants/rak4631/platformio.ini` |
| ITEM-pitfalls-18 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/MeshCore/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp` |
