# Stack Research: Firmware-only MeshCore Bot

Checked: 2026-05-14

### ITEM-stack-1: Build in upstream MeshCore's Arduino C++/PlatformIO firmware stack

- **Recommendation:** Build the firmware-only bot in C++ inside the MeshCore companion firmware stack, using Arduino framework and PlatformIO exactly as upstream does. Keep `meshcore-bot` as behavioral reference only.
- **Rationale:** Current MeshCore describes itself as a compact C++ embedded LoRa/packet-radio library, and current upstream `platformio.ini` uses `framework = arduino` with architecture bases for ESP32, nRF52, RP2040, and STM32. The Python `meshcore-bot` connects over serial/BLE/TCP and implements a plugin bot outside the radio; it is useful for command semantics, but not for reducing firmware advert/response behavior on-device.
- **Confidence:** HIGH
- **Source:** Official docs + local code — https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/README.md; https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/platformio.ini; `/Users/cjvana/Documents/GitHub/meshcore-bot/README.md`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not introduce ESP-IDF-only, Zephyr, Rust, embedded Python/JS, Docker, or a VPS-side coordinator as the product stack; they either break MeshCore's cross-device firmware model or preserve the external-bot architecture being replaced.

### ITEM-stack-2: Keep MeshCore as an upstream submodule plus Colorado overlay/patch queue

- **Recommendation:** Structure the Colorado Mesh repo as a wrapper with `upstream/MeshCore` as a pinned git submodule, a `colorado/` overlay for new source files/config snippets/tests, and a deterministic `patches/` or `git-format-patch` queue applied to the submodule during build. Do not keep local edits directly inside the submodule as the only source of truth.
- **Rationale:** The user has selected submodule/patch-base strategy. MeshCore's PlatformIO project expects builds from the MeshCore repo root (`extra_configs = variants/*/platformio.ini`, relative `build_src_filter`, `build.sh`, and variant paths), so the wrapper should stage/apply patches into a worktree and invoke upstream `build.sh` from there. This preserves an auditable upstream pin while still allowing changes across `examples/companion_radio`, `src/helpers`, `docs`, and selected variant `.ini` files.
- **Confidence:** HIGH
- **Source:** Local code + project decision — `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/build.sh`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not vendor-copy MeshCore into the Colorado repo; upstream is moving. Do not rely on uncommitted submodule edits; they make rebuilds and upstream syncs non-reproducible. Do not place a separate PlatformIO project above MeshCore unless it merely orchestrates the upstream-root build.

### ITEM-stack-3: Use current MeshCore main as the baseline, then pin it

- **Recommendation:** Initialize the submodule from current `meshcore-dev/MeshCore` main, record the exact commit SHA in the Colorado repo, and refresh intentionally. Validate against the local checkout only as read-only reconnaissance.
- **Rationale:** The local MeshCore checkout is usable for inspection, but current upstream has evolved: current upstream `platformio.ini` has LoRa defaults `LORA_FREQ=869.618`, `LORA_BW=62.5`, `LORA_SF=8`, adds `-D ESP32_PLATFORM`, and nRF52 extra scripts include `create-uf2.py` and `patch_bluefruit.py`, while the local checkout differs. Starting from current upstream reduces protocol/build drift.
- **Confidence:** MEDIUM
- **Source:** Official source + local git — https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/platformio.ini; `/Users/cjvana/Documents/GitHub/MeshCore` commit `6b52fb32`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not base implementation solely on the stale local tree; it may miss current companion protocol and build changes. Do not float the submodule without a pinned commit; firmware builds need reproducibility.

### ITEM-stack-4: Preserve upstream dependency/platform pins for the first bot build

