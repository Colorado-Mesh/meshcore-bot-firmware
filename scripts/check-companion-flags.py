#!/usr/bin/env python3
"""Check effective flags for every USB/BLE companion environment."""
import json
import os
import shlex
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    meshcore = Path(os.environ.get("MESHCORE_DIR", str(ROOT / "vendor" / "MeshCore")))
    result = subprocess.run(["pio", "project", "config", "--project-dir", str(meshcore), "--json-output"],
                            check=True, capture_output=True, text=True)
    count = 0
    for section, options in json.loads(result.stdout):
        if not section.startswith("env:") or not section.endswith(("_companion_radio_usb", "_companion_radio_ble")):
            continue
        count += 1
        flags = dict(options).get("build_flags", [])
        tokens = shlex.split(" ".join(flags) if isinstance(flags, list) else flags)
        macros = {}
        i = 0
        while i < len(tokens):
            token = tokens[i]
            if token in ("-D", "-U"):
                i += 1
                token += tokens[i]
            if token.startswith("-D"):
                name, _, value = token[2:].partition("=")
                macros[name] = value or "1"
            elif token.startswith("-U"):
                macros.pop(token[2:], None)
            i += 1
        expected = {"CMESH_BOT_ENABLED": "1", "ENABLE_PRIVATE_KEY_IMPORT": "0", "ENABLE_PRIVATE_KEY_EXPORT": "0"}
        for name, value in expected.items():
            if macros.get(name) != value:
                raise SystemExit(f"{section}: expected {name}={value}, got {macros.get(name)!r}")
    if not count:
        raise SystemExit("No companion USB/BLE environments found")
    print(f"Production flags verified in {count} companion environments")


if __name__ == "__main__":
    main()
