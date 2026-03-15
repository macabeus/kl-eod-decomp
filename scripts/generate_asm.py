#!/usr/bin/env python3
"""
Generate the asm/ directory from baserom.gba.

Pipeline:
  1. Disassemble ROM with Luvdis → monolithic .s file
  2. Parse function boundaries from the Luvdis output
  3. Detect hidden sub-functions (return → padding → push {lr})
  4. Merge fragments (functions without a return at end)
  5. Write .s files, downgrade absorbed symbols to local labels
  6. Update C sources (remove/add INCLUDE_ASM lines)
  7. Apply fixups and renames

Outputs (not checked into git):
  - asm/rom_header.s
  - asm/libgcc.s
  - asm/nonmatchings/**/*.s
  - data/data.s

Requires:
  - baserom.gba (SHA1: a0a298d9dba1ba15d04a42fc2eb35893d1a9569b)
  - functions_merged.cfg (checked into git)
  - tools/luvdis (git submodule)

Usage:
  python3 scripts/generate_asm.py
"""

import hashlib
import os
import re
import shutil
import subprocess
import sys
import tomllib

# ---------------------------------------------------------------------------
# Paths & constants
# ---------------------------------------------------------------------------

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASEROM = os.path.join(ROOT, "baserom.gba")
EXPECTED_SHA1 = "a0a298d9dba1ba15d04a42fc2eb35893d1a9569b"
FUNCTIONS_CFG = os.path.join(ROOT, "functions_merged.cfg")
LUVDIS_DIR = os.path.join(ROOT, "tools", "luvdis")
DECOMP_TOML = os.path.join(ROOT, "klonoa-eod-decomp.toml")

CODE_END = 0x52000
DATA_START = 0x52000

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------


def _load_config():
    """Load modules and renames from klonoa-eod-decomp.toml."""
    with open(DECOMP_TOML, "rb") as f:
        config = tomllib.load(f)
    modules = [(m["name"], m["start"]) for m in config["modules"]]
    renames = config.get("renames", {})
    return modules, renames


MODULES, RENAMES = _load_config()

# ---------------------------------------------------------------------------
# Compiled regexes
# ---------------------------------------------------------------------------

# Return / tail-call instructions
_TERMINATING_RE = re.compile(r"^(bx\s|pop\s+\{[^}]*pc[^}]*\}|b\s)")

# Function prologue
_PUSH_LR_RE = re.compile(r"^push\s+\{.*lr.*\}", re.IGNORECASE)

# thumb_func_start macro invocations
_FUNC_MACRO_RE = re.compile(
    r"^\t(thumb_func_start|non_word_aligned_thumb_func_start)\s+(\S+)"
)

# Label definitions and ldr pool references
_LABEL_DEF_RE = re.compile(r"^(\w+):", re.MULTILINE)
_LDR_LABEL_RE = re.compile(r"\bldr\s+\w+,\s+(\w+)")

# INCLUDE_ASM in C sources
_INCLUDE_ASM_RE = re.compile(r'INCLUDE_ASM\("asm/nonmatchings/(\w+)",\s*(\w+)\)')

# Address extraction from Luvdis output
_ADDR_COMMENT_RE = re.compile(r"@ ([0-9A-Fa-f]{8})")
_NAME_ADDR_RE = re.compile(r"(?:FUN_|sub_|thunk_FUN_|thunk_sub_)([0-9A-Fa-f]{6,8})")

# NOP padding (opcode 0x0000)
_NOP = "lsls r0, r0, #0x00"

# ---------------------------------------------------------------------------
# Thumb assembly helpers
# ---------------------------------------------------------------------------


def _is_terminating(line: str) -> bool:
    """True if *line* is a return (bx, pop {pc}) or unconditional branch."""
    return bool(_TERMINATING_RE.match(line.strip()))


def _line_byte_size(stripped: str) -> int:
    """Number of bytes a single assembly line emits."""
    if not stripped or stripped.startswith("@"):
        return 0
    if stripped.endswith(":") and "." not in stripped:
        return 0
    if ".4byte" in stripped:
        return 4
    if ".2byte" in stripped:
        return 2
    if ".byte" in stripped:
        return stripped.split(".byte", 1)[1].count(",") + 1
    if stripped.startswith("."):
        return 0
    if stripped.startswith(("thumb_func_start", "non_word_aligned_thumb_func_start",
                           "thumb_func_end", "arm_func_start", "arm_func_end")):
        return 0
    if re.match(r"^blx?\s", stripped):
        return 4  # bl/blx are 32-bit Thumb instructions
    return 2


