# Colorado MeshCore Bot Firmware

Wrapper repository for Colorado Mesh firmware-only bot work on top of upstream MeshCore companion firmware.

## Layout

- `vendor/MeshCore/` — pinned upstream MeshCore submodule.
- `patches/meshcore/` — ordered patch queue applied to the submodule.
- `colorado/` — Colorado Mesh overlay files, fixtures, and notes.
- `scripts/` — wrapper scripts for patch and build workflows.

## Patch workflow

Initialize the submodule and apply patches:

```sh
git submodule update --init --recursive
bash scripts/apply-patches.sh
```

Develop firmware changes in `vendor/MeshCore`, commit them in that submodule worktree, then export the patch queue:

```sh
bash scripts/export-patches.sh origin/main
```

Representative companion build environments:

- `Heltec_v3_companion_radio_usb`
- `Heltec_v3_companion_radio_ble`
- `RAK_4631_companion_radio_usb`
- `RAK_4631_companion_radio_ble`
