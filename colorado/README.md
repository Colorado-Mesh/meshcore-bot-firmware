# Colorado Mesh Firmware Bot Overlay

This directory holds Colorado Mesh source overlays, fixtures, and notes that are not part of the upstream MeshCore submodule.

Firmware changes that must live inside MeshCore for PlatformIO builds are developed in `vendor/MeshCore`, committed there temporarily, and exported into `patches/meshcore/` with `scripts/export-patches.sh`.
