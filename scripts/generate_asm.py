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
import struct
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
    """Load modules, renames, and data regions from klonoa-eod-decomp.toml."""
    with open(DECOMP_TOML, "rb") as f:
        config = tomllib.load(f)
    modules = [(m["name"], m["start"]) for m in config["modules"]]
    renames = config.get("renames", {})
    data_regions = sorted(
        (d["start"], d["size"]) for d in config.get("data_regions", [])
    )
    return modules, renames, data_regions


MODULES, RENAMES, DATA_REGIONS = _load_config()

# ---------------------------------------------------------------------------
# Compiled regexes
# ---------------------------------------------------------------------------

# Return / tail-call instructions
_TERMINATING_RE = re.compile(r"^(bx\s|pop\s+\{[^}]*pc[^}]*\}|b\s)")

# Hard returns only (bx / pop {pc}) — excludes `b label` which may be
# an intra-function branch.  Used for leaf function detection.
_HARD_RETURN_RE = re.compile(r"^(bx\s|pop\s+\{[^}]*pc[^}]*\})")

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

# C function definition (detects decompiled functions)
# Matches "type FUN_xxx(" but not extern declarations, calls, or INCLUDE_ASM
_C_FUNC_DEF_RE = re.compile(
    r"^(?!extern\b)\w[\w\s\*]+"
    r"(FUN_[0-9A-Fa-f]+|sub_[0-9A-Fa-f]+)\s*\("
)

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


# Matches Luvdis address labels like "_080005CE:" or "_080005CE: .4byte ..."
_ADDR_LABEL_RE = re.compile(r"^_([0-9A-Fa-f]{7,8}):")


def _compute_addresses(lines: list[str], start: int) -> list[int]:
    """ROM address of each line by counting bytes from *start*."""
    addrs: list[int] = []
    addr = start
    for line in lines:
        addrs.append(addr)
        addr += _line_byte_size(line.strip())
    return addrs


_FUNC_LABEL_ADDR_RE = re.compile(r"^\w+:\s*@\s*([0-9A-Fa-f]{8})")


def _line_byte_size_strict(stripped: str) -> int:
    """Like ``_line_byte_size`` but handles labels with trailing comments.

    ``FUN_08000470: @ 08000470`` → 0 (label only).
    ``_08000630: .4byte 0x040000D4`` → 4 (label + data directive).
    """
    if not stripped or stripped.startswith("@"):
        return 0
    if _is_label_line(stripped):
        after = stripped.split(":", 1)[1].strip() if ":" in stripped else ""
        if ".4byte" in after:
            return 4
        if ".2byte" in after:
            return 2
        if ".byte" in after:
            return after.split(".byte", 1)[1].count(",") + 1
        return 0
    # .inst emits a raw 16-bit value (Luvdis uses it for encodings it
    # can't decode as standard Thumb instructions).
    if stripped.startswith(".inst"):
        return 2
    return _line_byte_size(stripped)


def _compute_addresses_anchored(lines: list[str], start: int) -> list[int]:
    """ROM address of each line, anchored to embedded label addresses.

    Resets the byte counter at ``_XXXXXXXX:`` and ``NAME: @ XXXXXXXX``
    labels.  Uses ``_line_byte_size_strict`` which correctly sizes
    label lines with trailing comments (0 bytes, not 2).
    """
    addrs: list[int] = []
    addr = start
    for line in lines:
        s = line.strip()
        m = _ADDR_LABEL_RE.match(s)
        if m:
            addr = int(m.group(1), 16)
        else:
            m = _FUNC_LABEL_ADDR_RE.match(s)
            if m:
                addr = int(m.group(1), 16)
        addrs.append(addr)
        addr += _line_byte_size_strict(s)
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


def _is_label_line(s: str) -> bool:
    """True if *s* is a label definition (``NAME:`` or ``NAME: @ comment``)."""
    colon = s.find(":")
    return colon > 0 and s[:colon].replace("_", "").isalnum()


def _find_leaf_end(lines: list[str], start: int, max_instrs: int = 20) -> int:
    """If lines[start:] begins a leaf function, return the line index of
    its ``bx lr``.  Otherwise return -1.

    A leaf function is a short code sequence (≤ *max_instrs* instructions)
    ending with ``bx lr`` that contains no ``.4byte`` data and no
    ``push {lr}`` (which would indicate a non-leaf function instead).

    ``.2byte`` directives are allowed — they are raw branch encodings
    that Luvdis emits for certain conditional branches.
    """
    instr_count = 0
    for j in range(start, min(start + max_instrs * 3, len(lines))):
        s = lines[j].strip()
        if not s or s.startswith("@"):
            continue
        if _is_label_line(s):
            continue
        # .4byte = literal pool data → not a leaf function
        if re.search(r"\.(?:4byte|byte)\b", s):
            return -1
        # .2byte = raw instruction encoding (e.g. conditional branches) → OK
        if s.startswith("."):
            continue
        if _PUSH_LR_RE.match(s):
            return -1  # hit another function prologue
        instr_count += 1
        if s == "bx lr":
            return j if instr_count >= 2 else -1
        if instr_count > max_instrs:
            return -1
    return -1


