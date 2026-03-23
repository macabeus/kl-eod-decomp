"""Verify extracted graphics .bin files match the original ROM exactly.

Reads graphics/manifest.json and compares each .bin file against the
corresponding region in baserom.gba.

Usage:
    python scripts/verify_gfx.py [baserom.gba]
"""

import json
import os
import sys


def main():
    rom_path = sys.argv[1] if len(sys.argv) > 1 else "baserom.gba"
    manifest_path = "graphics/manifest.json"

    if not os.path.exists(rom_path):
        print(f"ERROR: ROM not found at {rom_path}")
        sys.exit(1)

    if not os.path.exists(manifest_path):
        print(f"ERROR: Manifest not found at {manifest_path}")
        print("Run 'make extract_gfx' first.")
        sys.exit(1)

    with open(rom_path, 'rb') as f:
        rom = f.read()

    with open(manifest_path) as f:
        manifest = json.load(f)

    ok = 0
    fail = 0
    skip = 0

    for asset in manifest["assets"]:
        bin_path = asset.get("bin")
        if not bin_path or not os.path.exists(bin_path):
            skip += 1
            continue

        rom_offset = int(asset["rom_offset"], 16)

        with open(bin_path, 'rb') as f:
            bin_data = f.read()

        rom_region = rom[rom_offset:rom_offset + len(bin_data)]

        if bin_data == rom_region:
            ok += 1
        else:
            fail += 1
            # Find first mismatch
            for i in range(min(len(bin_data), len(rom_region))):
                if bin_data[i] != rom_region[i]:
                    print(f"FAIL: {bin_path} mismatch at byte {i} "
                          f"(bin=0x{bin_data[i]:02X}, rom=0x{rom_region[i]:02X})")
                    break
            else:
                print(f"FAIL: {bin_path} size mismatch "
                      f"(bin={len(bin_data)}, rom region={len(rom_region)})")

    print(f"\nResults: {ok} OK, {fail} FAIL, {skip} skipped")
    if fail > 0:
        sys.exit(1)
    print("All extracted .bin files match the ROM.")


if __name__ == '__main__':
    main()
