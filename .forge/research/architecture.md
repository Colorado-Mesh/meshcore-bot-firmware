# Architecture Research: Firmware-only MeshCore Bot

Project date: 2026-05-14
Mode: architecture
Scope: firmware-only MeshCore bot integrated into MeshCore companion firmware. This replaces stale coordinator-first/host-bot conclusions with an embedded command-handling architecture using upstream MeshCore as submodule/patch base and `meshcore-bot` as behavioral reference only.

### ITEM-architecture-1: Put the firmware bot at the companion application layer

- **Recommendation:** Implement the bot as a `FirmwareBot`/`BotRuntime` owned by `examples/companion_radio/MyMesh`, invoked from `onMessageRecv()`, `onSignedMessageRecv()`, `onChannelMessageRecv()`, `onControlDataRecv()`, outbound send completion/timeout paths, and `MyMesh::loop()`. Keep `mesh::Dispatcher`, `mesh::Mesh::routeRecvPacket()`, ACKs, packet duplicate tables, path returns, and retransmit timing untouched.
- **Rationale:** MeshCore already separates radio scheduling (`Dispatcher`), packet parsing/dedup/routing (`Mesh`), chat abstractions (`BaseChatMesh`), and companion UX/protocol (`MyMesh`). Bot commands are application behavior, not mesh forwarding behavior. Placing the bot in `MyMesh` gives it decrypted DM/channel text, channel index/name, ContactInfo for DMs, SNR/path metadata, storage access, and send APIs without changing core behavior for repeaters, room servers, sensors, or stock companion clients.
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/src/Mesh.cpp`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not implement bot election inside `Dispatcher` or `Mesh::routeRecvPacket()`: that would couple command policy to every packet role. Do not keep a Python/VPS runtime in the critical path: the project pivot is firmware-only and off-grid.

### ITEM-architecture-2: Split the bot into policy, classification, execution, and coordination components

- **Recommendation:** Structure the embedded bot as four small modules: `BotPolicy` (DM/channel allowlist, banned users, known bots, emergency rules), `CommandClassifier` (normalization and command/keyword match), `CommandExecutor` (fixed C++ command handlers and response formatting), and `ResponseCoordinator` (delayed send/suppression state). `MyMesh` should be only the adapter that turns MeshCore callbacks into `BotMessage` inputs and sends approved `BotResponse` outputs.
- **Rationale:** The standard Python bot combines config, channel filtering, command lookup, external APIs, response formatting, and sending in host code. Firmware needs the same observable decisions but cannot afford a plugin framework or external data dependencies. Separate modules keep firmware testable and keep MeshCore patch hooks minimal: `MyMesh` observes messages, `BotRuntime` decides, then `MyMesh` sends by existing `sendMessage()`/`sendGroupMessage()`.
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/message_handler.py`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/command_manager.py`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not port Python plugins or SQLite/web/API features into firmware. Do not put channel policy inside each command handler; emergency/Public rules must be enforced centrally before execution.

### ITEM-architecture-3: Use strict channel policy: DMs, #bot, #testing, and emergency-only Public forwarding

- **Recommendation:** Default `BotPolicy` should allow normal bot command handling only for DMs, `#bot`, and `#testing`. It should ignore normal `Public` traffic entirely. Messages on `#emergency` should not run the normal command set; instead they should trigger a dedicated emergency-forward path to Public. Store allowlisted channel names as fixed strings or channel indexes resolved at boot, and re-resolve after `CMD_SET_CHANNEL`/channel load.
- **Rationale:** The project explicitly wants normal bot traffic off Public but wants emergency announcements routed to Public. MeshCore group messages are received by channel secret/hash and mapped to `ChannelDetails` in `BaseChatMesh`; `MyMesh` already has `findChannelIdx()` and `getChannel()` to get channel name. Central channel policy prevents a command override from accidentally enabling noisy public replies.
- **Confidence:** HIGH
- **Source:** Project requirements — `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; Local code — `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not copy the Python default `monitor_channels = general,test,emergency`; it contradicts the new scope. Do not use substring channel matching; use exact normalized channel names or resolved channel indexes to avoid `#botnet`/`#testing2` mistakes.

### ITEM-architecture-4: Forward #emergency to Public with a LoRa-native two-stage formatter