- **Recommendation:** Keep upstream platform and library pins initially: `platformio/espressif32@6.11.0` for ESP32 Arduino builds, `nordicnrf52` with `framework-arduinoadafruitnrf52 @ 1.10700.0` for nRF52, RadioLib `^7.3.0`, Crypto `^0.4.0`, RTClib `^2.1.3`, Melopero RV3028 `^1.1.0`, CayenneLPP `1.6.1`, and `densaugeo/base64 @ ~1.4.0` in companion targets.
- **Rationale:** The first risk is firmware behavior, not dependency modernization. `platformio/espressif32@6.11.0` with Arduino uses Arduino-ESP32 2.0.17, and RadioLib 7.3.0 is a 2025 release although newer RadioLib releases exist. Upgrading radio/platform dependencies while adding bot logic would confound failures across ESP32 and nRF52.
- **Confidence:** HIGH
- **Source:** Official docs/search + upstream source — https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/platformio.ini; https://github.com/platformio/platform-espressif32/blob/v6.11.0/platform.json; https://github.com/jgromes/RadioLib/releases/tag/7.3.0; https://docs.platformio.org/en/latest/librarymanager/dependencies.html
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not upgrade to Arduino-ESP32 3.x, latest RadioLib, or custom nRF framework packages until baseline bot patches build and run; dependency churn should be a separate phase.

### ITEM-stack-5: Add bot logic as small fixed-storage companion helpers

