#!/usr/bin/env python3
"""
Comprehensive audit of assembly files in the Klonoa: Empire of Dreams decomp project.
Checks for:
  1. Consecutive .2byte pairs that should be .4byte (GBA pointer detection)
  2. Sandwiched instructions (data-as-code)
  3. .2byte ROM verification (false positive detection)
  4. .4byte ROM verification (conversion verification)
  5. Isolated .2byte directives (potential false positives)
"""

import os
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

# --- Configuration ---
ASM_DIR = Path("/Users/macabeus/ApenasMeu/decompiler/klonoa-empire-of-dreams/asm")
ROM_PATH = Path("/Users/macabeus/ApenasMeu/decompiler/klonoa-empire-of-dreams/baserom.gba")

# GBA memory ranges
ROM_BASE = 0x08000000
IWRAM_START = 0x03000000
IWRAM_END = 0x03007FFF
EWRAM_START = 0x02000000
EWRAM_END = 0x0203FFFF
IO_START = 0x04000000
IO_END = 0x040003FF

# --- Load ROM ---
with open(ROM_PATH, "rb") as f:
    ROM_DATA = f.read()
ROM_SIZE = len(ROM_DATA)
ROM_END = ROM_BASE + ROM_SIZE

def is_gba_pointer(val):
    """Check if a 32-bit value is a valid GBA pointer."""
    if ROM_BASE <= val < ROM_END:
        return True
    if IWRAM_START <= val <= IWRAM_END:
        return True
    if EWRAM_START <= val <= EWRAM_END:
        return True
    if IO_START <= val <= IO_END:
        return True
    return False

def rom_read_u16(offset):
    """Read a 16-bit little-endian value from ROM at the given offset."""
    if 0 <= offset <= ROM_SIZE - 2:
        return struct.unpack_from("<H", ROM_DATA, offset)[0]
    return None

def rom_read_u32(offset):
    """Read a 32-bit little-endian value from ROM at the given offset."""
    if 0 <= offset <= ROM_SIZE - 4:
        return struct.unpack_from("<I", ROM_DATA, offset)[0]
    return None

# --- Regex patterns ---
# Function start: "FuncName: @ XXXXXXXX"
RE_FUNC_START = re.compile(r'^(\w+):\s+@\s+([0-9A-Fa-f]{8})')
# Label: "_XXXXXXXX:" at start of line (may have .4byte after it)
RE_LABEL = re.compile(r'^(_?[0-9A-Fa-f]{8}):')
# Also match labels like "FUN_XXXXXXXX:" or other named labels with address comment
RE_LABEL_WITH_ADDR = re.compile(r'^(\w+):\s+@\s+([0-9A-Fa-f]{8})')
# .2byte directive
RE_2BYTE = re.compile(r'^\s*\.2byte\s+0x([0-9A-Fa-f]{1,4})')
# .4byte directive (without label prefix)
RE_4BYTE_NO_LABEL = re.compile(r'^\s*\.4byte\s+0x([0-9A-Fa-f]{1,8})')
# .4byte directive (with label prefix like "_XXXXXXXX: .4byte ...")
RE_4BYTE_WITH_LABEL = re.compile(r'^_?[0-9A-Fa-f]{8}:\s*\.4byte\s+')
# .byte directive
RE_BYTE = re.compile(r'^\s*\.byte\s+')
# Any data directive
RE_DATA = re.compile(r'^\s*\.(2byte|4byte|byte)\s+')
# Instruction line (starts with optional whitespace, then a mnemonic)
RE_INSTRUCTION = re.compile(r'^\s+((?!\.)[a-z]\w*)')
# Thumb bl/blx instruction
RE_BL = re.compile(r'^\s+bl[x]?\s+')
# Label-only line
RE_LABEL_LINE = re.compile(r'^[_A-Za-z]\w*:')
# Directive lines (start with .)
RE_DIRECTIVE = re.compile(r'^\s*\.')
# .thumb_func or other assembler directives that don't emit bytes
RE_NO_EMIT_DIRECTIVE = re.compile(r'^\s*\.(thumb_func|global|align|text|code|syntax|section|type|size|pool)')
# thumb_func_start / non_word_aligned_thumb_func_start
RE_FUNC_START_MACRO = re.compile(r'^\s*(non_word_aligned_)?thumb_func_start\s+')
# Empty/comment lines
RE_EMPTY_OR_COMMENT = re.compile(r'^\s*([@#].*)?$')
# .align directive
RE_ALIGN = re.compile(r'^\s*\.align\s+(\d+)')