- **Recommendation:** Implement `EmergencyForwarder` as a special `BotRuntime` path: when a valid `#emergency` channel message arrives, send to Public as either one packet if it fits (`EMERGENCY MESSAGE FROM <user>: <original text>`) or two ordered Public packets when needed: `EMERGENCY MESSAGE FROM <user>` followed by the original message text truncated to MeshCore’s single-message limit with an explicit truncation marker if necessary. Deduplicate by emergency fingerprint for a short TTL so multiple bots do not all forward the same emergency.
- **Rationale:** `BaseChatMesh::sendGroupMessage()` prepends `<node_name>: ` and clamps total group text to `MAX_TEXT_LEN` (160 bytes in the local source). MeshCore packet payload is capped at `MAX_PACKET_PAYLOAD` 184 bytes. The emergency header consumes payload budget; forcing everything into one packet can silently truncate the important text. A two-stage formatter preserves the required phrase and the original text better while staying LoRa-native. Public is preconfigured in current `MyMesh::begin()`, but saved channel config can override indexes, so Public should be found by name/secret rather than assumed to be index 0.
- **Confidence:** HIGH
- **Source:** Project requirements + Local code — `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/docs/packet_structure.md`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not let `#emergency` execute normal bot commands. Do not forward to Public without deduplication. Do not send unlimited multipart emergency text in v1; each extra packet increases congestion during emergencies.

### ITEM-architecture-5: Fingerprint requests from decrypted semantics, not packet bytes alone

- **Recommendation:** Define `BotMessageFingerprint` as 64 bits from SHA-256 over stable application fields: message kind (DM/channel/emergency), channel name or DM peer key prefix, sender identity where available, sender timestamp, normalized command text, and payload type. Keep RF packet hash/path/SNR as metadata only.
- **Rationale:** MeshCore already deduplicates raw packets, but semantically identical bot requests can arrive through different RF envelopes, and independently generated bot responses will never share a packet hash. The Python bot/coordinator concepts key on logical message identity, while MeshCore packet hash is designed for route duplicate suppression. Firmware needs both: semantic fingerprint for bot suppression and raw packet/path metadata for diagnostics and scoring.
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/SimpleMeshTables.h`; `/Users/cjvana/Documents/GitHub/MeshCore/src/Mesh.cpp`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/message_handler.py`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not key by text alone; two users can send `ping` simultaneously. Do not key only by `Packet::calculatePacketHash()`; the same command-level event may be represented differently at RF/path layers.

### ITEM-architecture-6: Coordinate with a bounded passive listen-before-answer FSM

- **Recommendation:** Implement `ResponseCoordinator` as fixed-size state tables and a run-to-completion FSM: `OBSERVED -> ELIGIBLE -> PENDING_SELF -> SENT_SELF | SUPPRESSED_BY_KNOWN_BOT | EXPIRED | FAILED`. Events are `message_seen`, `score_ready`, `timer_due`, `known_bot_response_seen`, `known_claim_seen` (future), `send_ok`, `send_failed`, and `ttl_expired`. Default behavior should be passive: compute a score, schedule a response in a 300-3500 ms window, and cancel if a trusted known-bot response/claim for the same fingerprint arrives first.
- **Rationale:** The project explicitly prefers separate bot delays and passive suppression before explicit claim frames. MeshCore’s packet manager already supports scheduled outbound packets, but command execution and suppression should remain app-level so ACK/path behavior is not suppressed. A fixed FSM avoids blocking `loop()`, serial/BLE handling, radio RX, and flash writes.
- **Confidence:** HIGH
- **Source:** Project requirements + Local code — `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/StaticPoolPacketManager.cpp`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not call `delay()` before responding. Do not allocate per-message heap objects. Do not start with mandatory claim/election packets; they may add more airtime than they save.

### ITEM-architecture-7: Trust known bot identities explicitly; do not trust arbitrary group text