- **Recommendation:** Implement the bot as small C++ helper classes called by `examples/companion_radio/MyMesh`, with fixed-size arrays/ring buffers and compile-time feature flags such as `CMESH_BOT_ENABLED`, `CMESH_BOT_MAX_PENDING`, and `CMESH_BOT_MAX_KNOWN_BOTS`.
- **Rationale:** Companion `MyMesh` already owns the required hooks: private DM receive (`onMessageRecv`), group channel receive (`onChannelMessageRecv`), control/raw receive, channel lookup, contact lookup, and group/direct send (`sendGroupMessage`, `sendMessage`). Upstream contribution guidance says to keep embedded code concise and avoid dynamic allocation except during setup/begin. A helper object avoids a large framework while keeping Colorado logic reviewable.
- **Confidence:** HIGH
- **Source:** Local code + upstream README — `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.h`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.h`; https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/README.md
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not port the Python plugin loader, SQLite-backed stats, API clients, or dynamic command registry to firmware. Do not create a general embedded scripting runtime.

### ITEM-stack-6: Handle only firmware-feasible bot behavior in phase 1

- **Recommendation:** Phase 1 should implement deterministic local commands and routing policy: `ping`, `test`, compact `help`, `cmd`, simple `dice/roll`, path/channel echo where already available, passive suppression/listen-before-answer, #bot/#testing routing, private DM replies, and #emergency forwarding to Public. Defer weather, AQI, satellite, sports, jokes from web APIs, repeater database management, and web viewer features.
- **Rationale:** `meshcore-bot` includes many network/API/database features that assume Python, internet, filesystem logs, and async plugins. Firmware bot value is local decentralized operation and duplicate suppression. Keeping responses short also fits MeshCore `MAX_TEXT_LEN = 10*CIPHER_BLOCK_SIZE = 160` bytes and group messages include a sender-name prefix.
- **Confidence:** HIGH
- **Source:** Local code/docs — `/Users/cjvana/Documents/GitHub/meshcore-bot/README.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.h`; `/Users/cjvana/Documents/GitHub/MeshCore/docs/payloads.md`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not attempt feature parity with host-side `meshcore-bot`; API-backed commands are not off-grid and would expand flash/RAM/storage unnecessarily.

### ITEM-stack-7: Use MeshCore group/private message APIs for bot traffic, not new transport timing knobs

- **Recommendation:** Send normal bot replies through existing `sendMessage`/`sendGroupMessage` paths and implement bot-specific delay/suppression above the radio layer. Use configured channel names/hash lookup to restrict normal traffic to private DMs, #bot, and #testing, and special-case #emergency to publish an `EMERGENCY MESSAGE FROM <user>` announcement to Public.
- **Rationale:** MeshCore already parses private text and group text into `onMessageRecv`/`onChannelMessageRecv`, and packet docs define group text as encrypted channel payload with `<sender name>: <message body>`. The project decision explicitly says not to repurpose lower-layer MeshCore `txdelay`, `direct.txdelay`, or `rxdelay` as bot election controls.
- **Confidence:** HIGH
- **Source:** Local code/docs + project decision — `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/docs/payloads.md`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not route normal bot chatter on Public. Do not use lower-layer TX/RX timing knobs as the primary coordinator; it risks affecting mesh behavior beyond bot replies.

### ITEM-stack-8: Build representative firmware with upstream build.sh before expanding

- **Recommendation:** Use upstream `build.sh` from inside the patched MeshCore worktree. During development, build only `Heltec_v3_companion_radio_usb`, `Heltec_v3_companion_radio_ble`, `RAK_4631_companion_radio_usb`, and `RAK_4631_companion_radio_ble`; after those pass, run `build.sh build-companion-firmwares` in CI.
- **Rationale:** Upstream `build.sh build-firmware <env>` injects firmware version/build date, runs `pio run -e`, creates merged ESP32 `.bin` images, creates nRF52 `.uf2`, and writes outputs to `out/`. The representative set covers ESP32-S3 and nRF52840 plus USB/BLE companion variants without paying the cost of every supported board during inner-loop development.
- **Confidence:** HIGH
- **Source:** Local CI/build scripts — `/Users/cjvana/Documents/GitHub/MeshCore/build.sh`; `/Users/cjvana/Documents/GitHub/MeshCore/.github/workflows/build-companion-firmwares.yml`; `/Users/cjvana/Documents/GitHub/MeshCore/.github/actions/setup-build-environment/action.yml`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not hand-maintain an independent board matrix first; upstream environment suffixes and artifact logic already encode release behavior. Do not wait for all companion boards before validating the two requested representatives.

### ITEM-stack-9: Heltec v3 companion target is comfortably feasible

- **Recommendation:** Treat Heltec v3 as the easiest first target. Use current upstream `Heltec_v3_companion_radio_usb` and `Heltec_v3_companion_radio_ble` envs unchanged except for the bot feature flag/overlay patch.
- **Rationale:** Current upstream Heltec v3 extends `Heltec_lora32_v3`, uses board `esp32-s3-devkitc-1`, ESP32-S3 Arduino, SSD1306 UI, `MAX_CONTACTS=350`, `MAX_GROUP_CHANNELS=40`, and BLE adds `OFFLINE_QUEUE_SIZE=256`. PlatformIO's `esp32-s3-devkitc-1` board uses 8 MB flash with `default_8MB.csv`; app0/app1 are 0x330000 each (3,342,336 bytes), SPIFFS is 0x180000 (1,572,864 bytes). Heltec official specs confirm ESP32-S3FN8/ESP32-S3N8 with 8 MB flash and no PSRAM.
- **Confidence:** HIGH
- **Source:** Official source/docs — https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/variants/heltec_v3/platformio.ini; https://raw.githubusercontent.com/platformio/platform-espressif32/master/boards/esp32-s3-devkitc-1.json; https://raw.githubusercontent.com/espressif/arduino-esp32/master/tools/partitions/default_8MB.csv; https://docs.heltec.cn/en/node/esp32/wifi_lora_32/index.html; https://resource.heltec.cn/download/WiFi_LoRa_32_V3/HTIT-WB32LA_V3.2.pdf
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not optimize for Heltec v3 storage first; its app and filesystem margins are much larger than RAK4631's. Do not depend on PSRAM; Heltec v3 does not provide it.

### ITEM-stack-10: RAK4631 is feasible but is the storage/RAM gatekeeper

- **Recommendation:** Treat RAK4631 BLE companion as the gating target. Keep the firmware bot under roughly 20 KB additional flash and 2 KB additional static RAM until measured builds prove more margin.
- **Rationale:** Current upstream RAK4631 companion USB/BLE extend `rak4631`, use `boards/nrf52840_s140_v6_extrafs.ld`, and explicitly set `board_upload.maximum_size = 712704`. The extra-FS linker gives an app FLASH region of 712,704 bytes and RAM region of 237,568 bytes. RAK's raw nRF52840 has 1 MB flash/256 KB RAM, but SoftDevice, bootloader/settings, and extra filesystem reserve much of that; the companion app region, not raw flash, is the practical limit.
- **Confidence:** HIGH
- **Source:** Official source/docs — https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/variants/rak4631/platformio.ini; https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/boards/rak4631.json; https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/boards/nrf52840_s140_v6_extrafs.ld; https://docs.rakwireless.com/product-categories/wisblock/rak4631/overview/; https://www.nordicsemi.com/Products/nRF52840
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not assume the full 1 MB nRF52840 flash is available to the app. Do not store large response tables or logs in RAK4631 internal flash without measuring filesystem pressure.

### ITEM-stack-11: Firmware bot storage budget is small enough if responses stay static and ephemeral

- **Recommendation:** Budget the phase-1 firmware bot at approximately 10-25 KB flash, 0.5-2 KB RAM, and 0-4 KB persistent storage. Require CI to report binary size deltas for Heltec v3 and RAK4631 on every PR.
- **Rationale:** A minimal command parser, static command table, short static response strings, a small known-bot/suppression table, and 4-8 pending response records fit easily: example estimate is 3-8 KB `.text`, 2-10 KB `.rodata` depending on help text, 512-1536 B `.bss` for suppression state, and <512 B stack per invocation if implemented without large local buffers. MeshCore already carries large state for companion mode: approximate struct sizing from source gives `ContactInfo` ~184 B, `ChannelDetails` ~56 B, offline frame ~173 B, and `Packet` ~260 B; existing `MAX_CONTACTS=350`, `MAX_GROUP_CHANNELS=40`, and BLE `OFFLINE_QUEUE_SIZE=256` dominate RAM more than the bot should. This estimate could not be verified by compiling because `pio` is not installed in the local environment.
- **Confidence:** MEDIUM
- **Source:** Local source analysis — `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/ContactInfo.h`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/ChannelDetails.h`; `/Users/cjvana/Documents/GitHub/MeshCore/src/Packet.h`; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.h`; local command result: `pio: command not found`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not embed full help prose, translation files, jokes, channel databases, repeater databases, web/API clients, or persistent usage stats in firmware phase 1; those could turn a <25 KB feature into a storage problem.

### ITEM-stack-12: Storage feasibility estimate by representative target

- **Recommendation:** Proceed with firmware-only bot if the first implementation stays within the conservative budget below; block or trim features if measured RAK4631 BLE app flash delta exceeds 25 KB or static RAM delta exceeds 2 KB.
- **Rationale:** Heltec v3 has about 3.34 MB per OTA app slot and 1.57 MB SPIFFS under the default 8 MB partition table, so a 10-25 KB bot is negligible (<0.8% of one app slot). RAK4631 companion has a 712,704-byte app region and 237,568-byte RAM region; the same bot is ~1.4-3.5% of the app region and ~0.2-0.9% of RAM. Persistent config should be a few hundred bytes; even a 4 KB settings/log reserve is reasonable but should be optional. The unknown is current baseline binary size, not the expected bot delta, because no local PlatformIO build artifacts exist.
- **Confidence:** MEDIUM
- **Source:** Official docs + local analysis — https://raw.githubusercontent.com/espressif/arduino-esp32/master/tools/partitions/default_8MB.csv; https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/boards/nrf52840_s140_v6_extrafs.ld; https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/variants/rak4631/platformio.ini; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.h`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not declare final binary margin until PlatformIO builds run; use this as feasibility guidance, then replace with measured `.text/.data/.bss` and firmware size deltas.