def compute_byte_size(line):
    """
    Compute how many bytes a line emits.
    Returns (size, is_data_directive, is_instruction, is_label, is_nothing).
    """
    stripped = line.strip()
    if not stripped or stripped.startswith('@') or stripped.startswith('#'):
        return (0, False, False, False, True)

    # thumb_func_start / non_word_aligned_thumb_func_start macros
    if RE_FUNC_START_MACRO.match(stripped):
        return (0, False, False, False, True)

    # Label line (may have content after the colon)
    if RE_LABEL_LINE.match(stripped):
        # Check if there's content after the label
        after_colon = stripped.split(':', 1)[1].strip()
        if after_colon:
            # e.g., "_XXXXXXXX: .4byte 0x..."
            if re.match(r'\.4byte\s+', after_colon):
                return (4, True, False, True, False)
            if re.match(r'\.2byte\s+', after_colon):
                return (2, True, False, True, False)
            if re.match(r'@', after_colon):
                return (0, False, False, True, False)
            # Instruction after label like "FUN_xxx: beq ..."
            return (2, False, True, True, False)
        return (0, False, False, True, False)

    # .4byte
    if re.match(r'\s*\.4byte\s+', stripped):
        return (4, True, False, False, False)

    # .2byte
    if re.match(r'\s*\.2byte\s+', stripped):
        return (2, True, False, False, False)

    # .byte - count the number of byte values
    m = re.match(r'\s*\.byte\s+(.*)', stripped)
    if m:
        # Count comma-separated values
        vals = m.group(1).split(',')
        return (len(vals), True, False, False, False)

    # .align - can emit padding bytes, but we skip address tracking for this
    if RE_ALIGN.match(stripped):
        return (0, False, False, False, True)  # We handle alignment via labels

    # Non-emitting directives
    if RE_NO_EMIT_DIRECTIVE.match(stripped):
        return (0, False, False, False, True)

    # Other directives we don't know about
    if RE_DIRECTIVE.match(stripped):
        return (0, False, False, False, True)

    # bl/blx = 4 bytes (32-bit Thumb encoding)
    if RE_BL.match(line):
        return (4, False, True, False, False)

    # Regular Thumb instruction = 2 bytes
    if RE_INSTRUCTION.match(line):
        return (2, False, True, False, False)

    return (0, False, False, False, True)


def parse_file(filepath):
    """
    Parse a .s file and return structured line data with address tracking.
    Returns list of dicts with: line_num, text, address (or None), byte_size, etc.
    """
    with open(filepath, 'r') as f:
        lines = f.readlines()

    result = []
    current_addr = None  # Current tracked address
    has_anchor = False

    for i, raw_line in enumerate(lines):
        line = raw_line.rstrip('\n')
        line_num = i + 1

        # Check for function start with address
        m = RE_FUNC_START.match(line)
        if not m:
            m = RE_LABEL_WITH_ADDR.match(line)
        if m:
            addr = int(m.group(2), 16)
            current_addr = addr
            has_anchor = True

        # Check for address label like "_XXXXXXXX:"
        m_label = RE_LABEL.match(line)
        if m_label and not RE_LABEL_WITH_ADDR.match(line):
            label_addr = int(m_label.group(1).lstrip('_'), 16)
            # Validate: should be in ROM range and close to current address
            if ROM_BASE <= label_addr < ROM_END:
                current_addr = label_addr
                has_anchor = True

        size, is_data, is_instr, is_label, is_nothing = compute_byte_size(line)

        entry = {
            'line_num': line_num,
            'text': line,
            'address': current_addr if has_anchor else None,
            'byte_size': size,
            'is_data': is_data,
            'is_instruction': is_instr,
            'is_label': is_label,
            'is_nothing': is_nothing,
            'has_anchor': has_anchor,
        }
        result.append(entry)

        # Advance address
        if current_addr is not None and size > 0:
            current_addr += size

    return result


def get_relative_path(filepath):
    """Get path relative to ASM_DIR."""
    return str(filepath.relative_to(ASM_DIR))


# --- Collect all .s files ---
all_files = sorted(ASM_DIR.rglob("*.s"))
# Filter to only nonmatchings/ and matchings/ subdirectories
all_files = [f for f in all_files if '/nonmatchings/' in str(f) or '/matchings/' in str(f)]

