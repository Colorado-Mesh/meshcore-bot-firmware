<!--
Thanks for the PR. A few quick prompts to make review fast.
Delete sections that don't apply.
-->

## Summary

<!-- What changed and why. Lead with the why. -->

## Affected layer

- [ ] MeshCore submodule (`vendor/MeshCore/`) — patches re-exported
- [ ] Wrapper tooling (`scripts/`, `tests/`, `.github/`)
- [ ] Colorado overlay (`colorado/`)
- [ ] Docs only

## Verification

- [ ] `MESHCORE_SKIP_APPLY_PATCHES=1 bash scripts/verify.sh --no-build` passes locally
- [ ] (if firmware changed) Built a representative env and confirmed size impact in `out/size/summary.json`
- [ ] (if behavior changed on the radio) Flashed and tested on real hardware — note the board and what you tried:
  > _e.g. Heltec V3, sent `ping`/`trace`/`path` at 0, 2, 4, 6 hops from another node on #bot_

## Bot-prefs schema impact

- [ ] No change to `BotPrefs` shape or default values
- [ ] Schema changed → `BOT_PREFS_VERSION` bumped accordingly
- [ ] N/A
