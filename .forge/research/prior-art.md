# Prior Art Research: Firmware-Resident MeshCore Bot

Checked: 2026-05-14

Scope: firmware-resident bots and embedded command responders in LoRa/mesh systems; existing MeshCore companion firmware, Python MeshCore bot, Colorado Mesh community bot/VPS coordinator patterns, and storage/flash/RAM feasibility for Heltec v3 and RAK4631 representative builds. This replaces stale coordinator-first conclusions: the recommended direction is a firmware-only bot with host-side projects used only as behavioral references.

### ITEM-prior-art-1: Upstream MeshCore companion firmware is the correct patch base

- **URL:** https://github.com/meshcore-dev/MeshCore
- **What it does well:** MeshCore provides the C++ mesh library and firmware examples for Companion, Repeater, Room Server, Sensor, and related roles. The local `examples/companion_radio` implementation already receives DMs and channel messages, queues offline messages, sends contact/channel text, exposes battery/storage reporting, stores contacts/channels/prefs, and has hooks (`onMessageRecv`, `onChannelMessageRecv`) exactly where a firmware bot can process commands before/alongside the serial companion interface. The latest fetched release was Companion Firmware v1.15.0, published 2026-04-19.
- **What it lacks:** No upstream firmware-resident command bot or multi-bot response election layer is visible. Companion firmware is still designed around an external app/host, so bot logic must be added without breaking phone/serial clients or changing the node role into a repeater.
- **What we can learn:** Use upstream MeshCore as a submodule and add Colorado Mesh firmware-bot code as a narrow companion-radio extension. Put command parsing in the receive hooks and response sending through existing MeshCore `sendMessage` / `sendGroupMessage` paths rather than inventing a parallel radio stack.
- **License:** MIT
- **Confidence:** HIGH
- **Source:** WebFetch + local code — https://github.com/meshcore-dev/MeshCore ; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`
- **Checked:** 2026-05-14

### ITEM-prior-art-2: MeshCore companion protocol supports the needed behavior but imposes short-message constraints

- **URL:** https://docs.meshcore.io/companion_protocol/
- **What it does well:** The companion protocol documents channel text send (`0x03`), channel binary datagrams (`0x3E`), queued-message polling (`0x0A`), async message-waiting notices, and battery/storage reporting (`0x14` response includes battery millivolts plus used/total storage KB). It states a text-message limit of 133 characters and a datagram payload limit of 163 bytes.
- **What it lacks:** It exposes used/total storage but no general-purpose arbitrary config/storage API for a bot. It is an app-to-firmware protocol, not an internal bot API, and BLE framing/MTU constraints make verbose responses inappropriate.
- **What we can learn:** Firmware bot responses must be terse and deterministic. Treat 133 characters as the safe response budget. Avoid large JSON/config payloads over chat. For bot configuration, prefer compile-time defaults plus a small firmware-side config structure saved with `DataStore`, not a full Python-style config file.
- **License:** Documentation / N/A
- **Confidence:** HIGH
- **Source:** WebFetch — https://docs.meshcore.io/companion_protocol/
- **Checked:** 2026-05-14

### ITEM-prior-art-3: agessaman meshcore-bot is the behavior oracle, not the runtime model

- **URL:** https://github.com/agessaman/meshcore-bot
- **What it does well:** The Python bot supports serial/BLE/TCP companion connections, configurable keywords, plugin commands, rate limiting, user bans, scheduled messages, DMs, logging, Discord/webhook integrations, weather/AQI/solar/sports/satellite feeds, stats, path diagnostics, repeater management, and web viewer features. Local command modules show a rich command set and a practical channel policy surface (`monitor_channels`, `respond_to_dms`).
- **What it lacks:** It assumes Python, asyncio, SQLite, HTTP/TLS clients, API keys, geocoding, dynamic plugins, and often internet reachability. Those features are unrealistic in first-pass firmware, especially on RAK4631-class nRF52840 targets.
- **What we can learn:** Port semantics in tiers. Firmware MVP should include: `ping`, `help/cmd`, `test`, `hello`, `dice`, `roll`, short `path/status` diagnostics from in-memory packet metadata, DM response support, #bot/#testing channel handling, #emergency-to-Public forwarding, per-command cooldown/rate limiting, passive listen-before-answer suppression, and known-bot trust. Defer: weather, AQI, sports, jokes via HTTP, satellite passes, solar forecast, Discord/web viewer, SQLite stats, dynamic plugins, repeater-management workflows, and feed parsing.
- **License:** MIT
- **Confidence:** HIGH
- **Source:** Local repo — `/Users/cjvana/Documents/GitHub/meshcore-bot/README.md`, `/Users/cjvana/Documents/GitHub/meshcore-bot/modules/commands/*`, `/Users/cjvana/Documents/GitHub/meshcore-bot/requirements.txt`
- **Checked:** 2026-05-14