- **Recommendation:** Add a persisted `KnownBotRegistry` keyed by full 32-byte public keys plus optional display name and capability flags. Suppression should be authoritative only when the competing bot identity can be validated: DM/control data tied to a known key, signed coordination metadata, or a future signed group-data/claim frame. For plain `GRP_TXT`, treat matching known bot names as a weak hint for low-risk `#bot`/`#testing` only, and never let unauthenticated group text suppress emergency forwarding or safety-critical behavior.
- **Rationale:** MeshCore docs and local code identify group text as unverified: it is encrypted to the channel but the sender name is just text in the group payload. A malicious or mistaken node can spoof `KnownBot: ping`. The user requires trusting known bot identities only, which means the firmware needs explicit key-based trust. Passive suppression can still work for normal channels, but the architecture must not pretend group text alone authenticates a bot.
- **Confidence:** HIGH
- **Source:** Local docs/code — `/Users/cjvana/Documents/GitHub/MeshCore/docs/packet_structure.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/src/Packet.h`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not trust node names, response prefixes, or arbitrary control payloads as identities. Do not use unauthenticated suppression for `#emergency`.

### ITEM-architecture-8: Keep v1 commands small and deterministic

- **Recommendation:** Ship only firmware-suitable commands first: `ping`, `test/t`, `help`, `status`, `about`, `channels`, minimal `path`/routing summary from available packet metadata, and DM-only `advert`. Keep weather, AQI, sports, satpass, web viewer, Discord, SQLite stats, and external API commands out of firmware unless a board-specific host/network adapter is later added.
- **Rationale:** Representative targets include RAK4631/nRF52840 and Heltec v3/ESP32-S3. External data commands in `meshcore-bot` depend on Python, HTTP APIs, local databases, or host services. A firmware-only bot should prioritize reliable off-grid responses, not feature parity. The Python bot remains the behavioral reference for command words, rate limits, channel filtering, and response style.
- **Confidence:** HIGH
- **Source:** Local code/config — `/Users/cjvana/Documents/GitHub/meshcore-bot/config.ini.example`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/message_handler.py`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/commands`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not embed Python, JavaScript, SQLite, TLS HTTP clients, or large plugin registries. Do not claim full `meshcore-bot` command parity in firmware v1.

### ITEM-architecture-9: Persist bot settings in a versioned BotPrefs file and expose protocol plus rescue CLI settings

- **Recommendation:** Add a small versioned `BotPrefs` persisted separately from `NodePrefs`, e.g. `/bot_prefs_v1`, with: enabled/passive, tier, base/max delay, jitter, response cooldowns, allowed channel names/indexes, emergency source/destination channel names, known bot public-key slots, command enable bitset, and stats reset counter. Expose companion protocol commands such as `CMD_GET_BOT_CONFIG`, `CMD_SET_BOT_CONFIG`, `CMD_GET_BOT_STATS`, and `CMD_SET_KNOWN_BOT` in an unused/new range gated by firmware version/capability. Add rescue CLI commands for minimal field recovery: `bot enable|disable`, `bot tier`, `bot channels`, `bot known add <64hex>`, and `bot stats`.
- **Rationale:** Existing `NodePrefs` serialization is manually offset-based; changing it directly risks compatibility mistakes. A separate versioned file isolates bot migration and can be ignored by stock clients. The companion protocol already uses binary commands, async push notifications, response codes, and version/response-length gating; CLI rescue currently supports simple field maintenance when apps are unavailable.
- **Confidence:** HIGH
- **Source:** Local code + official docs — `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/NodePrefs.h`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/DataStore.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; https://docs.meshcore.io/companion_protocol/
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not append fields to `NodePrefs` without a migration/version plan. Do not make all bot config compile-time only; Colorado Mesh deployments need field tuning. Do not require a host app to recover from a bad bot setting.

### ITEM-architecture-10: Design RAM/storage budgets around RAK4631 first, not ESP32 first

- **Recommendation:** Budget the firmware bot for the tight representative target, RAK4631: keep runtime bot RAM under 4 KB in v1 and filesystem bot storage under 4 KB. A concrete target design is: 12 pending coordinator entries at 64 bytes (768 B), 24 recent fingerprint entries at 16 bytes (384 B), 16 known bots at 40 bytes (640 B RAM if loaded; ~640 B persisted), stats/counters under 256 B, and one shared 160-byte response scratch buffer reused from bot runtime. Keep command response strings in flash/PROGMEM where supported.
- **Rationale:** RAK4631 is nRF52840 with 1 MB flash and 256 KB RAM; local linker for RAK companion with extra FS exposes ~712,704 bytes executable flash and ~237,568 bytes RAM after SoftDevice reservation. Heltec v3 has much more flash (8 MB) and ESP32-S3 SRAM (512 KB), so RAK4631 is the limiting design point. Existing companion firmware already reserves large static tables: `MAX_CONTACTS=350`, `MAX_GROUP_CHANNELS=40`, offline queue up to 256 on BLE builds, packet pools, contacts, channels, and advert blobs. Bot state must stay compact.
- **Confidence:** HIGH
- **Source:** Official hardware docs + local code — https://docs.rakwireless.com/product-categories/wisblock/rak4631/overview/ ; https://documentation.espressif.com/esp32-s3_datasheet_en.pdf ; `/Users/cjvana/Documents/GitHub/MeshCore/variants/rak4631/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/boards/nrf52840_s140_v6_extrafs.ld`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.h`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not size the feature using Heltec v3 headroom. Do not add persistent message history, SQLite-like logs, or heap-heavy registries on-device. Do not increase `OFFLINE_QUEUE_SIZE`/packet pool to hide bot backpressure.