def _detect_sub_functions(lines: list[str], start_addr: int):
    """Find sub-function starts within a Luvdis function body.

    Detects two patterns:
      1. **Non-leaf**: terminating instr → padding/data → ``push {… lr}``
      2. **Leaf**: ``bx``/``pop {pc}`` → padding/data → short code → ``bx lr``

    Pattern 1 triggers on any terminator (including ``b label``).
    Pattern 2 only triggers on hard returns (``bx``/``pop {pc}``) to avoid
    false positives from intra-function unconditional branches.

    Skips splits that would break literal pool references.
    Returns list of (line_idx, rom_addr).
    """
    addresses = _compute_addresses(lines, start_addr)
    seen_any_return = False
    seen_hard_return = False
    candidates = []

    for i, line in enumerate(lines):
        s = line.strip()
        if not s or s.startswith("@"):
            continue

        if _is_terminating(s):
            seen_any_return = True
            if _HARD_RETURN_RE.match(s):
                seen_hard_return = True
            continue

        if not seen_any_return and not seen_hard_return:
            continue

        # Skip labels, directives, NOP padding, data
        if _is_label_line(s) or s.startswith(".") or s == _NOP:
            continue
        if re.search(r"\.(?:4byte|2byte|byte)\b", s):
            continue

        # Pattern 1: push {lr} after any terminator
        if seen_any_return and _PUSH_LR_RE.match(s):
            candidates.append((i, addresses[i]))
            seen_any_return = seen_hard_return = False
            continue

        # Pattern 2: leaf function after a hard return (bx / pop {pc})
        if seen_hard_return and _find_leaf_end(lines, i) >= 0:
            candidates.append((i, addresses[i]))
            seen_any_return = seen_hard_return = False
            continue

        # Data decoded as instructions — keep scanning
        continue

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


def _is_gba_pointer(word: int, rom_size: int) -> bool:
    """True if *word* looks like a valid GBA memory-mapped pointer."""
    return (0x08000000 <= word < 0x08000000 + rom_size  # ROM
            or 0x03000000 <= word <= 0x03007FFF          # IWRAM
            or 0x02000000 <= word <= 0x0203FFFF          # EWRAM
            or 0x04000000 <= word <= 0x040003FF)         # I/O registers


# Regex for the lsrs instruction — extracts Rd and Rm register numbers.
_LSRS_REG_RE = re.compile(r"lsrs\s+r(\d),\s*r(\d)")


def _is_instruction_line(s: str) -> bool:
    """True if *s* is an instruction mnemonic (not a directive/label/data)."""
    if _line_byte_size(s) == 0:
        return False
    if ".4byte" in s or ".2byte" in s or ".byte" in s:
        return False
    if _is_label_line(s) or s.endswith(":"):
        return False
    if s.startswith(".") or s.startswith(("thumb_func_start",
            "non_word_aligned_thumb_func_start")):
        return False
    return True


def _next_instruction(func_lines: list[str], after: int) -> str:
    """Return the stripped text of the next instruction line after *after*."""
    for ni in range(after + 1, min(len(func_lines), after + 3)):
        ns = func_lines[ni].strip()
        if _line_byte_size(ns) > 0 and not _is_label_line(ns):
            return ns
    return ""


def _rom_hw(rom_data: bytes, rom_offset: int) -> int:
    """Read a little-endian 16-bit halfword from ROM."""
    return struct.unpack_from("<H", rom_data, rom_offset)[0]


def _rom_word(rom_data: bytes, rom_offset: int) -> int:
    """Read a little-endian 32-bit word from ROM."""
    return struct.unpack_from("<I", rom_data, rom_offset)[0]


def _build_near_pool(func_lines: list[str], extra: set[int] = ()) -> set[int]:
    """Line indices within ±5 of any data directive or *extra* entry."""
    sources = set()
    for i, line in enumerate(func_lines):
        s = line.strip()
        if ".4byte" in s or ".2byte" in s:
            sources.add(i)
    sources |= set(extra)
    near = set()
    for i in sources:
        for j in range(max(0, i - 5), min(len(func_lines), i + 6)):
            near.add(j)
    return near


def _find_high_halfwords(func_lines, addresses, rom_data, rom_size):
    """Identify lines whose instruction is the HIGH halfword of a data pointer.

    Detects two patterns:
      - ``lsrs Rd, Rm, #0x20`` → encoding ``0x08XX`` (ROM pointer high)
      - ``lsls r0, r0, #0x0C`` → encoding ``0x0300`` (IWRAM pointer high)

    Returns the set of line indices to convert.
    """
    rom_base = 0x08000000
    convert = set()

    for i, line in enumerate(func_lines):
        s = line.strip()
        if not _is_instruction_line(s):
            continue

        rom_offset = addresses[i] - rom_base
        if rom_offset < 0 or rom_offset + 2 > len(rom_data):
            continue

        hw = _rom_hw(rom_data, rom_offset)

        # --- Pattern 1: lsrs Rd, Rm, #0x20 (encodes as 0x08XX) ---
        if "lsrs" in s and "#0x20" in s:
            if (hw >> 8) != 0x08:
                continue  # address drifted

            # If the NEXT instruction is 0x0300 (lsls r0, r0, #0x0C), this
            # lsrs is a LOW halfword of an IWRAM pointer, not a high halfword.
            if _next_instruction(func_lines, i) == "lsls r0, r0, #0x0C":
                continue

            # The preceding halfword + this halfword must form a valid pointer.
            if rom_offset >= 2:
                prev = _rom_hw(rom_data, rom_offset - 2)
                if not _is_gba_pointer((hw << 16) | prev, rom_size):
                    continue

            # Verify encoding matches the register operands in the text.
            m = _LSRS_REG_RE.search(s)
            if m:
                expected = 0x0800 | int(m.group(1)) | (int(m.group(2)) << 3)
                if hw != expected:
                    continue  # address drifted
            convert.add(i)

        # --- Pattern 2: lsls r0, r0, #0x0C (encodes as 0x0300) ---
        elif s == "lsls r0, r0, #0x0C":
            if hw != 0x0300:
                continue  # address drifted
            convert.add(i)

    return convert


