#!/usr/bin/env python3
"""Inject ${cmesh_bot_production.build_flags} into every
*_companion_radio_(usb|ble) env across vendor/MeshCore/variants/.

Run after scripts/apply-patches.sh. Idempotent: skips envs that already
include the flag.

Upstream MeshCore only includes cmesh_bot_production in Heltec V3 and
RAK 4631 variants. For our release matrix (133 companion envs) we want
the bot enabled everywhere, and we don't want to maintain a patch
that has to be kept in sync with every new upstream board.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "vendor" / "MeshCore" / "variants"
ENV_HEADER = re.compile(r"^\[env:[A-Za-z0-9_-]+_companion_radio_(?:usb|ble)\]\s*$")
NEW_SECTION = re.compile(r"^\[")
INJECT_LINE = "  ${cmesh_bot_production.build_flags}"


def process_file(path: Path) -> int:
    lines = path.read_text().splitlines()
    out: list[str] = []
    i = 0
    injected = 0
    while i < len(lines):
        line = lines[i]
        if ENV_HEADER.match(line):
            # Collect this env block until the next [section] header (or EOF).
            j = i + 1
            while j < len(lines) and not NEW_SECTION.match(lines[j]):
                j += 1
            block = lines[i:j]

            if "${cmesh_bot_production.build_flags}" in "\n".join(block):
                out.extend(block)
            else:
                # Find the last indented continuation line of the build_flags block
                # so we can append the injection after it.
                in_flags = False
                last_flag_idx = -1
                for k, ln in enumerate(block):
                    stripped = ln.lstrip()
                    if stripped.startswith("build_flags"):
                        in_flags = True
                        last_flag_idx = k
                    elif in_flags:
                        # A continuation line of build_flags must be indented
                        # with whitespace; comments and blanks don't count.
                        if (ln.startswith("  ") or ln.startswith("\t")) and not stripped.startswith(";"):
                            last_flag_idx = k
                        elif ln.startswith("  ") or ln.startswith("\t"):
                            # An indented comment - stays inside the block but doesn't
                            # change the insertion point.
                            pass
                        else:
                            # Hit a non-indented line (likely build_src_filter or lib_deps).
                            in_flags = False
                if last_flag_idx >= 0:
                    block = block[: last_flag_idx + 1] + [INJECT_LINE] + block[last_flag_idx + 1 :]
                    injected += 1
                out.extend(block)
            i = j
            continue
        out.append(line)
        i += 1

    if injected > 0:
        path.write_text("\n".join(out) + "\n")
    return injected


def main() -> int:
    if not ROOT.is_dir():
        print(f"variants dir not found: {ROOT}", file=sys.stderr)
        return 1

    total = 0
    files_touched = 0
    for ini in sorted(ROOT.rglob("platformio.ini")):
        n = process_file(ini)
        if n:
            print(f"  + {ini.relative_to(ROOT.parent.parent.parent)}: injected into {n} env(s)")
            files_touched += 1
            total += n

    if total == 0:
        print("All companion_radio_(usb|ble) envs already include cmesh_bot_production.build_flags.")
    else:
        print(
            f"Injected cmesh_bot_production.build_flags into {total} env(s) "
            f"across {files_touched} variant file(s)."
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
