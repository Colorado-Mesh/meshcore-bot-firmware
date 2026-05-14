#!/usr/bin/env python3
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD_DIR = ROOT / "out" / "tests" / "firmware_bot"
SRC_DIR = ROOT / "vendor" / "MeshCore" / "examples" / "companion_radio"


def main():
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    binary = BUILD_DIR / "test_firmware_bot"
    command = [
        os.environ.get("CXX", "c++"),
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-I",
        str(SRC_DIR),
        str(ROOT / "tests" / "firmware_bot" / "test_firmware_bot.cpp"),
        str(SRC_DIR / "FirmwareBot.cpp"),
        str(SRC_DIR / "BotPolicy.cpp"),
        "-o",
        str(binary),
    ]
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