def _find_low_halfwords(func_lines, addresses, rom_data, rom_size,
                        convert, near_pool):
    """Find low halfword partners for converted high halfwords.

    For each high halfword in *convert* (or existing ``.2byte 0x08XX``),
    look at the preceding line.  If it forms a valid GBA pointer pair,
    mark it for conversion and record the low→high pairing.

    Returns ``(low_halves, low_to_high)`` where:
      - *low_halves*: set of line indices to add to convert
      - *low_to_high*: dict mapping low line → high line (for .4byte merge)
    """
    rom_base = 0x08000000

    # Also pair with existing .2byte 0x08XX entries (Luvdis pre-converted).
    existing_high = set()
    for i, line in enumerate(func_lines):
        if re.match(r"\.2byte 0x08[0-9A-Fa-f]{2}$", line.strip()):
            existing_high.add(i)

    low_halves = set()
    low_to_high: dict[int, int] = {}
    already_high = set(low_to_high.values())

    for i in sorted(convert | existing_high):
        # Walk backwards to find the preceding data-bearing line.
        for k in range(i - 1, max(0, i - 3), -1):
            ks = func_lines[k].strip()
            if _line_byte_size(ks) == 0 or _is_label_line(ks):
                continue  # skip labels / zero-byte directives

            # Already converted or a .4byte literal pool → not a low partner.
            if k in convert or ".4byte" in ks:
                break

            is_existing_2byte = ".2byte" in ks
            # Instruction lines must be near existing data (guards drift).
            if not is_existing_2byte and k not in near_pool:
                break

            # Read the 32-bit word starting at k's address.
            lo_off = addresses[k] - rom_base
            if lo_off < 0 or lo_off + 4 > len(rom_data):
                break
            word = _rom_word(rom_data, lo_off)
            if not _is_gba_pointer(word, rom_size):
                break

            if not is_existing_2byte:
                low_halves.add(k)
            # Record for .4byte merge only when directly adjacent
            # (no label between them that would be misaligned).
            if k == i - 1:
                low_to_high[k] = i
            break

    return low_halves, low_to_high


def _pair_consecutive_converts(convert, low_to_high, addresses, rom_data,
                               rom_size):
    """Pair adjacent convert entries that form a GBA pointer.

    Handles the edge case where BOTH halfwords are ``lsrs #0x20``
    (e.g. ``0x0808`` + ``0x0806`` = ``0x08060808``).  Neither was
    picked up as a low partner in the previous pass because both
    were in ``convert``.
    """
    rom_base = 0x08000000
    already_paired = set(low_to_high) | set(low_to_high.values())
    for k, i in zip(sorted(convert), sorted(convert)[1:]):
        if k in already_paired or i in already_paired:
            continue
        if k != i - 1:
            continue
        lo_off = addresses[k] - rom_base
        if lo_off < 0 or lo_off + 4 > len(rom_data):
            continue
        word = _rom_word(rom_data, lo_off)
        if _is_gba_pointer(word, rom_size):
            low_to_high[k] = i
            already_paired.update((k, i))


def _emit_conversions(func_lines, addresses, rom_data, rom_size,
                      convert, low_to_high):
    """Produce the final line list, replacing converted entries with data
    directives and merging pointer pairs into ``.4byte``.
    """
    rom_base = 0x08000000
    high_to_low = {h: k for k, h in low_to_high.items()}
    is_low_partner = set(low_to_high)  # lines that should emit .4byte
    is_high_partner = set(high_to_low)  # lines that may be skipped

    merged_low = set()  # tracks which lows actually emitted .4byte
    result = []

    for i, line in enumerate(func_lines):
        # --- High partner: skip if its low partner already emitted .4byte ---
        if i in is_high_partner:
            if high_to_low[i] in merged_low:
                continue
            # Low partner didn't merge → fall through and keep this line.

        # --- Low partner (existing .2byte, not in convert) ---
        if i not in convert and i in is_low_partner:
            rom_offset = addresses[i] - rom_base
            if 0 <= rom_offset and rom_offset + 4 <= len(rom_data):
                word = _rom_word(rom_data, rom_offset)
                if _is_gba_pointer(word, rom_size):
                    result.append(f"\t.4byte 0x{word:08X}\n")
                    merged_low.add(i)
                    continue
            result.append(line)
            continue

        # --- Not in convert → keep unchanged ---
        if i not in convert:
            result.append(line)
            continue

        # --- Converted line: emit data directive ---
        rom_offset = addresses[i] - rom_base
        s = line.strip()
        line_size = _line_byte_size_strict(s)

        if line_size == 2 and rom_offset + 2 <= len(rom_data):
            hw = _rom_hw(rom_data, rom_offset)

            # Safety: verify lsrs encoding still matches ROM.
            if "lsrs" in s and "#0x20" in s and (hw >> 8) != 0x08:
                result.append(line)  # address drift — keep original

            elif i in is_low_partner:
                # Low halfword → emit .4byte for the full pointer pair.
                if rom_offset + 4 <= len(rom_data):
                    word = _rom_word(rom_data, rom_offset)
                    if _is_gba_pointer(word, rom_size):
                        result.append(f"\t.4byte 0x{word:08X}\n")
                        merged_low.add(i)
                    else:
                        result.append(line)
                else:
                    result.append(line)

            else:
                # Unpaired high halfword → emit .2byte.
                result.append(f"\t.2byte 0x{hw:04X}\n")

        elif line_size == 4 and rom_offset + 4 <= len(rom_data):
            word = _rom_word(rom_data, rom_offset)
            result.append(f"\t.4byte 0x{word:08X}\n")

        else:
            result.append(line)

    return result