### ITEM-prior-art-4: Colorado Mesh community bot/VPS coordinator proves the desired coordination semantics

- **URL:** N/A
- **What it does well:** The local and read-only remote community bot patches host-side `send_response` / `send_channel_message` so DMs bypass coordination, channel responses are coordinated exactly once, and coordinator outages fall back to score-based delay. The community scoring model uses hop score, infrastructure/fan-in proxy, exact path familiarity, and freshness. The LAN host runtime config currently uses `monitor_channels = #bot,#emergency` and `emergency_broadcast_channel = Public`, matching the pivoted channel policy.
- **What it lacks:** The VPS coordinator requires IP reachability, registration/auth, PostgreSQL, Python containers, and host-side companion connectivity. It is the opposite of the off-grid firmware-only goal.
- **What we can learn:** Preserve the observable behavior, not the deployment. Firmware should key pending responses by a stable original-message hash, compute a compact local delivery score, schedule a delay, listen for a known bot's response to the same request, and cancel if another trusted bot answers first. DMs should remain immediate because only the addressed bot received them.
- **License:** Private / N/A
- **Confidence:** HIGH
- **Source:** Local repo + read-only SSH — `/Users/cjvana/Documents/GitHub/meshcore-community-bot/docs/COMMUNITY_DESIGN.md`, `/Users/cjvana/Documents/GitHub/meshcore-community-bot/community/message_interceptor.py`, `cj-vps:~/meshcore-community-bot`, `cjvana@10.0.0.222:~/meshcore-community-bot/config.ini`
- **Checked:** 2026-05-14

### ITEM-prior-art-5: Meshtastic firmware modules prove small embedded bot-like features are the norm

- **URL:** https://meshtastic.org/docs/configuration/module/canned-message/
- **What it does well:** Meshtastic ships firmware modules that generate mesh messages from device-side logic. Canned Message sends predefined messages without a phone and limits the combined preset list to 200 bytes. Detection Sensor monitors one GPIO and sends rate-limited mesh alerts. Remote Hardware performs addressed GPIO read/write/watch operations through firmware-controlled request/response behavior.
- **What it lacks:** These are narrow firmware modules, not a general text command bot. Remote Hardware is GPIO-specific and newer firmware may require custom builds; Canned Message needs an input peripheral; Detection Sensor is a one-pin alert module.
- **What we can learn:** The successful embedded pattern is deliberately small: fixed config, bounded text, simple state machines, rate limits, and compile-time/module flags. A MeshCore firmware bot should follow that model instead of embedding a general plugin runtime.
- **License:** Project/documentation / N/A
- **Confidence:** HIGH
- **Source:** WebFetch — https://meshtastic.org/docs/configuration/module/canned-message/ ; https://meshtastic.org/docs/configuration/module/detection-sensor/ ; https://meshtastic.org/docs/configuration/module/remote-hardware/
- **Checked:** 2026-05-14

### ITEM-prior-art-6: disaster.radio shows firmware-resident slash commands are practical on LoRa mesh devices