def _compute_addresses(lines: list[str], start: int) -> list[int]:
    """ROM address of each line, starting at *start*."""
    addrs: list[int] = []
    addr = start
    for line in lines:
        addrs.append(addr)
        addr += _line_byte_size(line.strip())
    return addrs


def _get_last_instruction(lines: list[str]) -> str | None:
    """Last real instruction, skipping trailing pool data and NOP padding."""
    for line in reversed(lines):
        s = line.strip()
        if not s or s.startswith("@") or s.endswith(":") or s.startswith("."):
            continue
        if re.match(r"^(\w+:\s*)?\.(?:4byte|2byte|byte|align|pool|space)\b", s):
            continue
        if s == _NOP:
            continue
        return s
    return None


def _has_unresolved_pool_refs(first: list[str], second: list[str]) -> bool:
    """True if *first* has ``ldr`` refs to labels only defined in *second*."""
    a, b = "".join(first), "".join(second)
    refs = set(_LDR_LABEL_RE.findall(a))
    defined_here = set(_LABEL_DEF_RE.findall(a))
    defined_next = set(_LABEL_DEF_RE.findall(b))
    return bool((refs - defined_here) & defined_next)


def _is_fragment(lines: list[str], is_last_in_module: bool = False) -> bool:
    """True if the function falls through (no return at end).

    For the last function in a module, also scans for a return anywhere in
    the body — inter-module padding is often decoded as fake instructions.
    """
    last = _get_last_instruction(lines)
    if last is None:
        return True
    if _is_terminating(last):
        return False
    if is_last_in_module:
        return not any(_is_terminating(l.strip()) for l in lines)
    return True


# ---------------------------------------------------------------------------
# Phase 1: Parse Luvdis output into func_entries
# ---------------------------------------------------------------------------


def _parse_luvdis(path):
    """Parse the monolithic Luvdis .s file into per-function entries.

    Returns (func_entries, libgcc_lines, pre_func) where:
      - func_entries: [(name, addr, module, lines)]
      - libgcc_lines: lines for asm/libgcc.s
      - pre_func: (module, lines) or None — content before first function
    """
    with open(path) as f:
        all_lines = f.readlines()

    # Skip Luvdis header (macros, .syntax, .text)
    content_start = 0
    for i, line in enumerate(all_lines):
        s = line.strip()
        if s and not s.startswith("@") and not s.startswith("."):
            content_start = i
            break

    # Find all function boundaries
    func_spans = []  # [(line_idx, name, addr)]
    for i in range(content_start, len(all_lines)):
        m = _FUNC_MACRO_RE.match(all_lines[i])
        if not m:
            continue
        name = m.group(2)
        addr = 0
        if i + 1 < len(all_lines):
            am = _ADDR_COMMENT_RE.search(all_lines[i + 1])
            if am:
                addr = int(am.group(1), 16)
        if addr == 0:
            nm = _NAME_ADDR_RE.search(name)
            if nm:
                addr = int(nm.group(1), 16)
        func_spans.append((i, name, addr))

    # Module lookup
    module_starts = sorted((addr, name) for name, addr in MODULES)

    def get_module(addr):
        mod = module_starts[0][1]
        for mod_addr, mod_name in module_starts:
            if addr >= mod_addr:
                mod = mod_name
            else:
                break
        return mod

    # Split into entries
    func_entries = []
    libgcc_lines = []
    for idx, (start, name, addr) in enumerate(func_spans):
        end = func_spans[idx + 1][0] if idx + 1 < len(func_spans) else len(all_lines)
        lines = all_lines[start:end]
        module = get_module(addr)
        if module == "libgcc":
            libgcc_lines.extend(lines)
        else:
            func_entries.append((name, addr, module, lines))

    # Pre-function content (before the first function)
    pre_func = None
    if func_spans and func_spans[0][0] > content_start:
        pre_lines = all_lines[content_start:func_spans[0][0]]
        if any(l.strip() for l in pre_lines):
            pre_func = (get_module(func_spans[0][2]), pre_lines)

    return func_entries, libgcc_lines, pre_func