def _apply_data_regions(func_lines: list[str], func_addr: int,
                        rom_data: bytes) -> list[str]:
    """Replace data-as-code instruction mnemonics with data directives.

    Three-pass pipeline:
      1. **High halfwords** — identify ``lsrs #0x20`` (0x08XX) and
         ``lsls r0, r0, #0x0C`` (0x0300) entries.
      2. **Low halfwords** — for each high, find its preceding partner
         and verify the 32-bit ROM word is a valid GBA pointer.
      3. **Consecutive pairs** — handle edge cases where both halfwords
         are in the convert set (both encode as 0x08XX).

    Adjacent low+high pairs are emitted as a single ``.4byte``;
    unpaired high halfwords become ``.2byte``.
    """
    if not DATA_REGIONS:
        return func_lines

    rom_size = len(rom_data)
    addresses = _compute_addresses_anchored(func_lines, func_addr)

    # Pass 1: find high halfwords to convert.
    convert = _find_high_halfwords(func_lines, addresses, rom_data, rom_size)

    # Expand near_pool so low halfwords adjacent to converts are eligible.
    near_pool = _build_near_pool(func_lines, extra=convert)

    # Pass 2: find low halfword partners.
    low_halves, low_to_high = _find_low_halfwords(
        func_lines, addresses, rom_data, rom_size, convert, near_pool)
    convert |= low_halves

    # Pass 3: pair consecutive converts (both-0x08XX edge case).
    _pair_consecutive_converts(convert, low_to_high, addresses, rom_data,
                               rom_size)

    if not convert:
        return func_lines

    return _emit_conversions(func_lines, addresses, rom_data, rom_size,
                             convert, low_to_high)


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

    # Load ROM bytes for data region conversion
    rom_data = b""
    if DATA_REGIONS:
        with open(BASEROM, "rb") as f:
            rom_data = f.read()

    module_funcs = {}
    for name, addr, module, lines in merged_entries:
        if DATA_REGIONS:
            lines = _apply_data_regions(lines, addr, rom_data)
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
    # Include renamed versions so we match C sources that use new names
    for old_name, new_name in RENAMES.items():
        if old_name in absorbed:
            absorbed.add(new_name)

    name_to_addr = {
        name: addr
        for func_list in module_funcs.values()
        for addr, name in func_list
    }

    src_dir = os.path.join(ROOT, "src")

    # Map module -> C filename, and collect all existing INCLUDE_ASM names
    # and decompiled function definitions
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
                else:
                    m = _C_FUNC_DEF_RE.search(line)
                    if m:
                        existing_in_c.add(m.group(1))

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


def _manual_split_leaf(nm_root, module, parent_fname, trigger_instr,
                       new_addr, new_name):
    """Split a leaf function out of a merged .s file at a specific instruction.

    Finds the first occurrence of *trigger_instr* after the last ``bx``
    return in *parent_fname* and splits everything from that line onward
    into a new file *new_name*.s.  Also adds INCLUDE_ASM to the C source.
    """
    parent_path = os.path.join(nm_root, module, parent_fname)
    if not os.path.exists(parent_path):
        return
    with open(parent_path) as f:
        lines = f.readlines()

    # Find the split point: trigger_instr after a bx return
    split_at = None
    seen_bx = False
    for i, line in enumerate(lines):
        s = line.strip()
        if s.startswith("bx ") or (s.startswith("pop") and "pc" in s):
            seen_bx = True
        if seen_bx and s == trigger_instr:
            split_at = i
            break

    if split_at is None:
        return

    # Write the two files
    parent_lines = lines[:split_at]
    new_lines = [
        f"\tnon_word_aligned_thumb_func_start {new_name}\n",
        f"{new_name}: @ {new_addr:08X}\n",
    ] + lines[split_at:]

    with open(parent_path, "w") as f:
        f.writelines(parent_lines)
    new_path = os.path.join(nm_root, module, f"{new_name}.s")
    with open(new_path, "w") as f:
        f.writelines(new_lines)

    # Add INCLUDE_ASM to the C source, right after the parent's INCLUDE_ASM
    # (only if not already present)
    parent_name = parent_fname[:-2]  # strip .s
    src_dir = os.path.join(ROOT, "src")
    include_line = f'INCLUDE_ASM("asm/nonmatchings/{module}", {new_name});\n'
    for cfname in os.listdir(src_dir):
        if not cfname.endswith(".c"):
            continue
        cpath = os.path.join(src_dir, cfname)
        with open(cpath) as f:
            clines = f.readlines()
        # Skip if already present
        if any(new_name in cl for cl in clines):
            break
        for ci, cl in enumerate(clines):
            if parent_name in cl and "INCLUDE_ASM" in cl:
                clines.insert(ci + 1, include_line)
                with open(cpath, "w") as f:
                    f.writelines(clines)
                break

    print(f"  Manual split: {parent_fname} -> {new_name}")


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

    # Split leaf function at 0x0804FE10 out of FUN_0804fc10.
    # This function uses push {r4, r5} (no lr) which the automatic
    # prologue detection doesn't handle.  It ends with pop {r4, r5}; bx lr.
    _manual_split_leaf(nm_root, "m4a", "FUN_0804fc10.s",
                       "push {r4, r5}", 0x0804FE10, "FUN_0804fe10")

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

        # Update INCLUDE_ASM references in C sources
        src_dir = os.path.join(ROOT, "src")
        for cfname in os.listdir(src_dir):
            if not cfname.endswith(".c"):
                continue
            cpath = os.path.join(src_dir, cfname)
            with open(cpath) as f:
                content = f.read()
            if old_name in content:
                with open(cpath, "w") as f:
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


# Matches ".2byte 0xXXXX @ bCC _TARGET"
_2BYTE_BRANCH_RE = re.compile(
    r"(\s*)\.2byte\s+0x[0-9A-Fa-f]+\s+@\s+(b\w+)\s+(_[0-9A-Fa-f]+)"
)