print(f"Scanning {len(all_files)} assembly files...")
print(f"ROM size: {ROM_SIZE} bytes (0x{ROM_SIZE:X})")
print(f"ROM range: 0x{ROM_BASE:08X} - 0x{ROM_END:08X}")
print()

# --- Results storage ---
check1_results = defaultdict(list)  # file -> [(line_num, low_hex, high_hex, combined)]
check2_results = defaultdict(list)  # file -> [(line_num, text, context)]
check3_results = defaultdict(list)  # file -> [(line_num, text, expected, actual, address)]
check4_results = defaultdict(list)  # file -> [(line_num, text, expected, actual, address)]
check5_results = defaultdict(list)  # file -> [(line_num, text)]

files_processed = 0

for filepath in all_files:
    rel_path = get_relative_path(filepath)
    parsed = parse_file(filepath)
    files_processed += 1

    # ===== CHECK 1: Consecutive .2byte pairs that should be .4byte =====
    for i in range(len(parsed) - 1):
        entry = parsed[i]
        next_entry = parsed[i + 1]

        m1 = RE_2BYTE.match(entry['text'].strip())
        m2 = RE_2BYTE.match(next_entry['text'].strip())

        if m1 and m2:
            # Make sure no label between them
            has_label_between = False
            # They're consecutive lines, so just check the next line isn't a label
            # (labels would be on their own line)
            low = int(m1.group(1), 16)
            high = int(m2.group(1), 16)
            combined = (high << 16) | low

            if is_gba_pointer(combined):
                check1_results[rel_path].append((
                    entry['line_num'],
                    f"0x{low:04X}",
                    f"0x{high:04X}",
                    f"0x{combined:08X}"
                ))

    # ===== CHECK 2: Sandwiched instructions =====
    for i in range(len(parsed)):
        entry = parsed[i]
        if not entry['is_instruction']:
            continue
        # Also skip if this line has a label on it
        if entry['is_label']:
            continue

        # Look for data directives in preceding and following non-empty, non-label lines (within ±2)
        has_data_before = False
        has_data_after = False

        # Check up to 2 lines before
        for j in range(max(0, i - 2), i):
            prev = parsed[j]
            if prev['is_nothing'] or prev['is_label']:
                continue
            if prev['is_data']:
                has_data_before = True
                break

        # Check up to 2 lines after
        for j in range(i + 1, min(len(parsed), i + 3)):
            nxt = parsed[j]
            if nxt['is_nothing'] or nxt['is_label']:
                continue
            if nxt['is_data']:
                has_data_after = True
                break

        if has_data_before and has_data_after:
            # Gather context (±2 lines)
            ctx_start = max(0, i - 2)
            ctx_end = min(len(parsed), i + 3)
            context_lines = []
            for j in range(ctx_start, ctx_end):
                marker = ">>>" if j == i else "   "
                context_lines.append(f"{marker} L{parsed[j]['line_num']}: {parsed[j]['text']}")
            check2_results[rel_path].append((
                entry['line_num'],
                entry['text'].strip(),
                "\n".join(context_lines)
            ))

    # ===== CHECK 3: .2byte ROM verification =====
    for entry in parsed:
        m = RE_2BYTE.match(entry['text'].strip())
        if not m:
            continue
        if entry['address'] is None:
            continue
        if not entry['has_anchor']:
            continue

        addr = entry['address']
        rom_offset = addr - ROM_BASE

        # Validate the ROM offset is reasonable
        if rom_offset < 0 or rom_offset >= ROM_SIZE - 1:
            continue

        expected = int(m.group(1), 16)
        actual = rom_read_u16(rom_offset)

        if actual is not None and actual != expected:
            check3_results[rel_path].append((
                entry['line_num'],
                entry['text'].strip(),
                f"0x{expected:04X}",
                f"0x{actual:04X}",
                f"0x{addr:08X}"
            ))

    # ===== CHECK 4: .4byte ROM verification (our conversions only, not literal pool) =====
    for entry in parsed:
        line_stripped = entry['text'].strip()

        # Skip .4byte with label prefix (literal pool entries from Luvdis)
        if RE_4BYTE_WITH_LABEL.match(entry['text'].lstrip()):
            continue
        # Also skip if the raw line starts with a label
        if RE_LABEL_LINE.match(entry['text'].lstrip()):
            continue

        m = RE_4BYTE_NO_LABEL.match(line_stripped)
        if not m:
            continue
        if entry['address'] is None:
            continue
        if not entry['has_anchor']:
            continue

        addr = entry['address']
        rom_offset = addr - ROM_BASE

        if rom_offset < 0 or rom_offset >= ROM_SIZE - 3:
            continue

        expected = int(m.group(1), 16)
        actual = rom_read_u32(rom_offset)

        if actual is not None and actual != expected:
            check4_results[rel_path].append((
                entry['line_num'],
                line_stripped,
                f"0x{expected:08X}",
                f"0x{actual:08X}",
                f"0x{addr:08X}"
            ))

    # ===== CHECK 5: Isolated .2byte directives =====
    for i, entry in enumerate(parsed):
        m = RE_2BYTE.match(entry['text'].strip())
        if not m:
            continue

        # Check ±3 lines for other .2byte or .4byte directives
        has_neighbor = False
        for j in range(max(0, i - 3), min(len(parsed), i + 4)):
            if j == i:
                continue
            neighbor = parsed[j]
            neighbor_stripped = neighbor['text'].strip()
            if RE_2BYTE.match(neighbor_stripped) or re.match(r'\.4byte\s+', neighbor_stripped) or RE_4BYTE_WITH_LABEL.match(neighbor['text'].lstrip()):
                has_neighbor = True
                break

        if not has_neighbor:
            check5_results[rel_path].append((
                entry['line_num'],
                entry['text'].strip()
            ))