### ITEM-stack-13: Add a size-reporting CI step before feature growth

- **Recommendation:** Add a wrapper command such as `scripts/build-representative.sh` that applies patches, runs the four representative envs, captures PlatformIO RAM/flash usage output, records artifact byte sizes, and fails on configurable bot-delta thresholds once a baseline is established.
- **Rationale:** The user's explicit question is storage feasibility. PlatformIO normally reports memory usage per environment, but local `pio` is unavailable and there are no existing `.pio` artifacts. CI size reports make the feasibility estimate concrete and keep future feature creep visible, especially on RAK4631 BLE.
- **Confidence:** HIGH
- **Source:** Local build scripts + official PlatformIO behavior — `/Users/cjvana/Documents/GitHub/MeshCore/build.sh`; https://docs.platformio.org/en/latest/core/userguide/cmd_run.html
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not rely on source-only estimates after code exists. Do not run only Heltec v3; RAK4631 is the constrained representative.

### ITEM-stack-14: Use host-side Python only for tests and behavioral fixtures

- **Recommendation:** Keep Python in the repo only for development tooling: generating patch worktrees, golden command-response fixtures, fake MeshCore frames, and CI parsing of size reports. Firmware behavior should not depend on Python at runtime.
- **Rationale:** `meshcore-bot` is rich behavioral prior art: command keywords, rate limits, channel policy, and response wording. But its async plugin loader, HTTP API clients, SQLite/database managers, web viewer, and serial/BLE/TCP connection code cannot run inside companion firmware. Host-side tests can still prevent regressions when translating selected behavior to C++.
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/commands`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/command_manager.py`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/message_handler.py`
- **Checked:** 2026-05-14
- **Alternatives rejected:** Do not make the firmware call out to a local Python service for normal bot responses; that recreates the coordinator dependency.

## Confidence Summary

| Item ID | Level | Source Type | URL/Reference |
|---------|-------|-------------|---------------|
| ITEM-stack-1 | HIGH | Official docs + Local code | https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/README.md; https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/platformio.ini; `/Users/cjvana/Documents/GitHub/meshcore-bot/README.md` |
| ITEM-stack-2 | HIGH | Local code + Project decision | `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/platformio.ini`; `/Users/cjvana/Documents/GitHub/MeshCore/build.sh` |
| ITEM-stack-3 | MEDIUM | Official source + Local git | https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/platformio.ini; `/Users/cjvana/Documents/GitHub/MeshCore` |
| ITEM-stack-4 | HIGH | Official docs/search + Upstream source | https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/platformio.ini; https://github.com/platformio/platform-espressif32/blob/v6.11.0/platform.json; https://github.com/jgromes/RadioLib/releases/tag/7.3.0; https://docs.platformio.org/en/latest/librarymanager/dependencies.html |
| ITEM-stack-5 | HIGH | Local code + Upstream README | `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.h`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.h`; https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/README.md |
| ITEM-stack-6 | HIGH | Local code/docs | `/Users/cjvana/Documents/GitHub/meshcore-bot/README.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.h`; `/Users/cjvana/Documents/GitHub/MeshCore/docs/payloads.md` |
| ITEM-stack-7 | HIGH | Local code/docs + Project decision | `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`; `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/BaseChatMesh.cpp`; `/Users/cjvana/Documents/GitHub/MeshCore/docs/payloads.md` |
| ITEM-stack-8 | HIGH | Local CI/build scripts | `/Users/cjvana/Documents/GitHub/MeshCore/build.sh`; `/Users/cjvana/Documents/GitHub/MeshCore/.github/workflows/build-companion-firmwares.yml` |
| ITEM-stack-9 | HIGH | Official source/docs | https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/variants/heltec_v3/platformio.ini; https://raw.githubusercontent.com/platformio/platform-espressif32/master/boards/esp32-s3-devkitc-1.json; https://raw.githubusercontent.com/espressif/arduino-esp32/master/tools/partitions/default_8MB.csv; https://docs.heltec.cn/en/node/esp32/wifi_lora_32/index.html |
| ITEM-stack-10 | HIGH | Official source/docs | https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/variants/rak4631/platformio.ini; https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/boards/rak4631.json; https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/boards/nrf52840_s140_v6_extrafs.ld; https://docs.rakwireless.com/product-categories/wisblock/rak4631/overview/ |
| ITEM-stack-11 | MEDIUM | Local source analysis | `/Users/cjvana/Documents/GitHub/MeshCore/src/helpers/ContactInfo.h`; `/Users/cjvana/Documents/GitHub/MeshCore/src/Packet.h`; local `pio` availability check |
| ITEM-stack-12 | MEDIUM | Official docs + Local analysis | https://raw.githubusercontent.com/espressif/arduino-esp32/master/tools/partitions/default_8MB.csv; https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/boards/nrf52840_s140_v6_extrafs.ld; https://raw.githubusercontent.com/meshcore-dev/MeshCore/main/variants/rak4631/platformio.ini |
| ITEM-stack-13 | HIGH | Local build scripts + Official docs | `/Users/cjvana/Documents/GitHub/MeshCore/build.sh`; https://docs.platformio.org/en/latest/core/userguide/cmd_run.html |
| ITEM-stack-14 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/commands`; `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/command_manager.py` |
