# Cutting a release

Releases are produced by pushing a `cmesh-bot-vX.Y.Z` tag. The
`.github/workflows/release.yml` workflow then:

1. Builds **every** companion USB+BLE environment exposed by the pinned
   MeshCore submodule (133 boards as of this writing).
2. Splits the build across 8 parallel matrix shards for wall-time.
3. Embeds the tag version (`vX.Y.Z`) into the firmware via
   `FIRMWARE_VERSION`.
4. Uploads each shard's artifacts.
5. Aggregates everything into a single GitHub Release in draft state,
   ready for you to add release notes and publish.

## Prerequisites

- All PRs for the release are merged to `main`.
- CI on `main` is green (host tests + representative builds).
- You've updated `CHANGELOG.md` with notes for the new version.
- Your local branch is up to date with `origin/main`.

## Steps

```sh
git checkout main
git pull origin main

# Bump CHANGELOG.md, commit, push.
$EDITOR CHANGELOG.md
git commit -am "docs: prep cmesh-bot-vX.Y.Z release notes"
git push origin main

# Tag and push.
git tag -a cmesh-bot-vX.Y.Z -m "Colorado Mesh Bot Firmware vX.Y.Z"
git push origin cmesh-bot-vX.Y.Z
```

The `release.yml` workflow will start automatically. Track progress at
<https://github.com/Colorado-Mesh/meshcore-bot-firmware/actions>.

When the workflow finishes, a **draft** release will appear at
<https://github.com/Colorado-Mesh/meshcore-bot-firmware/releases>.
Review it, paste in the CHANGELOG entry as the description, and publish.

## Tag scheme

- `cmesh-bot-vX.Y.Z` — production release.
- `cmesh-bot-vX.Y.Z-rc.N` — release candidate (still publishes as draft).
- `cmesh-bot-vX.Y.Z-alpha.N` / `-beta.N` — pre-release, mark as
  pre-release when publishing.

The leading `cmesh-bot-` namespace leaves room for future variants
(repeater, room-server) without colliding.

## Versioning

Semantic-ish: bump major for breaking pref/protocol changes, minor for
new commands or boards, patch for fixes. Bump `BOT_PREFS_VERSION` in
`vendor/MeshCore/examples/companion_radio/BotTypes.h` whenever an
existing pref field's meaning or default changes — that forces deployed
bots to reset prefs on first boot.

## Hotfix / unpublish

If a published release turns out to be broken:

1. Mark it as a pre-release (don't delete — flashed devices may still
   reference the URL).
2. Tag and publish `cmesh-bot-vX.Y.(Z+1)` immediately.
3. Edit the broken release's notes to point at the fix.

If you tagged the wrong commit and the workflow hasn't finished:

```sh
git tag -d cmesh-bot-vX.Y.Z
git push --delete origin cmesh-bot-vX.Y.Z
# fix, re-tag, re-push
```

Avoid deleting tags after the workflow has published artifacts — users
may have already downloaded them.
