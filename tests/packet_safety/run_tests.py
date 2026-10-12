#!/usr/bin/env python3
"""Run native packet bounds regressions with the firmware's real Crypto library."""
import argparse
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "vendor" / "MeshCore" / "src"
TESTS = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--crypto-dir", type=Path, required=True)
    args = parser.parse_args()
    crypto = args.crypto_dir.resolve()
    build = ROOT / "out" / "tests" / "packet_safety"
    build.mkdir(parents=True, exist_ok=True)
    text = (SRC.parent / "examples" / "companion_radio" / "MyMesh.cpp").read_text()
    start = text.index("void MyMesh::onContactResponse(")
    end = text.index("bool MyMesh::onContactPathRecv(", start)
    (build / "contact_response.inc").write_text(text[start:end])
    sources = ["AES128.cpp", "AESCommon.cpp", "BlockCipher.cpp", "SHA256.cpp", "Hash.cpp", "Crypto.cpp"]
    binary = build / "test_packet_safety"
    command = [os.environ.get("CXX", "c++"), "-std=c++17", "-g", "-fsanitize=address,undefined",
               "-fno-omit-frame-pointer", "-I", str(SRC), "-I", str(crypto),
               "-I", str(TESTS), "-I", str(build), str(TESTS / "test_packet_safety.cpp"),
               str(TESTS / "test_contact_response.cpp"), str(SRC / "Utils.cpp"), str(SRC / "Packet.cpp")]
    command += [str(crypto / source) for source in sources] + ["-o", str(binary)]
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