# ---------------------------------------------------------------------------
# Phase 2: Detect and split internal sub-functions
# ---------------------------------------------------------------------------


def _detect_sub_functions(lines: list[str], start_addr: int):
    """Find sub-function starts within a Luvdis function body.

    Pattern: terminating instruction → optional padding/data → push {… lr}.
    Skips splits that would break literal pool references.

    Returns list of (line_idx, rom_addr).
    """
    addresses = _compute_addresses(lines, start_addr)
    seen_return = False
    candidates = []

    for i, line in enumerate(lines):
        s = line.strip()
        if not s or s.startswith("@"):
            continue
        if _is_terminating(s):
            seen_return = True
            continue
        if seen_return:
            # Skip labels, directives, NOP padding, and data-as-code
            if s.endswith(":") or s.startswith(".") or s == _NOP:
                continue
            if _PUSH_LR_RE.match(s):
                candidates.append((i, addresses[i]))
                seen_return = False
            else:
                continue  # data decoded as instructions — keep scanning

    # Validate back-to-front: reject splits that break pool references
    valid = []
    remaining = lines
    for idx, addr in reversed(candidates):
        if not _has_unresolved_pool_refs(remaining[:idx], remaining[idx:]):
            valid.append((idx, addr))
            remaining = remaining[:idx]
    valid.reverse()
    return valid


def _expand_sub_functions(func_entries):
    """Split entries that contain multiple logical functions.

    New sub-functions get a ``FUN_XXXXXXXX`` name from their ROM address
    and a ``non_word_aligned_thumb_func_start`` header (no ``.align``
    padding that could shift literal pool entries).
    """
    known_addrs = {addr for _, addr, _, _ in func_entries}
    expanded = []

    for name, addr, module, lines in func_entries:
        subs = [(li, sa) for li, sa in _detect_sub_functions(lines, addr)
                if sa not in known_addrs]
        if not subs:
            expanded.append((name, addr, module, lines))
            continue

        # Build split points: original function + detected sub-functions
        splits = [(0, addr, name)] + [
            (li, sa, f"FUN_{sa:08x}") for li, sa in subs
        ]
        for j, (seg_start, seg_addr, seg_name) in enumerate(splits):
            seg_end = splits[j + 1][0] if j + 1 < len(splits) else len(lines)
            seg_lines = lines[seg_start:seg_end]
            if j > 0:
                seg_lines = [
                    f"\tnon_word_aligned_thumb_func_start {seg_name}\n",
                    f"{seg_name}: @ {seg_addr:08X}\n",
                ] + seg_lines
            expanded.append((seg_name, seg_addr, module, seg_lines))

        print(f"    Split {name} -> {[s[2] for s in splits[1:]]}")

    return expanded


# ---------------------------------------------------------------------------
# Phase 3: Merge fragments
# ---------------------------------------------------------------------------


def _starts_with_prologue(lines: list[str]) -> bool:
    """True if the first real instruction in *lines* is ``push {… lr}``."""
    for line in lines:
        s = line.strip()
        if not s or s.startswith("@") or s.startswith("."):
            continue
        # Skip labels ("LABEL:" or "LABEL: @ comment")
        if ":" in s and not s.startswith(("\t", " ")):
            continue
        if s.startswith(("thumb_func_start", "non_word_aligned_thumb_func_start")):
            continue
        return bool(_PUSH_LR_RE.match(s))
    return False