- **URL:** https://github.com/sudomesh/disaster-radio/blob/master/firmware/src/middleware/Console.cpp
- **What it does well:** The firmware console parses line-based slash commands such as `/help`, `/join`, `/nick`, `/raw`, `/lora`, `/get`, `/set`, and `/restart`. Commands update settings, broadcast info datagrams, restart the device, and use simple tokenization and bounded buffers.
- **What it lacks:** It is not MeshCore, is not a multi-bot responder election system, and has no direct compatibility with MeshCore companion channels or DMs.
- **What we can learn:** Keep the firmware command parser C-style and bounded: copy/terminate the input line, tokenize first word, dispatch to a fixed command table, validate all lengths, and use short replies. This is the right implementation style for `ping/help/test/dice/roll/status` in companion firmware.
- **License:** Repository license not verified here / N/A
- **Confidence:** HIGH
- **Source:** WebFetch — https://github.com/sudomesh/disaster-radio/blob/master/firmware/src/middleware/Console.cpp
- **Checked:** 2026-05-14

### ITEM-prior-art-7: LoRa_APRS_iGate proves embedded automation can be broad, but its heavy features are not MVP guidance

- **URL:** https://github.com/richonguzman/LoRa_APRS_iGate
- **What it does well:** This ESP32/LoRa APRS firmware performs iGate/digipeater automation, beacon scheduling, failover, packet filtering/blacklisting, telemetry, APRS queries, web configuration, OTA, MQTT, and sensor integrations. It targets many LoRa boards, including Heltec variants and RAK4631-class hardware.
- **What it lacks:** It is APRS-oriented, not MeshCore. Many features depend on WiFi/APRS-IS/MQTT/web subsystems and its GPL-3.0 license is not suitable for direct code reuse in an MIT MeshCore patch without a deliberate license strategy.
- **What we can learn:** Embedded LoRa firmware can do meaningful autonomous automation, but the MVP should not copy heavyweight web, MQTT, and internet-service layers. Copy the bounded scheduler/rate-limit/filtering concepts only.
- **License:** GPL-3.0
- **Confidence:** HIGH
- **Source:** WebFetch — https://github.com/richonguzman/LoRa_APRS_iGate
- **Checked:** 2026-05-14

### ITEM-prior-art-8: Cyclenerd meshcore-bot shows the safe minimal host-bot baseline

- **URL:** https://github.com/Cyclenerd/meshcore-bot
- **What it does well:** A small Node.js MeshCore bot over USB serial that responds only in private channels to avoid public-channel spam. It implements simple `.ping` and `.date` commands and can query/log repeater status.
- **What it lacks:** It is host-side, private-channel-only, and does not solve coordinated channel replies. It has no firmware component.
- **What we can learn:** If channel coordination is not ready, the safe fallback is DM-only operation plus explicit #bot/#testing opt-in. The firmware MVP can still be useful if it starts with DMs and restricted channels rather than Public.
- **License:** Apache-2.0
- **Confidence:** HIGH
- **Source:** WebFetch — https://github.com/Cyclenerd/meshcore-bot
- **Checked:** 2026-05-14

### ITEM-prior-art-9: MESH-API/MESH-AI host bots are useful behavior references but too heavy for firmware

- **URL:** https://github.com/mr-tbot/mesh-api
- **What it does well:** These host-side tools bridge Meshtastic/MeshCore, MQTT, Discord, AI/API providers, custom commands, emergency commands, origin tags, duplicate buffers, and randomized command aliases. They explicitly account for loop prevention and multi-system routing.
- **What it lacks:** They require a host runtime, network services, API integrations, and GPL-3.0 licensing. They are not firmware-resident bots or MeshCore companion patches.
- **What we can learn:** Adopt origin markers and duplicate buffers conceptually. Do not port AI/API routes into firmware. A useful firmware response format could include a compact bot marker and original-message hash suffix when needed for suppression/debugging.
- **License:** GPL-3.0 for mesh-api as fetched
- **Confidence:** MEDIUM
- **Source:** WebSearch/WebFetch — https://github.com/mr-tbot/mesh-api ; https://github.com/mr-tbot/mesh-ai
- **Checked:** 2026-05-14

### ITEM-prior-art-10: Existing MeshCore storage model has enough persistent space for small bot config, not host-bot databases

