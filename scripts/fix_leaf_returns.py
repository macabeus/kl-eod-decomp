#!/usr/bin/env python3
"""Replace 'b .LN' with 'bx lr' in leaf-function shared epilogues.

agbcc shares a single 'bx lr' epilogue across all return paths via
unconditional branches. For matching against hand-written assembly
that has separate 'bx lr' at each return point, this script inlines
the return by replacing 'b .LN' with 'bx lr' directly.

Only applies within leaf functions (no push/pop) that have a backward
loop branch, targeting hand-written loop functions like StrCmp.
"""
import re
import sys


def fix_leaf_returns(path):
    # Only apply to system.s (contains StrCmp which was hand-written
    # with separate bx lr at each return point).
    if not path.endswith("/system.s"):
        return

    with open(path) as f:
        content = f.read()

    # Split into per-function blocks delimited by .thumb_func / .Lfe labels
    # Process each function independently.
    lines = content.splitlines(keepends=True)
    changed = False

    # Find function boundaries: .thumb_func marks the start,
    # .Lfe marks the end (agbcc size directive)
    func_starts = []
    for i, line in enumerate(lines):
        s = line.strip()
        if s == ".thumb_func":
            func_starts.append(i)

    for fi, start in enumerate(func_starts):
        end = func_starts[fi + 1] if fi + 1 < len(func_starts) else len(lines)
        func_lines = lines[start:end]

        # Check: leaf function (no push/pop)?
        has_push = any("push" in l for l in func_lines)
        if has_push:
            continue

        # Check: has TWO distinct return values (mov r0, #0 AND mov r0, #1)?
        # This identifies functions with dual return paths that the compiler
        # merges into a shared bx lr epilogue.
        return_vals = set()
        for l in func_lines:
            s = l.strip()
            if s.startswith("mov\tr0, #"):
                return_vals.add(s)
        if len(return_vals) < 2:
            continue

        # Find return labels: .LN: followed by bx lr
        return_labels = set()
        for i, line in enumerate(func_lines):
            s = line.strip()
            if s == "bx\tlr" and i > 0:
                prev = func_lines[i - 1].strip()
                if prev.startswith(".L") and prev.endswith(":"):
                    return_labels.add(prev[:-1])

        if not return_labels:
            continue

        # Replace "b .LN" with "bx lr" when preceded by "mov rN, #IMM"
        for i, line in enumerate(func_lines):
            s = line.strip()
            if s.startswith("b\t") and i > 0:
                target = s.split("\t")[1]
                if target in return_labels:
                    prev = func_lines[i - 1].strip()
                    if prev.startswith("mov\t") and "#" in prev:
                        func_lines[i] = "\tbx\tlr\n"
                        changed = True

        lines[start:end] = func_lines

    if changed:
        with open(path, "w") as f:
            f.writelines(lines)


if __name__ == "__main__":
    for path in sys.argv[1:]:
        fix_leaf_returns(path)