### ITEM-architecture-11: Firmware bot flash/storage estimate is feasible if kept to a compact C++ core

- **Recommendation:** Treat v1 firmware bot overhead as feasible but require measurement in CI. Expected incremental resources for the compact design are approximately 20-60 KB flash/rodata, 2-4 KB RAM, and 1-4 KB filesystem storage. Compare that against representative devices: Heltec v3 has 8 MB flash and 512 KB ESP32-S3 SRAM; RAK4631 has 1 MB flash/256 KB RAM, with the local RAK companion executable limit set to 712,704 bytes. Existing v1.15 release assets show Heltec v3 merged images around 695 KB (USB) to 1.33 MB (BLE, includes bootloader/partition gaps) and RAK4631 UF2 assets around 955-971 KB (UF2 encoding, not raw app size); local RAK linker limit remains the useful compile-time constraint.
- **Rationale:** The bot’s proposed static tables are small compared with existing contact/channel/offline queue tables. The bigger risk is flash growth from careless libraries, string-heavy commands, debug logs, or enabling ESP32 WiFi/OTA-style dependencies globally. A compact command set and no dynamic plugin system should fit RAK4631, but exact headroom cannot be asserted until PlatformIO builds produce `.text/.data/.bss` reports for Heltec v3 and RAK4631.
- **Confidence:** MEDIUM
- **Source:** Official release/hardware docs + local config + asset HEAD check — https://github.com/meshcore-dev/MeshCore/releases/tag/companion-v1.15.0 ; https://docs.heltec.cn/en/node/esp32/wifi_lora_32/index.html ; https://docs.rakwireless.com/product-categories/wisblock/rak4631/overview/ ; `/Users/cjvana/Documents/GitHub/MeshCore/variants/heltec_v3/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/variants/rak4631/platformio.ini`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not rely on GitHub asset file size as exact app flash size, especially UF2 and merged ESP32 binaries. Do not add external API clients or large command data tables until actual build reports show headroom.

### ITEM-architecture-12: Use scoring as a delay input, not lower-layer MeshCore timing knobs

- **Recommendation:** Compute a per-message bot score from command eligibility, channel priority, directness/path length, recent heard path, SNR only for direct/zero-hop observations, tier preference, queue health, and deterministic tiebreaker from known bot key/fingerprint. Convert score into a coordinator delay with bounded jitter. Do not repurpose MeshCore `txdelay`, `direct.txdelay`, or `rxdelay` as bot election controls.
- **Rationale:** MeshCore lower-layer timing affects flood/direct retransmit and receive processing, not application-level “which bot answers.” The project already decided to use bot-specific coordinator delays and suppression windows. Keeping this separate avoids breaking path discovery, ACK behavior, and repeater fairness.
- **Confidence:** HIGH
- **Source:** Project requirements + local code — `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/Dispatcher.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/CommonCLI.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/simple_repeater/MyMesh.cpp`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not use topology tier as a fixed “winner.” Do not configure values outside existing MeshCore validation ranges just to influence bot replies.

### ITEM-architecture-13: Instrument duplicate suppression and storage headroom from day one