- **URL:** https://github.com/meshcore-dev/MeshCore
- **What it does well:** `DataStore` already persists identity, node prefs, contacts, group channels, and advert blobs; it reports used/total storage in KB. On nRF52 with `EXTRAFS`, companion firmware creates `CustomLFS ExtraFS(0xD4000, 0x19000, 128)`, i.e. about 100 KB of secondary LittleFS-style storage. Contacts are fixed records of about 153 bytes each, channels about 68 bytes each, and advert blobs are fixed bounded records. ESP32 uses SPIFFS and reports `SPIFFS.usedBytes()` / `SPIFFS.totalBytes()`.
- **What it lacks:** There is no existing firmware database analogous to Python SQLite stats, no large config store, and storage competes with contacts/channels/adverts. RAK4631-class nRF52 storage is especially finite.
- **What we can learn:** Store only tiny bot state persistently: enable flag, allowed channel names/indices, a few known-bot public-key prefixes, cooldown settings, and maybe a dozen canned response strings. Keep history/suppression volatile. Do not port Python stats, feed caches, or repeater analytics DB into firmware.
- **License:** MIT
- **Confidence:** HIGH
- **Source:** Local code — `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/DataStore.cpp`, `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp`, `/Users/cjvana/Documents/GitHub/MeshCore/variants/rak4631/platformio.ini`
- **Checked:** 2026-05-14

### ITEM-prior-art-11: Representative device space supports a firmware bot MVP, with RAK4631 as the limiting target

- **URL:** https://docs.rakwireless.com/product-categories/wisblock/rak4631/overview/
- **What it does well:** Heltec WiFi LoRa 32 V3 uses an ESP32-S3N8 with 8 MB integrated flash and SX1262 radio. RAK4631 uses Nordic nRF52840 with 1 MB flash, 256 KB RAM, 64 MHz Cortex-M4, and SX1262. MeshCore `platformio.ini` caps RAK4631 companion uploads at 712,704 bytes. Release asset sizes for MeshCore Companion v1.15.0 were: Heltec v3 USB non-merged 629,424 bytes, Heltec v3 BLE non-merged 1,262,752 bytes, RAK4631 USB UF2 955,392 bytes / ZIP 478,359 bytes, and RAK4631 BLE UF2 971,264 bytes / ZIP 486,343 bytes. UF2/ZIP sizes are packaging sizes, not exact linked ELF flash usage, so exact headroom requires a local build.
- **What it lacks:** I did not run fresh PlatformIO builds or measure patched `.elf` sections, so exact free flash/RAM after adding bot code is still missing. RAK4631 headroom cannot be safely inferred from compressed ZIP alone.
- **What we can learn:** Design to the RAK4631 cap first. A realistic firmware-bot MVP should target roughly 15-40 KB additional flash, 2-8 KB additional RAM, and under 2 KB persistent config. That budget fits simple parsers, fixed command tables, response strings, pending-response suppression, known-bot prefixes, and channel policy. It does not fit TLS/HTTP clients, geocoders, ephemeris libraries, SQLite-style history, web UIs, or dynamic plugins. Heltec v3 has much more flash margin; RAK4631 should be the release gate.
- **License:** Hardware docs / N/A
- **Confidence:** MEDIUM
- **Source:** WebFetch + gh release + local code — https://docs.heltec.org/en/node/esp32/wifi_lora_32/index.html ; https://docs.rakwireless.com/product-categories/wisblock/rak4631/overview/ ; https://www.nordicsemi.com/products/nrf52840 ; `gh release view companion-v1.15.0 --repo meshcore-dev/MeshCore`; `/Users/cjvana/Documents/GitHub/MeshCore/variants/rak4631/platformio.ini`
- **Checked:** 2026-05-14

### ITEM-prior-art-12: Public-channel emergency handling should be explicit, not an accidental bridge of all bot traffic

- **URL:** N/A
- **What it does well:** The existing local/remote Python/community bot configs show normal monitored channels as #bot/#emergency and an emergency broadcast target of Public. The pivoted project policy refines this: normal bot traffic belongs in private DMs, #bot, and #testing; #emergency should be routed/announced to Public with `EMERGENCY MESSAGE FROM <user>` plus the original text.
- **What it lacks:** Current host-side code also contains Discord webhook forwarding and richer emergency formatting. Firmware cannot assume internet, Discord, or webhook reachability.
- **What we can learn:** Implement #emergency as a special firmware path: do not answer with normal bot help/noise; forward a concise Public alert using the configured node/user identity and original text, then rate-limit to avoid loops. Treat Public as emergency output only for MVP.
- **License:** Private config / N/A
- **Confidence:** HIGH
- **Source:** Local + read-only SSH + project brief — `/Users/cjvana/Documents/GitHub/meshcore-bot-fw/.forge/PROJECT.md`, `cjvana@10.0.0.222:~/meshcore-community-bot/config.ini`, `/Users/cjvana/Documents/GitHub/meshcore-community-bot/community/message_interceptor.py`
- **Checked:** 2026-05-14