def _merge_fragments(func_entries):
    """Merge consecutive fragment functions into complete units.

    A function is merged with the next when:
      - It doesn't end with a return (fall-through fragment), OR
      - Its literal pool references labels defined in the next function

    Merging **stops** before absorbing a function that starts with
    ``push {… lr}`` — that's a real function prologue, not a fragment.

    Returns (merged_entries, merged_groups) where merged_groups maps
    each primary function name to all names in its group.
    """
    merged_entries = []
    merged_groups = {}

    i = 0
    while i < len(func_entries):
        name, addr, module, lines = func_entries[i]
        group = [name]
        merged = list(lines)

        j = i
        while True:
            next_same_module = (
                j + 1 < len(func_entries) and func_entries[j + 1][2] == module
            )
            if not next_same_module:
                break
            _, _, _, next_lines = func_entries[j + 1]
            # Never absorb a function that starts with push {lr}
            if _starts_with_prologue(next_lines):
                break
            if (not _is_fragment(merged, is_last_in_module=False)
                    and not _has_unresolved_pool_refs(merged, next_lines)):
                break
            j += 1
            group.append(func_entries[j][0])
            merged.extend(func_entries[j][3])

        merged_entries.append((name, addr, module, merged))
        merged_groups[name] = group
        if len(group) > 1:
            print(f"    Merged {name} <- {group[1:]}")
        i = j + 1

    return merged_entries, merged_groups


# ---------------------------------------------------------------------------
# Phase 4: Write .s files
# ---------------------------------------------------------------------------


def _write_asm_files(merged_entries, libgcc_lines, pre_func):
    """Write asm/nonmatchings/**/*.s and asm/libgcc.s.

    Returns module_funcs: {module: [(addr, name), ...]}.
    """
    nm_root = os.path.join(ROOT, "asm", "nonmatchings")
    match_root = os.path.join(ROOT, "asm", "matchings")

    for d in (nm_root, match_root):
        if os.path.exists(d):
            shutil.rmtree(d)
    for mod_name, _ in MODULES:
        if mod_name != "libgcc":
            os.makedirs(os.path.join(nm_root, mod_name), exist_ok=True)

    module_funcs = {}
    for name, addr, module, lines in merged_entries:
        with open(os.path.join(nm_root, module, f"{name}.s"), "w") as f:
            f.writelines(lines)
        module_funcs.setdefault(module, []).append((addr, name))

    if pre_func:
        mod, pre_lines = pre_func
        with open(os.path.join(nm_root, mod, "_pre.s"), "w") as f:
            f.writelines(pre_lines)

    with open(os.path.join(ROOT, "asm", "libgcc.s"), "w") as f:
        f.write('.include "asm/macros.inc"\n.syntax unified\n.text\n\n')
        f.write("@ libgcc runtime (thunks, division, modulo)\n")
        f.writelines(libgcc_lines)

    print(f"  {len(merged_entries)} functions in asm/nonmatchings/")
    print(f"  Generated asm/libgcc.s ({len(libgcc_lines)} lines)")
    return module_funcs


# ---------------------------------------------------------------------------
# Phase 5: Downgrade internal symbols
# ---------------------------------------------------------------------------


def _downgrade_internal_symbols():
    """Replace internal thumb_func_start macros with plain .thumb_func labels.

    The first thumb_func_start in each file is the real entry point.
    Every subsequent one is a compiler artifact (shared epilogue, tail-merged
    block) that was absorbed during merging.  We keep .global only if the
    symbol is referenced from other .s files in the same module.
    """
    nm_root = os.path.join(ROOT, "asm", "nonmatchings")

    # Pass 1: collect which files reference each symbol
    refs_by_sym = {}  # symbol -> set of file paths
    for mod in os.listdir(nm_root):
        mod_dir = os.path.join(nm_root, mod)
        if not os.path.isdir(mod_dir):
            continue
        for fname in os.listdir(mod_dir):
            if not fname.endswith(".s"):
                continue
            fpath = os.path.join(mod_dir, fname)
            with open(fpath) as f:
                content = f.read()
            for sym in re.findall(
                r"\b(?:bl?|ldr\s+\w+,)\s+(FUN_\w+|sub_\w+|thunk_\w+)", content
            ):
                refs_by_sym.setdefault(sym, set()).add(fpath)

    # Pass 2: downgrade non-first macros to plain labels
    count = 0
    for mod in os.listdir(nm_root):
        mod_dir = os.path.join(nm_root, mod)
        if not os.path.isdir(mod_dir):
            continue
        for fname in os.listdir(mod_dir):
            if not fname.endswith(".s") or fname == "_pre.s":
                continue
            fpath = os.path.join(mod_dir, fname)
            with open(fpath) as f:
                lines = f.readlines()

            first = True
            changed = False
            new_lines = []
            for line in lines:
                m = _FUNC_MACRO_RE.match(line)
                if not m:
                    new_lines.append(line)
                    continue
                if first:
                    first = False
                    new_lines.append(line)
                    continue
                # Downgrade: replace macro with optional .global + .thumb_func
                sym = m.group(2)
                if any(r != fpath for r in refs_by_sym.get(sym, set())):
                    new_lines.append(f".global {sym}\n")
                new_lines.append("\t.thumb_func\n")
                changed = True
                count += 1

            if changed:
                with open(fpath, "w") as f:
                    f.writelines(new_lines)

    print(f"  Downgraded {count} internal symbols to local labels")


