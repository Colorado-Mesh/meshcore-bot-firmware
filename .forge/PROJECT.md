# Forge Project

## Description
Build firmware for MeshCore bots using the standard MeshCore bot project (https://github.com/agessaman/meshcore-bot.git) and MeshCore companion firmware (https://github.com/meshcore-dev/MeshCore.git). The goal is to reduce bot adverts and evaluate/build a built-in response coordinator in firmware as a better alternative to the current VPS coordinator.

## Constraints
- Use the upstream MeshCore bot project and MeshCore companion firmware as primary references.
- Target all MeshCore companion device types where feasible.
- Perform only read-only discovery on local GitHub projects and the hosts `cj-vps` and `cjvana@10.0.0.222` unless later authorized otherwise.
- Deliverable type: code.

## Context
- Greenfield/Brownfield: New project in the Colorado Mesh org, starting from an empty local directory.
- Platform: MeshCore companion firmware / embedded firmware for companion device types.
- Deliverable type: code
- Date: 2026-05-14

## Initial Questions and Answers
- What kind of deliverable should Forge produce? Code project.
- New project or existing local project? New project in the Colorado Mesh org.
- Which target hardware should the first firmware build support? All companion types.
- May local projects and named hosts be inspected? Read-only discovery is OK.

## Candidate Coordination Approach
The user suggested using configured transmit and receive delays to bias which bot responds, potentially approximating which bot is closest or most appropriate for the original message. Proposed topology tiers:

- HILLTOP: highest elevation / backbone infrastructure, typical neighbors 20+, commands `set txdelay 2`, `set direct.txdelay 2`, `set rxdelay 3`.
- FOOTHILLS: mid elevation bridge nodes, typical neighbors 10-20, commands `set txdelay 1.5`, `set direct.txdelay 1`, `set rxdelay 3`.
- SUBURBAN: average rooftop installs, typical neighbors 5-10, commands `set txdelay 0.8`, `set direct.txdelay 0.4`, `set rxdelay 3`.
- LOCAL: low elevation / immediate area, typical neighbors 1-3, commands `set txdelay 0.3`, `set direct.txdelay 0.1`, `set rxdelay 3`.
- MOBILE: variable elevation, should defer to fixed infrastructure, commands `set txdelay 3`, `set direct.txdelay 2.5`, `set rxdelay 3`.

Research and planning should evaluate whether this delay-tier model can reduce duplicate bot adverts/responses, whether it conflicts with MeshCore's existing TX/RX timing semantics, and whether extra coordinator messages are still needed. The current preference is to use these tiers as inspiration only and avoid directly overloading MeshCore `txdelay`, `direct.txdelay`, or `rxdelay` for bot election.

## Refined Decisions
- Scope pivot: target a firmware-only bot rather than a host-side meshcore-bot permission layer. Use meshcore-bot as behavioral reference, not the runtime dependency, unless storage/feasibility research proves firmware-only is impractical.
- Coordination scope: all bot-originated traffic is in scope, but bot traffic should not use the general Public channel during normal operation.
- Channel policy: firmware bot may handle private DMs, #bot, and #testing. Messages sent to #emergency should be forwarded/announced to Public as `EMERGENCY MESSAGE FROM <user>` followed by the original message text.
- Delay policy: use separate bot-specific coordinator delays and suppression windows; do not repurpose lower-layer MeshCore timing knobs as the primary coordination mechanism.
- Duplicate/latency target: balanced; accept rare duplicates while reducing bot noise without excessive response latency.
- Claims: defer explicit on-air claim frames in the first implementation; use passive listen-before-answer suppression first.
- Trust: suppression should trust known bot identities only.
- Compatibility: new firmware-only bot nodes may require the new firmware; mixed stock compatibility is not a hard requirement for participating bots.
- Off-grid model: design for decentralized firmware operation, not VPS parity.
- Source strategy: keep upstream MeshCore as a submodule and maintain Colorado Mesh firmware-bot code/patches around it.
- Build validation: use a small representative build set during development, specifically Heltec v3 and RAK4631, then expand later.
- Additional research request: estimate flash/RAM/storage required by a firmware-only bot and compare it with space available on representative companion devices.
- Hardware available for validation: a Heltec v3 is plugged into this machine and is intended to be the user's bot node. The user authorized using it for firmware flashing after implementation and verification pass.

## Final Deep-Questioning Decisions
- Firmware v1 should pursue more bot parity than the compact MVP, focused on lightweight fun + utility commands rather than HTTP/API/database-backed features.
- RAK4631 remains a hard release gate despite Heltec v3 being the first physical bot node.
- Bot configuration should be runtime-editable through CLI/config commands in Phase 1.
- Production bot firmware should disable private key import/export by default.
- Emergency forwarding may use multipart Public messages rather than strict one- or two-packet truncation.
- Emergency forwarding should never be suppressed; duplicate emergency Public announcements are preferable to a missed emergency.
- Suppression metadata should be hidden if MeshCore supports it; otherwise use passive recognition rather than visible chat markers.
- CI may install PlatformIO and must build representative Heltec v3 USB/BLE plus RAK4631 USB/BLE targets with size reports.
- Public channel bot commands should be ignored silently except for #emergency routing to Public.
- The plugged-in Heltec v3 should be flashed after code review and builds pass.

## Open Planning Interpretations
- “More bot parity” means prioritize firmware-feasible fun + utility commands and avoid features requiring network APIs, files, databases, plugins, or large dynamic text unless size evidence proves they fit.
- “Runtime CLI” should be implemented as compact bot-specific CLI/config commands without changing existing `NodePrefs` binary layout.
- “Multipart emergency” must still be bounded by rate limits and loop prevention to avoid accidental Public floods.
- “All companion types” means representative Heltec v3 and RAK4631 gates first, then expansion to the full companion build matrix after MVP viability.