## Firmware Feature Portability Assessment

| Python/community bot feature | Firmware MVP status | Reason |
|---|---:|---|
| DM command handling | First | Already present receive path; no multi-bot election needed. |
| #bot/#testing command handling | First | Small channel gate plus passive suppression. |
| #emergency to Public forwarding | First | Project requirement; simple string transform and strict rate limit. |
| `ping`, `help`, `cmd`, `test`, `hello` | First | Fixed strings and small parser. |
| `dice`, `roll` | First | Tiny random-number commands using existing RNG. |
| `path` / basic heard status | First/second | Use packet path/SNR metadata and existing advert path table; keep output short. |
| Passive listen-before-answer suppression | First | Core reason for firmware bot; bounded pending table. |
| Known bot identity trust | First | Small prefix list or full-key list; avoids suppressing on spoofed user text. |
| `stats` | Later, tiny version only | Volatile counters are feasible; SQLite-style history is not. |
| `repeater` management | Later | Contact-store operations are delicate and UI-heavy. |
| Weather/AQI/sports/jokes/dadjoke/feeds | Too heavy for firmware MVP | HTTP/TLS, parsing, API keys, caching, and internet dependency. |
| Satellite/solar forecast/geocoding | Too heavy for firmware MVP | Large math/data/API dependencies. |
| Discord/web viewer/MQTT/analytics | Too heavy/off-device | Requires IP services and host/web stack. |
| Dynamic plugins/i18n translation files | Too heavy | Firmware should use compile-time command table and fixed strings. |

## Confidence Summary

| Item ID | Level | Source Type | URL/Reference |
|---------|-------|-------------|---------------|
| ITEM-prior-art-1 | HIGH | WebFetch + local code | https://github.com/meshcore-dev/MeshCore ; `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/MyMesh.cpp` |
| ITEM-prior-art-2 | HIGH | WebFetch | https://docs.meshcore.io/companion_protocol/ |
| ITEM-prior-art-3 | HIGH | Local repo | `/Users/cjvana/Documents/GitHub/meshcore-bot/README.md`, command modules, requirements |
| ITEM-prior-art-4 | HIGH | Local repo + read-only SSH | community bot design/interceptor and runtime configs |
| ITEM-prior-art-5 | HIGH | WebFetch | https://meshtastic.org/docs/configuration/module/canned-message/ ; https://meshtastic.org/docs/configuration/module/detection-sensor/ ; https://meshtastic.org/docs/configuration/module/remote-hardware/ |
| ITEM-prior-art-6 | HIGH | WebFetch | https://github.com/sudomesh/disaster-radio/blob/master/firmware/src/middleware/Console.cpp |
| ITEM-prior-art-7 | HIGH | WebFetch | https://github.com/richonguzman/LoRa_APRS_iGate |
| ITEM-prior-art-8 | HIGH | WebFetch | https://github.com/Cyclenerd/meshcore-bot |
| ITEM-prior-art-9 | MEDIUM | WebSearch/WebFetch | https://github.com/mr-tbot/mesh-api ; https://github.com/mr-tbot/mesh-ai |
| ITEM-prior-art-10 | HIGH | Local code | `/Users/cjvana/Documents/GitHub/MeshCore/examples/companion_radio/DataStore.cpp` |
| ITEM-prior-art-11 | MEDIUM | WebFetch + gh release + local code | Heltec docs, RAK docs, Nordic docs, MeshCore v1.15.0 release assets, RAK4631 platformio cap |
| ITEM-prior-art-12 | HIGH | Local config + project brief | `.forge/PROJECT.md`, LAN community-bot config, community message interceptor |