- **Recommendation:** Add `BotStats` counters and a stats protocol subtype: observed eligible messages, commands executed, emergency forwards, pending scheduled, self sent, suppressed by known response, suppressed by signed claim, suppressed by weak group hint, expired, failed sends, duplicate emergency fingerprints, outbound queue high-water mark, packet pool failures, free heap/stack where available, bot RAM config size, and DataStore used/total KB.
- **Rationale:** The project goal is to reduce duplicate bot traffic without excessive latency. Existing companion firmware already exposes core/radio/packet stats and storage totals. Bot-specific counters are necessary to prove the firmware coordinator helps and to catch false suppression, queue exhaustion, or storage pressure on RAK4631.
- **Confidence:** HIGH
- **Source:** Local code + official docs — `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/DataStore.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/docs/stats_binary_frames.md`; https://docs.meshcore.io/companion_protocol/
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not tune delay windows by anecdote. Do not only count transmissions; false suppression and failed emergency forwarding are higher-risk metrics.

### ITEM-architecture-14: Preserve upstream maintainability with a submodule plus small patch series

- **Recommendation:** Structure the Colorado Mesh project as a wrapper repository with upstream MeshCore pinned as a git submodule (for example `vendor/MeshCore`) plus a versioned patch series (`patches/meshcore/*.patch`) and optional overlay files. CI should initialize the submodule, verify the upstream commit/tag, apply patches cleanly, build representative environments, and fail if patches no longer apply. Keep patches small and upstream-shaped: one for bot source files, one for `MyMesh` hooks/protocol constants, one for DataStore/BotPrefs, one for build flags/CI if needed.
- **Rationale:** The user chose upstream MeshCore as a submodule/patch base. PlatformIO source filters and example paths make a pure out-of-tree plugin hard, so the maintainable compromise is a clean patch stack over upstream rather than a drifting vendored copy. This preserves a clear diff for eventual upstream discussion while allowing Colorado-specific bot policy to stay outside upstream until proven.
- **Confidence:** HIGH
- **Source:** Project requirements + local code — `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/variants/heltec_v3/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/variants/rak4631/platformio.ini`; https://github.com/meshcore-dev/MeshCore/releases/tag/companion-v1.15.0
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not maintain a permanent copy of MeshCore with unstructured edits. Do not create board-specific forks. Do not make the wrapper call into files outside the MeshCore tree if PlatformIO source filters cannot reliably include them across all environments.

## Confidence Summary

| Item ID | Level | Source Type | URL/Reference |
|---------|-------|-------------|---------------|
| ITEM-architecture-1 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/src/Mesh.cpp` |
| ITEM-architecture-2 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/message_handler.py`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/command_manager.py`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp` |
| ITEM-architecture-3 | HIGH | Project requirements + local code | `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp` |
| ITEM-architecture-4 | HIGH | Project requirements + local code docs | `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/docs/packet_structure.md` |
| ITEM-architecture-5 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/SimpleMeshTables.h`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/message_handler.py` |
| ITEM-architecture-6 | HIGH | Project requirements + local code | `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/StaticPoolPacketManager.cpp` |
| ITEM-architecture-7 | HIGH | Local docs/code | `/Users/cjvana/Documents/GitHub/MeshCore/docs/packet_structure.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp` |
| ITEM-architecture-8 | HIGH | Local bot code/config | `/Users/cjvana/Documents/GitHub/meshcore-bot/config.ini.example`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/commands` |
| ITEM-architecture-9 | HIGH | Local code + official docs | https://docs.meshcore.io/companion_protocol/ ; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/DataStore.cpp` |
| ITEM-architecture-10 | HIGH | Hardware docs + local linker/config | https://docs.rakwireless.com/product-categories/wisblock/rak4631/overview/ ; https://documentation.espressif.com/esp32-s3_datasheet_en.pdf ; `/Users/cjvana/Documents/GitHub/MeshCore/boards/nrf52840_s140_v6_extrafs.ld` |
| ITEM-architecture-11 | MEDIUM | Release/hardware docs + local config + asset HEAD check | https://github.com/meshcore-dev/MeshCore/releases/tag/companion-v1.15.0 ; https://docs.heltec.cn/en/node/esp32/wifi_lora_32/index.html ; https://docs.rakwireless.com/product-categories/wisblock/rak4631/overview/ |
| ITEM-architecture-12 | HIGH | Project requirements + local code | `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/Dispatcher.cpp` |
| ITEM-architecture-13 | HIGH | Local code + official docs | `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/DataStore.cpp`; https://docs.meshcore.io/companion_protocol/ |
| ITEM-architecture-14 | HIGH | Project requirements + local code + official release | `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/platformio.ini`; https://github.com/meshcore-dev/MeshCore/releases/tag/companion-v1.15.0 |