def _fix_2byte_branches(path: str):
    """Replace .2byte branch encodings with proper branch instructions.

    Luvdis emits ``.2byte 0xXXXX @ bCC _TARGET`` when it cannot resolve
    a conditional branch (typically cross-function branches within the
    same module).  This function:

    1. Computes the ROM address of every line using
       ``_compute_addresses_anchored``.
    2. Collects all branch targets from ``.2byte`` comments.
    3. Inserts missing target labels at the correct byte positions.
    4. Replaces ``.2byte`` with the proper branch instruction.

    Called on the monolithic Luvdis output **before** function splitting
    so that labels and branches are in place when functions are detected.
    """
    with open(path) as f:
        lines = f.readlines()

    # --- Compute address of every line ---
    # Find the first function label to get the ROM base
    start_addr = 0x08000000
    for line in lines:
        m = _FUNC_LABEL_ADDR_RE.match(line.strip())
        if m:
            start_addr = int(m.group(1), 16)
            break

    addrs = _compute_addresses_anchored(lines, start_addr)

    # --- Collect branch targets ---
    needed = set()          # label names needed
    branch_idxs = {}        # line_idx -> (indent, insn, target)
    for i, line in enumerate(lines):
        m = _2BYTE_BRANCH_RE.match(line)
        if m:
            branch_idxs[i] = (m.group(1), m.group(2), m.group(3))
            needed.add(m.group(3))

    # --- Find existing labels ---
    existing = set()
    for line in lines:
        m = re.match(r"^(\w+):", line)
        if m:
            existing.add(m.group(1))

    missing = needed - existing
    if not missing and not branch_idxs:
        return  # nothing to do

    # --- Map target addresses to line indices ---
    target_addrs = {}       # addr -> label name
    for label in missing:
        m = re.match(r"_([0-9A-Fa-f]+)$", label)
        if m:
            target_addrs[int(m.group(1), 16)] = label

    # Build addr -> first line index at that addr (for label insertion)
    inserts = {}            # line_idx -> label name
    for i, addr in enumerate(addrs):
        if addr in target_addrs:
            inserts[i] = target_addrs.pop(addr)

    placed = len(missing) - len(target_addrs)

    # --- Rebuild file ---
    new_lines = []
    for i, line in enumerate(lines):
        if i in inserts:
            new_lines.append(f"{inserts[i]}:\n")
        if i in branch_idxs:
            indent, insn, target = branch_idxs[i]
            new_lines.append(f"{indent}{insn} {target}\n")
        else:
            new_lines.append(line)

    with open(path, "w") as f:
        f.writelines(new_lines)

    branches_fixed = len(branch_idxs)
    unplaced = len(target_addrs)
    print(f"  {placed} labels added, {branches_fixed} branches resolved"
          + (f" ({unplaced} targets not placed)" if unplaced else ""))