# ---------------------------------------------------------------------------
# Phase 6: Update C sources
# ---------------------------------------------------------------------------


def _update_c_sources(merged_groups, module_funcs):
    """Remove INCLUDE_ASM for absorbed fragments; add for new sub-functions.

    New INCLUDE_ASM lines are inserted in ROM address order, right after
    the preceding function's INCLUDE_ASM.
    """
    absorbed = {name for group in merged_groups.values() for name in group[1:]}

    name_to_addr = {
        name: addr
        for func_list in module_funcs.values()
        for addr, name in func_list
    }

    src_dir = os.path.join(ROOT, "src")

    # Map module -> C filename, and collect all existing INCLUDE_ASM names
    module_to_c = {}
    existing_in_c = set()
    for fname in os.listdir(src_dir):
        if not fname.endswith(".c"):
            continue
        with open(os.path.join(src_dir, fname)) as f:
            for line in f:
                m = _INCLUDE_ASM_RE.search(line)
                if m:
                    module_to_c.setdefault(m.group(1), fname)
                    existing_in_c.add(m.group(2))

    # New functions: in module_funcs but not in C, not absorbed, not renamed
    new_by_module = {}  # module -> [(addr, name)]
    for module, func_list in module_funcs.items():
        for addr, name in func_list:
            if name not in existing_in_c and name not in absorbed and name not in RENAMES:
                new_by_module.setdefault(module, []).append((addr, name))

    total_removed = total_added = 0

    for fname in os.listdir(src_dir):
        if not fname.endswith(".c"):
            continue
        file_module = next((m for m, cf in module_to_c.items() if cf == fname), None)
        fpath = os.path.join(src_dir, fname)
        with open(fpath) as f:
            original = f.readlines()

        result = []
        removed = added = 0

        for line in original:
            m = _INCLUDE_ASM_RE.search(line)

            # Drop absorbed fragments
            if m and m.group(2) in absorbed:
                removed += 1
                continue

            result.append(line)

            # After each kept INCLUDE_ASM, insert new functions whose address
            # falls between this one and the next existing INCLUDE_ASM
            if not m or not file_module or file_module not in new_by_module:
                continue

            cur_addr = name_to_addr.get(m.group(2), 0)
            next_addr = _find_next_include_addr(original, m.group(2), name_to_addr)

            insertable = sorted(
                (a, n) for a, n in new_by_module[file_module]
                if cur_addr < a < next_addr
            )
            new_by_module[file_module] = [
                (a, n) for a, n in new_by_module[file_module]
                if not (cur_addr < a < next_addr)
            ]
            for _, n in insertable:
                result.append(f'INCLUDE_ASM("asm/nonmatchings/{file_module}", {n});\n')
                added += 1

        total_removed += removed
        total_added += added
        if removed or added:
            with open(fpath, "w") as f:
                f.writelines(result)
            parts = []
            if removed:
                parts.append(f"removed {removed}")
            if added:
                parts.append(f"added {added}")
            print(f"  {fname}: {', '.join(parts)} INCLUDE_ASM lines")

    if total_removed:
        print(f"  Removed {total_removed} absorbed fragment references")
    if total_added:
        print(f"  Added {total_added} new sub-function references")


def _find_next_include_addr(lines, current_sym, name_to_addr):
    """Find the ROM address of the next INCLUDE_ASM after *current_sym*."""
    past_current = False
    for line in lines:
        m = _INCLUDE_ASM_RE.search(line)
        if not m:
            continue
        if past_current:
            addr = name_to_addr.get(m.group(2), 0)
            if addr > name_to_addr.get(current_sym, 0):
                return addr
        if m.group(2) == current_sym:
            past_current = True
    return float("inf")


