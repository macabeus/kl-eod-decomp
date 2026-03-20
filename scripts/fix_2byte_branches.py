#!/usr/bin/env python3
"""Fix .2byte branch encodings in the monolithic Luvdis output.

Luvdis emits `.2byte 0xXXXX @ bCC _TARGET` when it can't resolve a branch.
This script:
1. Maps each line in the disassembly to its ROM address
2. Collects all branch targets from .2byte comments
3. Inserts labels at the correct positions
4. Replaces .2byte with proper branch instructions

Run AFTER Luvdis but BEFORE generate_asm.py splits the output.
Usage: python3 scripts/fix_2byte_branches.py build/rom_disasm.s
"""

import re
import sys


def instruction_size(line):
    """Return byte size of a disassembly line, or 0 for directives/labels."""
    s = line.strip()
    if not s or s.startswith("@") or s.startswith(".include"):
        return 0
    if re.match(r"^\w+:", s):
        return 0
    if s.startswith("."):
        if s.startswith(".4byte") or s.startswith(".word"):
            return 4
        if s.startswith(".2byte") or s.startswith(".short"):
            return 2
        if s.startswith(".byte"):
            return len(s.split(","))
        return 0
    if s.startswith("bl ") or s.startswith("bl\t"):
        return 4
    return 2


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "build/rom_disasm.s"

    with open(path) as f:
        lines = f.readlines()

    # Pass 1: Map each line to its ROM address
    line_addr = [None] * len(lines)
    current_addr = None

    for i, line in enumerate(lines):
        m = re.match(r"^(\w+):\s*@\s*([0-9A-Fa-f]+)", line)
        if m:
            current_addr = int(m.group(2), 16)
            line_addr[i] = current_addr
            continue

        m = re.match(r"^(_[0-9A-Fa-f]+):", line)
        if m:
            addr = int(m.group(1).lstrip("_"), 16)
            if 0x08000000 <= addr <= 0x08FFFFFF:
                current_addr = addr
                line_addr[i] = current_addr
                continue

        if current_addr is not None:
            line_addr[i] = current_addr
            current_addr += instruction_size(line)

    # Pass 2: Collect all .2byte branch targets
    needed_labels = set()
    branch_lines = []

    for i, line in enumerate(lines):
        m = re.match(r"(\s*)\.2byte\s+0x[0-9A-Fa-f]+\s+@\s+(b\w+)\s+(_[0-9A-Fa-f]+)", line)
        if m:
            branch_lines.append((i, m.group(1), m.group(2), m.group(3)))
            needed_labels.add(m.group(3))

    # Pass 3: Find existing labels
    existing_labels = set()
    for line in lines:
        m = re.match(r"^(\w+):", line)
        if m:
            existing_labels.add(m.group(1))

    missing_labels = needed_labels - existing_labels

    print(f"Branch .2byte entries: {len(branch_lines)}")
    print(f"Unique targets needed: {len(needed_labels)}")
    print(f"Missing labels to add: {len(missing_labels)}")

    # Pass 4: Map target addresses to line positions
    target_addrs = {}
    for label in missing_labels:
        m = re.match(r"_([0-9A-Fa-f]+)$", label)
        if m:
            target_addrs[int(m.group(1), 16)] = label

    inserts = {}
    placed = 0
    for i, addr in enumerate(line_addr):
        if addr is not None and addr in target_addrs:
            inserts[i] = target_addrs.pop(addr)
            placed += 1

    print(f"Labels placed: {placed}")
    if target_addrs:
        print(f"WARNING: {len(target_addrs)} targets not placed")

    # Pass 5: Build new file
    new_lines = []
    branch_set = {bl[0]: bl for bl in branch_lines}

    for i, line in enumerate(lines):
        if i in inserts:
            new_lines.append(f"{inserts[i]}:\n")
        if i in branch_set:
            _, indent, insn, target = branch_set[i]
            new_lines.append(f"{indent}{insn} {target}\n")
        else:
            new_lines.append(line)

    with open(path, "w") as f:
        f.writelines(new_lines)

    print(f"Done: {placed} labels added, {len(branch_lines)} branches fixed")


if __name__ == "__main__":
    main()