def _fix_2byte_in_split_files():
    """Replace .2byte branch encodings in per-function .s files.

    Uses the ROM binary to verify each .2byte encoding against the
    computed source address.  Only converts branches where the ROM
    verification passes.  Two labels (0x08047B04, 0x0804EA70) are
    excluded due to data-as-code regions causing off-by-2 placement.
    """
    # Labels that _compute_addresses_anchored places incorrectly
    # due to data-as-code regions (off by 2 bytes).
    SKIP_LABELS = {"_08047B04", "_0804EA70"}
    nm_root = os.path.join(ROOT, "asm", "nonmatchings")
    with open(BASEROM, "rb") as f:
        rom = f.read()

    # Collect all .s files
    all_files = {}
    for mod in os.listdir(nm_root):
        mod_dir = os.path.join(nm_root, mod)
        if not os.path.isdir(mod_dir):
            continue
        for fname in os.listdir(mod_dir):
            if not fname.endswith(".s"):
                continue
            fpath = os.path.join(mod_dir, fname)
            with open(fpath) as f:
                all_files[fpath] = f.readlines()

    # For each file, compute addr map and find .2byte entries
    all_labels = {}  # label -> fpath
    for fpath, lines in all_files.items():
        for line in lines:
            m = re.match(r"^(\w+):", line)
            if m:
                all_labels[m.group(1)] = fpath

    # Build global addr → (fpath, line_idx) map from anchored addresses only
    addr_index = {}  # addr -> (fpath, line_idx)
    for fpath, lines in all_files.items():
        start = 0
        for line in lines:
            m = _FUNC_LABEL_ADDR_RE.match(line.strip())
            if m:
                start = int(m.group(1), 16)
                break
            m2 = _ADDR_LABEL_RE.match(line.strip())
            if m2:
                start = int(m2.group(1), 16)
                break
        if not start:
            continue
        addrs = _compute_addresses_anchored(lines, start)
        for i, a in enumerate(addrs):
            if a not in addr_index:
                addr_index[a] = (fpath, i)

    # Process each .2byte branch
    labels_added = 0
    branches_fixed = 0
    globals_added = 0

    # Collect all branch entries first
    branch_info = []  # (fpath, line_idx, indent, insn, target_label, target_addr, source_addr)
    for fpath, lines in all_files.items():
        start = 0
        for line in lines:
            m = _FUNC_LABEL_ADDR_RE.match(line.strip())
            if m:
                start = int(m.group(1), 16)
                break
            m2 = _ADDR_LABEL_RE.match(line.strip())
            if m2:
                start = int(m2.group(1), 16)
                break
        if not start:
            continue

        addrs = _compute_addresses_anchored(lines, start)
        for i, line in enumerate(lines):
            m2 = _2BYTE_BRANCH_RE.match(line)
            if not m2:
                continue
            indent, insn, target = m2.group(1), m2.group(2), m2.group(3)
            tm = re.match(r"_([0-9A-Fa-f]+)$", target)
            if not tm or target in SKIP_LABELS:
                continue
            target_addr = int(tm.group(1), 16)

            # Verify: read the ROM at the computed source address and check the .2byte value
            src_addr = addrs[i]
            rom_offset = src_addr - 0x08000000
            if 0 <= rom_offset < len(rom) - 1:
                rom_hw = struct.unpack_from("<H", rom, rom_offset)[0]
                raw_m = re.search(r"0x([0-9A-Fa-f]+)", line)
                if raw_m:
                    asm_hw = int(raw_m.group(1), 16)
                    if rom_hw == asm_hw:
                        # Verified: our source address is correct
                        branch_info.append((fpath, i, indent, insn, target, target_addr, src_addr))

    # For verified branches, add target labels and convert
    needs_global = {}  # label -> defining fpath
    file_changes = {}  # fpath -> list of (line_idx, action, data)

    for fpath, line_idx, indent, insn, target, target_addr, src_addr in branch_info:
        if target in all_labels:
            # Label already exists — just convert the .2byte
            file_changes.setdefault(fpath, []).append(
                (line_idx, "convert", (indent, insn, target)))
            branches_fixed += 1
            if all_labels[target] != fpath:
                needs_global[target] = all_labels[target]
            continue

        # Need to add label. Find position in addr_index
        if target_addr not in addr_index:
            continue  # can't place — skip this branch

        tgt_fpath, tgt_line = addr_index[target_addr]

        # Verify target position: ROM byte at target_addr should be a valid instruction
        tgt_rom_offset = target_addr - 0x08000000
        if tgt_rom_offset < 0 or tgt_rom_offset >= len(rom):
            continue

        # Add label and convert
        file_changes.setdefault(tgt_fpath, []).append(
            (tgt_line, "label", target))
        all_labels[target] = tgt_fpath
        labels_added += 1

        file_changes.setdefault(fpath, []).append(
            (line_idx, "convert", (indent, insn, target)))
        branches_fixed += 1

        if tgt_fpath != fpath:
            needs_global[target] = tgt_fpath

    # Apply changes to files (process in reverse line order to avoid index shifts)
    for fpath, changes in file_changes.items():
        lines = list(all_files[fpath])
        # Sort changes by line index, descending (to avoid index shifting)
        changes.sort(key=lambda x: x[0], reverse=True)
        # But labels must be inserted BEFORE conversions at the same index
        # Group by line_idx
        by_line = {}
        for line_idx, action, data in changes:
            by_line.setdefault(line_idx, []).append((action, data))

        new_lines = []
        for i, line in enumerate(lines):
            if i in by_line:
                for action, data in by_line[i]:
                    if action == "label":
                        new_lines.append(f"{data}:\n")
                converted = False
                for action, data in by_line[i]:
                    if action == "convert":
                        indent, insn, target = data
                        new_lines.append(f"{indent}{insn} {target}\n")
                        converted = True
                        break
                if not converted:
                    new_lines.append(line)
            else:
                new_lines.append(line)
        all_files[fpath] = new_lines

    # Add .global for cross-file references
    for target, def_fpath in needs_global.items():
        lines = all_files[def_fpath]
        if not any(f".global {target}" in l for l in lines):
            new_lines = []
            for line in lines:
                if line.strip() == f"{target}:" or line.strip().startswith(f"{target}:"):
                    new_lines.append(f".global {target}\n")
                    globals_added += 1
                new_lines.append(line)
            all_files[def_fpath] = new_lines

    # Write all files
    for fpath, lines in all_files.items():
        with open(fpath, "w") as f:
            f.writelines(lines)

    print(f"  {labels_added} labels added, {branches_fixed} branches resolved, "
          f"{globals_added} .global added")


def _resolve_known_2byte_branches():
    """Replace .2byte branch encodings ONLY when the target label already exists.

    Does NOT insert new labels (address computation is unreliable in
    data-as-code regions). Only converts .2byte → branch instruction
    when the target label is already defined in the same translation unit.

    Also adds .global for cross-file label references.
    """
    nm_root = os.path.join(ROOT, "asm", "nonmatchings")

    # Collect all labels across all .s files
    all_labels = set()
    all_files = {}
    for mod in os.listdir(nm_root):
        mod_dir = os.path.join(nm_root, mod)
        if not os.path.isdir(mod_dir):
            continue
        for fname in os.listdir(mod_dir):
            if not fname.endswith(".s"):
                continue
            fpath = os.path.join(mod_dir, fname)
            with open(fpath) as f:
                lines = f.readlines()
            all_files[fpath] = lines
            for line in lines:
                m = re.match(r"^(\w+):", line)
                if m:
                    all_labels.add(m.group(1))

    # Replace .2byte with branch instructions only if target label exists
    branches_fixed = 0
    globals_added = 0
    cross_file_refs = {}  # label -> set of files referencing it

    for fpath, lines in all_files.items():
        file_labels = {m.group(1) for line in lines
                       for m in [re.match(r"^(\w+):", line)] if m}
        new_lines = []
        changed = False
        for line in lines:
            m = _2BYTE_BRANCH_RE.match(line)
            if m:
                indent, insn, target = m.group(1), m.group(2), m.group(3)
                if target in all_labels:
                    new_lines.append(f"{indent}{insn} {target}\n")
                    branches_fixed += 1
                    changed = True
                    if target not in file_labels:
                        cross_file_refs.setdefault(target, set()).add(fpath)
                    continue
            new_lines.append(line)
        if changed:
            all_files[fpath] = new_lines

    # Add .global for cross-file references
    for target, ref_files in cross_file_refs.items():
        # Find which file defines the label
        for fpath, lines in all_files.items():
            if any(line.startswith(f"{target}:") for line in lines):
                if not any(f".global {target}" in l for l in lines):
                    new_lines = []
                    for line in lines:
                        if line.startswith(f"{target}:"):
                            new_lines.append(f".global {target}\n")
                            globals_added += 1
                        new_lines.append(line)
                    all_files[fpath] = new_lines
                break

    # Write modified files
    for fpath, lines in all_files.items():
        with open(fpath, "w") as f:
            f.writelines(lines)

    print(f"  {branches_fixed} branches resolved, {globals_added} .global added")