# ---------------------------------------------------------------------------
# Post-processing: fixups, renames, move matched
# ---------------------------------------------------------------------------


def _apply_fixups():
    """Apply known assembly fixups for matching."""
    nm_root = os.path.join(ROOT, "asm", "nonmatchings")

    # Fix bx r0 at 0x0802806C: SBZ bit must be preserved
    target = "_0802806C"
    for dirpath, _, filenames in os.walk(nm_root):
        for fname in filenames:
            fpath = os.path.join(dirpath, fname)
            with open(fpath) as f:
                content = f.read()
            if f"\n{target}:\n\tbx r0\n" not in content:
                continue
            content = content.replace(
                f"\n{target}:\n\tbx r0\n",
                f"\n{target}:\n\t.2byte 0x4704 @ bx r0 (SBZ bit preserved)\n",
            )
            with open(fpath, "w") as f:
                f.write(content)
            print(f"  Applied SBZ fix in {os.path.relpath(fpath, ROOT)}")
            break
        else:
            continue
        break
    else:
        print("  WARNING: Could not find bx r0 SBZ fixup target")

    # Add .global for labels referenced across compilation units
    for label in ("_080482B4", "_0804831C"):
        for dirpath, _, filenames in os.walk(nm_root):
            for fname in filenames:
                fpath = os.path.join(dirpath, fname)
                with open(fpath) as f:
                    content = f.read()
                if f"\n{label}:\n" in content and f".global {label}" not in content:
                    content = content.replace(
                        f"\n{label}:\n", f"\n.global {label}\n{label}:\n"
                    )
                    with open(fpath, "w") as f:
                        f.write(content)
                    print(f"  Added .global {label} in {os.path.relpath(fpath, ROOT)}")


def _apply_renames():
    """Apply function renames defined in klonoa-eod-decomp.toml."""
    nm_root = os.path.join(ROOT, "asm", "nonmatchings")

    for old_name, new_name in RENAMES.items():
        # Rename the .s file
        for module in os.listdir(nm_root):
            old_path = os.path.join(nm_root, module, f"{old_name}.s")
            if not os.path.exists(old_path):
                continue
            new_path = os.path.join(nm_root, module, f"{new_name}.s")
            with open(old_path) as f:
                content = f.read()
            with open(new_path, "w") as f:
                f.write(content.replace(old_name, new_name))
            os.remove(old_path)

        # Update references in all .s files
        for dirpath, _, filenames in os.walk(nm_root):
            for fname in filenames:
                if not fname.endswith(".s"):
                    continue
                fpath = os.path.join(dirpath, fname)
                with open(fpath) as f:
                    content = f.read()
                if old_name in content:
                    with open(fpath, "w") as f:
                        f.write(content.replace(old_name, new_name))

    if RENAMES:
        print(f"  Applied {len(RENAMES)} function renames")


def _move_matched_functions():
    """Move .s files for decompiled functions to asm/matchings/.

    A function is "matched" if its .s file is NOT referenced by any
    INCLUDE_ASM call in src/*.c.
    """
    src_dir = os.path.join(ROOT, "src")
    nm_root = os.path.join(ROOT, "asm", "nonmatchings")
    match_root = os.path.join(ROOT, "asm", "matchings")

    referenced = set()
    for fname in os.listdir(src_dir):
        if not fname.endswith(".c"):
            continue
        with open(os.path.join(src_dir, fname)) as f:
            for line in f:
                m = _INCLUDE_ASM_RE.search(line)
                if m:
                    referenced.add((m.group(1), m.group(2)))

    moved = 0
    for module in os.listdir(nm_root):
        module_dir = os.path.join(nm_root, module)
        if not os.path.isdir(module_dir):
            continue
        for fname in os.listdir(module_dir):
            if not fname.endswith(".s") or fname == "_pre.s":
                continue
            func_name = fname[:-2]
            if (module, func_name) not in referenced:
                dest_dir = os.path.join(match_root, module)
                os.makedirs(dest_dir, exist_ok=True)
                os.rename(
                    os.path.join(module_dir, fname),
                    os.path.join(dest_dir, fname),
                )
                moved += 1

    if moved:
        print(f"  Moved {moved} matched functions to asm/matchings/")