# ===== REPORTING =====

def print_check_header(num, title, results):
    total = sum(len(v) for v in results.values())
    file_count = len(results)
    print("=" * 80)
    print(f"CHECK {num}: {title}")
    print(f"  Total findings: {total} across {file_count} files")
    print("=" * 80)
    return total

def print_file_findings(results, formatter, max_examples=5):
    if not results:
        print("  (no findings)")
        print()
        return
    for filepath in sorted(results.keys()):
        findings = results[filepath]
        print(f"\n  --- {filepath} ({len(findings)} findings) ---")
        for finding in findings[:max_examples]:
            formatter(finding)
        if len(findings) > max_examples:
            print(f"  ... and {len(findings) - max_examples} more")
    print()


# --- Check 1 ---
total1 = print_check_header(1, "Consecutive .2byte pairs that should be .4byte (GBA pointers)", check1_results)
def fmt1(f):
    line_num, low, high, combined = f
    print(f"    Line {line_num}: {low} + {high} = {combined}")
print_file_findings(check1_results, fmt1)

# --- Check 2 ---
total2 = print_check_header(2, "Sandwiched instructions (possible data-as-code)", check2_results)
def fmt2(f):
    line_num, text, context = f
    print(f"    Line {line_num}: {text}")
    for ctx_line in context.split('\n'):
        print(f"      {ctx_line}")
    print()
print_file_findings(check2_results, fmt2)

# --- Check 3 ---
total3 = print_check_header(3, ".2byte ROM verification (value mismatch)", check3_results)
def fmt3(f):
    line_num, text, expected, actual, addr = f
    print(f"    Line {line_num} @ {addr}: {text}")
    print(f"      ASM value: {expected}  ROM value: {actual}")
print_file_findings(check3_results, fmt3)

# --- Check 4 ---
total4 = print_check_header(4, ".4byte ROM verification (unlabeled, value mismatch)", check4_results)
def fmt4(f):
    line_num, text, expected, actual, addr = f
    print(f"    Line {line_num} @ {addr}: {text}")
    print(f"      ASM value: {expected}  ROM value: {actual}")
print_file_findings(check4_results, fmt4)

# --- Check 5 ---
total5 = print_check_header(5, "Isolated .2byte directives (no neighbor data within ±3 lines)", check5_results)
def fmt5(f):
    line_num, text = f
    print(f"    Line {line_num}: {text}")
print_file_findings(check5_results, fmt5)

# --- Summary ---
print("=" * 80)
print("SUMMARY")
print("=" * 80)
print(f"  Files processed: {files_processed}")
print(f"  Check 1 (consecutive .2byte -> .4byte):  {total1} findings in {len(check1_results)} files")
print(f"  Check 2 (sandwiched instructions):        {total2} findings in {len(check2_results)} files")
print(f"  Check 3 (.2byte ROM mismatch):            {total3} findings in {len(check3_results)} files")
print(f"  Check 4 (.4byte ROM mismatch):            {total4} findings in {len(check4_results)} files")
print(f"  Check 5 (isolated .2byte):                {total5} findings in {len(check5_results)} files")
print()