def _fix_2byte_branches_verified():
    """Replace .2byte branch encodings with proper instructions, verified against ROM.

    For each .2byte branch:
    1. Decode the raw branch encoding to compute the target address
    2. Find the .s file containing the target address
    3. Add a label at the exact position (verified by _compute_addresses_anchored)
    4. Only convert if the computed target matches the @ comment

    This avoids silent mismatches from incorrect byte counting.
    """
    nm_root = os.path.join(ROOT, "asm", "nonmatchings")

    # Collect all .s files with their content, labels, and address maps
    all_files = {}  # fpath -> lines
    all_labels = {}  # label_name -> fpath
    addr_maps = {}  # fpath -> {addr: line_idx}

    for mod in os.listdir(nm_root):
        mod_dir = os.path.join(nm_root, mod)
        if not os.path.isdir(mod_dir):
            continue
        for fname in os.listdir(mod_dir):
            if not fname.endswith(".s"):
                continue
            fpath = os.path.join(mod_dir, fname)
            with open(fpath) as f:
                lines = f.readlines()
            all_files[fpath] = lines

            # Get start address
            start_addr = 0
            for line in lines:
                m = _FUNC_LABEL_ADDR_RE.match(line.strip())
                if m:
                    start_addr = int(m.group(1), 16)
                    break
                m2 = _ADDR_LABEL_RE.match(line.strip())
                if m2:
                    start_addr = int(m2.group(1), 16)
                    break

            if start_addr:
                addrs = _compute_addresses_anchored(lines, start_addr)
                amap = {}
                for i, a in enumerate(addrs):
                    if a not in amap:
                        amap[a] = i
                addr_maps[fpath] = amap

            for line in lines:
                m = re.match(r"^(\w+):", line)
                if m:
                    all_labels[m.group(1)] = fpath

    # Decode each .2byte branch to get its source address and target address
    COND_CODES = {
        0xD0: 'beq', 0xD1: 'bne', 0xD2: 'bcs', 0xD3: 'bcc',
        0xD4: 'bmi', 0xD5: 'bpl', 0xD6: 'bvs', 0xD7: 'bvc',
        0xD8: 'bhi', 0xD9: 'bls', 0xDA: 'bge', 0xDB: 'blt',
        0xDC: 'bgt', 0xDD: 'ble',
    }

    labels_added = 0
    branches_fixed = 0

    for fpath, lines in list(all_files.items()):
        if fpath not in addr_maps:
            continue
        amap = addr_maps[fpath]
        addrs = _compute_addresses_anchored(lines, min(amap.values()) if amap else 0)

        # Re-compute addrs from start
        start_addr = 0
        for line in lines:
            m = _FUNC_LABEL_ADDR_RE.match(line.strip())
            if m:
                start_addr = int(m.group(1), 16)
                break
            m2 = _ADDR_LABEL_RE.match(line.strip())
            if m2:
                start_addr = int(m2.group(1), 16)
                break
        if not start_addr:
            continue
        addrs = _compute_addresses_anchored(lines, start_addr)

        file_labels = {m.group(1) for line in lines
                       for m in [re.match(r"^(\w+):", line)] if m}

        inserts = {}  # line_idx -> label to insert
        converts = {}  # line_idx -> (indent, insn, target)

        for i, line in enumerate(lines):
            m = _2BYTE_BRANCH_RE.match(line)
            if not m:
                continue

            indent, insn, target = m.group(1), m.group(2), m.group(3)
            tm = re.match(r"_([0-9A-Fa-f]+)$", target)
            if not tm:
                continue
            target_addr = int(tm.group(1), 16)

            # Check if target label already exists
            if target in all_labels or target in file_labels:
                converts[i] = (indent, insn, target)
                continue

            # Try to add label: find which file has target_addr
            placed = False
            for tfpath, tamap in addr_maps.items():
                if target_addr in tamap:
                    tidx = tamap[target_addr]
                    tlines = all_files[tfpath]
                    # Verify: check the line at tidx isn't a label or directive
                    # (it should be an instruction line)
                    if tfpath == fpath:
                        inserts[tidx] = target
                        file_labels.add(target)
                    else:
                        # Cross-file: insert label and mark .global
                        tlines_new = list(tlines)
                        tlines_new.insert(tidx, f".global {target}\n{target}:\n")
                        all_files[tfpath] = tlines_new
                        # Recompute addr_maps for this file
                        tstart = 0
                        for tl in tlines_new:
                            tm2 = _FUNC_LABEL_ADDR_RE.match(tl.strip())
                            if tm2:
                                tstart = int(tm2.group(1), 16)
                                break
                            tm3 = _ADDR_LABEL_RE.match(tl.strip())
                            if tm3:
                                tstart = int(tm3.group(1), 16)
                                break
                        if tstart:
                            new_addrs = _compute_addresses_anchored(tlines_new, tstart)
                            new_amap = {}
                            for j, a in enumerate(new_addrs):
                                if a not in new_amap:
                                    new_amap[a] = j
                            addr_maps[tfpath] = new_amap
                    all_labels[target] = tfpath
                    converts[i] = (indent, insn, target)
                    labels_added += 1
                    placed = True
                    break

            if not placed:
                pass  # leave as .2byte

        # Rebuild this file's lines
        if inserts or converts:
            new_lines = []
            for i, line in enumerate(lines):
                if i in inserts:
                    new_lines.append(f"{inserts[i]}:\n")
                if i in converts:
                    indent, insn, target = converts[i]
                    new_lines.append(f"{indent}{insn} {target}\n")
                    branches_fixed += 1
                else:
                    new_lines.append(line)
            all_files[fpath] = new_lines

    # Write all modified files
    for fpath, lines in all_files.items():
        with open(fpath, "w") as f:
            f.writelines(lines)

    print(f"  {labels_added} labels added, {branches_fixed} branches resolved")


