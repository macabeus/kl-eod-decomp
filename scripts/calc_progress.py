#!/usr/bin/env python3
"""Calculate decompilation progress from asm/matchings and asm/nonmatchings folders."""

import json
import os
import re
import sys


def calc_function_size(filepath):
    """Calculate a function's byte size from its .s file.

    Parses the start address from the header label and finds the highest
    address label to determine the end. The size of the last entry
    (.4byte = 4, .2byte/.hword = 2, .byte = 1, instruction = 2) is added.
    """
    start_addr = None
    max_addr = 0
    last_entry_size = 2  # default Thumb instruction

    with open(filepath, "r") as f:
        for line in f:
            line = line.strip()

            # Start address: "FuncName: @ 08XXXXXX"
            if start_addr is None:
                m = re.match(r'\w+:\s+@\s+(0[0-9a-fA-F]+)', line)
                if m:
                    start_addr = int(m.group(1), 16)
                    continue

            # Address label: "_08XXXXXX:" possibly followed by data
            m = re.match(r'_?(0[0-9a-fA-F]+):\s*(.*)', line)
            if m:
                addr = int(m.group(1), 16)
                rest = m.group(2).strip()
                if addr >= max_addr:
                    max_addr = addr
                    if rest.startswith('.4byte'):
                        last_entry_size = 4
                    elif rest.startswith('.2byte') or rest.startswith('.hword'):
                        last_entry_size = 2
                    elif rest.startswith('.byte'):
                        last_entry_size = 1
                    else:
                        # Branch target label or instruction follows on next line
                        last_entry_size = 2

    if start_addr is None:
        return 0

    if max_addr == 0:
        # No address labels found — single-instruction function
        max_addr = start_addr

    return (max_addr - start_addr) + last_entry_size


def scan_folder(folder):
    """Scan a folder for .s files and return total bytes."""
    total_bytes = 0

    if not os.path.isdir(folder):
        return total_bytes

    for root, _dirs, files in os.walk(folder):
        for fname in files:
            if not fname.endswith('.s'):
                continue
            filepath = os.path.join(root, fname)
            total_bytes += calc_function_size(filepath)

    return total_bytes


def main():
    if len(sys.argv) < 2:
        print("Usage: calc_progress.py <project_root> [report.json]")
        sys.exit(1)

    project_root = sys.argv[1]
    report_path = sys.argv[2] if len(sys.argv) > 2 else None

    matchings_dir = os.path.join(project_root, "asm", "matchings")
    nonmatchings_dir = os.path.join(project_root, "asm", "nonmatchings")

    src_bytes = scan_folder(matchings_dir)
    asm_bytes = scan_folder(nonmatchings_dir)
    total_bytes = src_bytes + asm_bytes

    if total_bytes == 0:
        print("ERROR: No code found.", file=sys.stderr)
        sys.exit(1)

    src_pct = 100.0 * src_bytes / total_bytes
    asm_pct = 100.0 * asm_bytes / total_bytes

    print(f"{total_bytes} total bytes of code")
    print(f"{src_bytes} bytes of code in src ({src_pct:.4f}%)")
    print(f"{asm_bytes} bytes of code in asm ({asm_pct:.4f}%)")

    if report_path:
        report = {
            "measures": {
                "total_code": str(total_bytes),
                "matched_code": str(src_bytes),
                "matched_code_percent": round(src_pct, 4),
            }
        }
        with open(report_path, "w") as f:
            json.dump(report, f, indent=2)
        print(f"\nWrote {report_path}")


if __name__ == "__main__":
    main()
