#!/usr/bin/env python3
"""Builds the browser-flasher manifest and parts from a firmware release's binaries."""
import argparse
import json
import shutil
import sys
from pathlib import Path

OTADATA_SIZE = 0x2000
# Offsets match pgpemu-esp32/partitions.csv. nvs and pgpsecret are left out so pairings and secrets survive.
PARTS = [
    ("bootloader.bin", 0x0),
    ("partition-table.bin", 0x8000),
    ("otadata.bin", 0x2D000),
    ("pgpemu.bin", 0x40000),
]
INPUTS = ("bootloader.bin", "partition-table.bin", "pgpemu.bin")


def build(src: Path, version: str, out: Path) -> None:
    missing = [n for n in INPUTS if not (src / n).is_file()]
    if missing:
        raise FileNotFoundError(f"missing in {src}: {', '.join(missing)}")
    out.mkdir(parents=True, exist_ok=True)
    for name in INPUTS:
        shutil.copyfile(src / name, out / name)
    # A blank otadata makes the bootloader start ota_0 even after a BLE update moved the app to ota_1.
    (out / "otadata.bin").write_bytes(b"\xff" * OTADATA_SIZE)
    manifest = {
        "name": "PGPemu",
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [
            {
                "chipFamily": "ESP32-C3",
                "parts": [{"path": name, "offset": offset} for name, offset in PARTS],
            }
        ],
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("src", type=Path, help="directory with the three firmware .bin files")
    ap.add_argument("version")
    ap.add_argument("out", type=Path)
    args = ap.parse_args()
    try:
        build(args.src, args.version, args.out)
    except FileNotFoundError as e:
        print(e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
