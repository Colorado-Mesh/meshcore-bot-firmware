#!/usr/bin/env python3
"""Require flashable artifacts for every target and optionally write checksums."""
import argparse
import hashlib
import json
from pathlib import Path


def check_artifacts(directory, environments, required_suffixes=None, version=None):
    files = sorted(path for path in directory.iterdir() if path.is_file() and path.suffix in (".bin", ".uf2", ".hex", ".zip"))
    for env in environments:
        matches = [path for path in files if path.name.startswith(env + "-")]
        if not matches or any(path.stat().st_size == 0 for path in matches):
            raise ValueError(f"Missing or empty firmware for {env}")
        if version and any(not path.name.startswith((env + "-" + version + "-", env + "-" + version + ".")) for path in matches):
            raise ValueError(f"Unexpected firmware version for {env}; expected {version}")
        if not any(path.name.endswith(("-merged.bin", ".uf2", ".bin")) for path in matches):
            raise ValueError(f"No flashable firmware for {env}")
        for suffix in (required_suffixes or {}).get(env, []):
            if not any(path.name.endswith(suffix) for path in matches):
                raise ValueError(f"Missing {suffix} firmware for {env}")
        if any(path.name.endswith("-merged.bin") for path in matches) and not any(
                path.suffix == ".bin" and not path.name.endswith("-merged.bin") for path in matches):
            raise ValueError(f"Missing OTA binary for {env}")
    return files


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--env", action="append", default=[])
    parser.add_argument("--env-file", type=Path)
    parser.add_argument("--manifest", action="store_true")
    parser.add_argument("--version")
    parser.add_argument("--config-json", type=Path)
    args = parser.parse_args()
    envs = args.env
    if args.env_file:
        envs += args.env_file.read_text().splitlines()
    envs = sorted(set(env for env in envs if env))
    if not envs:
        parser.error("At least one environment is required")
    required = {}
    if args.config_json:
        for section, options in json.loads(args.config_json.read_text()):
            env = section.removeprefix("env:")
            if env not in envs:
                continue
            flags = " ".join(dict(options).get("build_flags", []))
            for platform, suffixes in {"ESP32_PLATFORM": ["-merged.bin", ".bin"],
                                       "NRF52_PLATFORM": [".uf2", ".zip"],
                                       "RP2040_PLATFORM": [".uf2", ".bin"],
                                       "STM32_PLATFORM": [".bin", ".hex"]}.items():
                if platform in flags:
                    required[env] = suffixes
                    break
        if set(required) != set(envs):
            raise SystemExit(f"Unknown platform for: {sorted(set(envs) - set(required))}")
    files = check_artifacts(args.artifacts, envs, required, args.version)
    if args.manifest:
        assets = [{"file": path.name, "bytes": path.stat().st_size,
                   "sha256": hashlib.sha256(path.read_bytes()).hexdigest()} for path in files]
        (args.artifacts / "manifest.json").write_text(json.dumps({"version": args.version, "environments": envs, "assets": assets}, indent=2) + "\n")
        (args.artifacts / "SHA256SUMS").write_text("".join(f"{asset['sha256']}  {asset['file']}\n" for asset in assets))
    print(f"Verified {len(files)} firmware assets for {len(envs)} environments")


if __name__ == "__main__":
    main()