def _fix_2byte_branches_per_file():
    """Replace .2byte branch encodings with proper instructions in per-function .s files.

    Runs AFTER function splitting so sub-function detection is unaffected.
    For each .s file:
    1. Compute ROM addresses using _compute_addresses_anchored
    2. Add missing branch target labels within the file
    3. Replace .2byte with proper branch instructions
    4. For targets in OTHER files, add .global declarations
    """
    nm_root = os.path.join(ROOT, "asm", "nonmatchings")

    # Pass 1: Collect all labels defined across all .s files
    all_labels = {}  # label_name -> filepath
    all_files = {}   # filepath -> lines
    for mod in os.listdir(nm_root):
        mod_dir = os.path.join(nm_root, mod)
        if not os.path.isdir(mod_dir):
            continue
        for fname in os.listdir(mod_dir):
            if not fname.endswith(".s"):
                continue
            fpath = os.path.join(mod_dir, fname)
            with open(fpath) as f:
                lines = f.readlines()
            all_files[fpath] = lines
            for line in lines:
                m = re.match(r"^(\w+):", line)
                if m:
                    all_labels[m.group(1)] = fpath

    # Pass 2: Collect all .2byte branch entries and their targets
    needed_targets = {}  # target_label -> set of source files
    for fpath, lines in all_files.items():
        for line in lines:
            m = _2BYTE_BRANCH_RE.match(line)
            if m:
                target = m.group(3)
                needed_targets.setdefault(target, set()).add(fpath)

    if not needed_targets:
        return

    # Pass 3: For missing labels, find which file should contain them and add them
    labels_added = 0
    globals_added = 0
    branches_fixed = 0

    for fpath, lines in all_files.items():
        # Get function start address
        start_addr = 0
        for line in lines:
            m = _FUNC_LABEL_ADDR_RE.match(line.strip())
            if not m:
                m2 = _ADDR_LABEL_RE.match(line.strip())
                if m2:
                    start_addr = int(m2.group(1), 16)
                    break
            else:
                start_addr = int(m.group(1), 16)
                break

        if start_addr == 0:
            continue

        addrs = _compute_addresses_anchored(lines, start_addr)

        # Find targets that should be in THIS file (address falls within)
        file_labels = {m.group(1) for line in lines
                       for m in [re.match(r"^(\w+):", line)] if m}

        # Build addr -> line_idx map for label insertion
        addr_to_line = {}
        for i, addr in enumerate(addrs):
            if addr not in addr_to_line:
                addr_to_line[addr] = i

        # Check which targets belong in this file
        inserts = {}  # line_idx -> label
        for target, source_files in needed_targets.items():
            if target in all_labels:
                # Label exists somewhere — if it's in this file, done.
                # If in another file and referenced from this file, add .global
                if target in file_labels:
                    continue  # already here
                if all_labels[target] != fpath and fpath in source_files:
                    # Need .global in the file that defines it
                    pass  # handled separately below
                continue

            # Label doesn't exist anywhere. Check if target addr is in this file
            m = re.match(r"_([0-9A-Fa-f]+)$", target)
            if not m:
                continue
            target_addr = int(m.group(1), 16)
            if target_addr in addr_to_line:
                inserts[addr_to_line[target_addr]] = target
                all_labels[target] = fpath
                file_labels.add(target)
                labels_added += 1

        # Replace .2byte with branch instructions (only if target label exists)
        new_lines = []
        for i, line in enumerate(lines):
            if i in inserts:
                new_lines.append(f"{inserts[i]}:\n")
            m = _2BYTE_BRANCH_RE.match(line)
            if m:
                indent, insn, target = m.group(1), m.group(2), m.group(3)
                if target in all_labels or target in file_labels:
                    new_lines.append(f"{indent}{insn} {target}\n")
                    branches_fixed += 1
                else:
                    new_lines.append(line)  # keep .2byte — target not found
            else:
                new_lines.append(line)

        if new_lines != lines:
            all_files[fpath] = new_lines

    # Pass 4: Add .global for cross-file references
    for target, source_files in needed_targets.items():
        if target not in all_labels:
            continue
        def_file = all_labels[target]
        if any(sf != def_file for sf in source_files):
            # Target is defined in def_file but referenced from another file
            lines = all_files[def_file]
            # Check if .global already exists
            if not any(f".global {target}" in l for l in lines):
                # Add .global before the label
                new_lines = []
                for line in lines:
                    if line.strip() == f"{target}:":
                        new_lines.append(f".global {target}\n")
                        globals_added += 1
                    new_lines.append(line)
                all_files[def_file] = new_lines

    # Write modified files
    for fpath, lines in all_files.items():
        with open(fpath, "w") as f:
            f.writelines(lines)

    print(f"  {labels_added} labels added, {branches_fixed} branches resolved, "
          f"{globals_added} .global directives added")


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

    print("[6.5/9] Fixing .2byte branch encodings...")
    _fix_2byte_in_split_files()

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