# ---------------------------------------------------------------------------
# Static asset generation
# ---------------------------------------------------------------------------


def _sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()


def _validate_baserom():
    if not os.path.exists(BASEROM):
        print(f"ERROR: {BASEROM} not found.", file=sys.stderr)
        print("Place your Klonoa: Empire of Dreams (USA) ROM as baserom.gba", file=sys.stderr)
        sys.exit(1)
    actual = _sha1(BASEROM)
    if actual != EXPECTED_SHA1:
        print(f"ERROR: baserom.gba SHA1 mismatch.", file=sys.stderr)
        print(f"  Expected: {EXPECTED_SHA1}", file=sys.stderr)
        print(f"  Got:      {actual}", file=sys.stderr)
        sys.exit(1)
    print(f"  baserom.gba: OK ({EXPECTED_SHA1})")


def _run_luvdis():
    """Run Luvdis disassembler → build/rom_disasm.s."""
    output = os.path.join(ROOT, "build", "rom_disasm.s")
    os.makedirs(os.path.dirname(output), exist_ok=True)

    env = os.environ.copy()
    env["PYTHONPATH"] = LUVDIS_DIR + os.pathsep + env.get("PYTHONPATH", "")

    print(f"  Running Luvdis (stop=0x{0x08000000 + CODE_END:08X})...")
    result = subprocess.run(
        [sys.executable, "-m", "luvdis", BASEROM,
         "-c", FUNCTIONS_CFG, "-o", output,
         "--default-mode", "THUMB", "--stop", hex(0x08000000 + CODE_END)],
        capture_output=True, text=True, cwd=ROOT, env=env,
    )
    if result.returncode != 0:
        print(f"ERROR: Luvdis failed:\n{result.stderr}", file=sys.stderr)
        sys.exit(1)
    return output


def _generate_rom_header():
    """Generate asm/rom_header.s from the first 0xC0 bytes of the ROM."""
    output = os.path.join(ROOT, "asm", "rom_header.s")
    with open(BASEROM, "rb") as f:
        header = f.read(0xC0)
    with open(output, "w") as f:
        f.write('.include "asm/macros.inc"\n.syntax unified\n.text\n\n')
        f.write("@ GBA ROM Header\n_08000000:\n")
        for i in range(0, len(header), 16):
            chunk = ", ".join(f"0x{b:02X}" for b in header[i:i + 16])
            f.write(f"\t.byte {chunk}\n")
    print(f"  Generated {output}")


def _generate_data_s():
    """Generate data/data.s with .incbin for the data section."""
    output = os.path.join(ROOT, "data", "data.s")
    os.makedirs(os.path.dirname(output), exist_ok=True)
    with open(output, "w") as f:
        f.write(".syntax unified\n.text\n\n@ Data section\n")
        f.write(f'\t.incbin "baserom.gba", 0x{DATA_START:X}\n')
    print(f"  Generated {output}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main():
    os.chdir(ROOT)
    print("Generating asm/ from baserom.gba...\n")

    print("[1/9] Validating baserom.gba...")
    _validate_baserom()

    print("[2/9] Running Luvdis disassembly...")
    luvdis_output = _run_luvdis()

    print("[3/9] Generating rom_header.s...")
    _generate_rom_header()

    print("[4/9] Splitting, detecting sub-functions, merging fragments...")
    func_entries, libgcc_lines, pre_func = _parse_luvdis(luvdis_output)
    func_entries = _expand_sub_functions(func_entries)
    merged_entries, merged_groups = _merge_fragments(func_entries)
    module_funcs = _write_asm_files(merged_entries, libgcc_lines, pre_func)

    print("[5/9] Updating C sources...")
    _update_c_sources(merged_groups, module_funcs)

    print("[6/9] Applying fixups...")
    _apply_fixups()

    print("[7/9] Downgrading internal symbols...")
    _downgrade_internal_symbols()

    print("[8/9] Applying renames...")
    _apply_renames()

    print("[9/9] Generating data.s & moving matched functions...")
    _generate_data_s()
    _move_matched_functions()

    os.remove(luvdis_output)
    print("\nDone! Run 'make compare' to verify.")


if __name__ == "__main__":
    main()
