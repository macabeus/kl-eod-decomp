# Data Decoded as Instructions in Assembly Files

## Background

### ARM Thumb encoding and inline data

ARM Thumb instructions are [16-bit (2 bytes)](https://azeria-labs.com/arm-instruction-set-part-3/) each. Compilers routinely place data — literal pools (constant values loaded via `ldr Rd, [PC, #offset]`) and jump tables (arrays of branch target addresses) — directly within the `.text` section, interleaved with executable code.

The ARM architecture defines [mapping symbols](https://sourceware.org/binutils/docs/as/ARM-Mapping-Symbols.html) (`$a`, `$t`, `$d`) to distinguish code from data within the same section. The assembler inserts `$t` at the start of Thumb code regions and `$d` at the start of data regions. The [ARM ELF ABI specification](https://github.com/ARM-software/abi-aa/blob/main/aaelf32/aaelf32.rst) (section 5.5.5) formally defines these symbols so that disassemblers, debuggers, and linkers can correctly interpret mixed code-and-data sections.

When a disassembler encounters a `.text` section without mapping symbols (as when disassembling a raw ROM binary), it has no way to distinguish data from code. It decodes every 2-byte halfword as a Thumb instruction — producing nonsensical mnemonics for bytes that are actually constants or addresses.

### What Luvdis does

[Luvdis](https://github.com/aarant/luvdis), the GBA-specialized disassembler used by this project, does not have a concept of inline data regions. It decodes all bytes in the code section as Thumb instructions. Literal pool entries that Luvdis can identify (via `ldr` PC-relative references) are correctly emitted as `.4byte` directives, but **jump table entries and other non-referenced data** are decoded as instruction mnemonics.

### How this affects decompilation

Data decoded as instructions produces assembly files where jump tables look like nonsensical code:

```asm
mov pc, r0              @ indirect jump through jump table
_080441E8: .4byte 0x03005494
_080441EC: .4byte 0x080441F0
tst r4, r0              @ ← NOT a tst instruction!
lsrs r4, r0, #0x20     @ ← NOT a shift instruction!
muls r4, r7             @ ← NOT a multiply!
lsrs r4, r0, #0x20
```

The `tst r4, r0` is actually `0x4204` — the low 16 bits of ROM address `0x08044204`, the first entry in a 5-entry jump table. The `lsrs r4, r0, #0x20` is `0x0804` — the high 16 bits. Together they form the 32-bit pointer `0x08044204`.

This causes several problems:

1. **m2c produces wrong C code** — literal pool entries decoded as instructions can't be resolved to their IWRAM/ROM addresses. m2c treats the label as a function pointer instead of a constant (see [m2c example](#m2c-literal-pool-impact) below). For `mov pc, r0` jump tables, m2c fails entirely with *"Unable to determine jump table"*.
2. **objdiff counts data as mismatched instructions** — inflating the diff and making it harder to identify real code differences
3. **AI decompilation (Mizuchi) is confused** — the model tries to write C code that reproduces `tst`/`muls`/`bics` instructions that don't exist in the original source
4. **Jump table structure is invisible** — `switch` statements can't be identified from the assembly

## Concrete example: m2c before and after

The function below reads an input register and conditionally increments a flag byte. Its literal pool constants (`gGameFlags` at `0x03005400` and `gEntityArray` at `0x03002920`) are stored inline after the function body.

### Before (data-as-code)

When Luvdis decodes the literal pool bytes as Thumb instructions, the assembly looks like:

```asm
UpdateFlag:
    push {r4, lr}
    ldr r4, _pool_flags             @ → gGameFlags
    ldr r0, _pool_input             @ → gEntityArray
    ldrh r0, [r0, #0x00]
    movs r1, #0x08
    ands r0, r1
    cmp r0, #0x00
    beq .Lend
    ldrb r0, [r4, #0x00]
    adds r0, #0x01
    strb r0, [r4, #0x00]
.Lend:
    pop {r4, pc}
_pool_flags:
    strb r4, [r0, r0]              @ ← DATA: 0x5400 (low half of 0x03005400)
    lsls r0, r0, #0x0C            @ ← DATA: 0x0300 (high half)
_pool_input:
    cmp r1, #0x20                  @ ← DATA: 0x2920 (low half of 0x03002920)
    lsls r0, r0, #0x0C            @ ← DATA: 0x0300 (high half)
```

m2c output (`m2c -t gba`):

```c
void UpdateFlag(void) {
    if (*_pool_input & 8) {
        *_pool_flags += 1;
    }
}
```

m2c cannot resolve the literal pool values — it emits `*_pool_flags` and `*_pool_input` as unresolved label references. The generated code has no IWRAM addresses, no types, and compiles to incorrect memory accesses.

### After (correct data directives)

```asm
UpdateFlag:
    push {r4, lr}
    ldr r4, _pool_flags
    ldr r0, _pool_input
    ldrh r0, [r0, #0x00]
    movs r1, #0x08
    ands r0, r1
    cmp r0, #0x00
    beq .Lend
    ldrb r0, [r4, #0x00]
    adds r0, #0x01
    strb r0, [r4, #0x00]
.Lend:
    pop {r4, pc}
_pool_flags: .4byte 0x03005400     @ gGameFlags (IWRAM)
_pool_input: .4byte 0x03002920     @ gEntityArray (IWRAM)
```

m2c output (`m2c -t gba`):

```c
void UpdateFlag(void) {
    if (*(u16 *)0x03002920 & 8) {
        *(u8 *)0x03005400 += 1;
    }
}
```

m2c resolves both constants to their IWRAM addresses, infers the correct types (`u16 *` for the halfword load, `u8 *` for the byte load/store), and produces compilable C.

### Jump tables

For `mov pc, r0` dispatch (used by `switch` statements), m2c reports *"Unable to determine jump table"* regardless of data format — this GBA pattern is not supported. However, `.4byte` entries make the table readable to Mizuchi and human reviewers:

```asm
@ Before: nonsensical instructions          @ After: clear jump table
    tst r4, r0                                  .4byte 0x08044204
    lsrs r4, r0, #0x20                         .4byte 0x0804437C
    muls r4, r7                                 .4byte 0x08044388
    lsrs r4, r0, #0x20                         .4byte 0x08044474
```

## The fix: multi-step pipeline

### Step 1: Detect data regions with Ghidra (`scripts/detect_data_regions.py`)

A game-agnostic Python script runs [Ghidra](https://ghidra-sre.org/) in headless mode to analyze the ROM:

```bash
python3 scripts/detect_data_regions.py \
  --ghidra /path/to/ghidra \
  --rom baserom.gba \
  --config klonoa-eod-decomp.toml \
  --code-end 0x52000 \
  --functions functions_merged.cfg
```

The script:
1. Imports the ROM with `ARM:LE:32:v4t` processor
2. Seeds analysis with the GBA entry point at ROM offset `0xC0` (ARM mode) and Thumb function entry points from `functions_merged.cfg`
3. Runs `analyzeAll()` which triggers Ghidra's `ARM Constant Reference Analyzer` and `Decompiler Switch Analysis` (the switch analyzer alone takes ~54 seconds)
4. Queries the listing for `Data` code units within the code section
5. Filters out regions < 8 bytes (already `.4byte` in Luvdis output) and regions overlapping known function entries
6. Writes `[[data_regions]]` entries to the TOML config

The script is **portable** — it takes `--ghidra`, `--rom`, `--config` as parameters. No Klonoa-specific logic. Any GBA decomp project can use it.

For Klonoa: Empire of Dreams, Ghidra identified **660 data regions** in the code section.

### Step 2: Convert data-as-code in `generate_asm.py`

During the assembly file writing phase (`_write_asm_files`), the `_apply_data_regions` function scans each function's lines. For each instruction that is data-as-code, it replaces the instruction mnemonic with a `.2byte` data directive containing the actual ROM bytes.

#### How it identifies data-as-code

The conversion uses a **5-layer safety check** to ensure only genuine data-as-code is converted:

1. **Instruction check**: the line must be an actual instruction mnemonic (not already a `.4byte`/`.2byte` directive, label, or assembler macro)

2. **ROM byte verification**: the ROM halfword at the computed address must be `0x08XX` — the high byte of a GBA ROM address. This is checked using `struct.unpack_from("<H", rom_data, rom_offset)`.

3. **Pointer pair validation**: the preceding halfword in the ROM must form a valid 32-bit GBA pointer when combined with the current `0x08XX` halfword. Valid ranges: ROM (`0x08000000` to `0x08000000 + rom_size`), IWRAM (`0x03000000`–`0x03007FFF`), EWRAM (`0x02000000`–`0x0203FFFF`), I/O (`0x04000000`–`0x040003FF`).

4. **Instruction encoding verification**: the ROM byte `0x08XX` must exactly match the expected encoding of the instruction text. For `lsrs Rd, Rm, #32`, the Thumb encoding is `0x0800 | Rd | (Rm << 3)`. If the ROM byte doesn't match, the address computation drifted and the conversion is skipped.

5. **Low halfword pairing**: for the preceding low halfword of each converted pair, the near-pool guard requires the line to be within ±5 lines of an existing or newly-identified data directive (`.4byte`/`.2byte` or a `convert` set entry), AND the 32-bit word at the ROM address must be a valid GBA pointer (same ranges as check 3).

#### How it handles address computation

The core challenge is mapping source lines to ROM addresses. The `_compute_addresses_anchored` function:

- Counts bytes from the function start (2 per Thumb instruction, 4 per `bl`/`.4byte`)
- **Resets at every embedded label** (`_XXXXXXXX:` and `FUN_XXXXXXXX: @ XXXXXXXX`) — these embed the original ROM address and correct for two sources of drift:
  - Luvdis placing labels after their instruction instead of before
  - `.align 2, 0` in `thumb_func_start` emitting 0 or 2 padding bytes

The label anchoring uses a strict label-aware byte size function (`_line_byte_size_strict`) that correctly returns 0 for label lines with trailing comments (e.g., `FUN_08000470: @ 08000470`), which the original `_line_byte_size` incorrectly counted as 2 bytes.

When `rom_data` is provided, `_compute_addresses_anchored` calls `_validate_addresses` which scores the computed addresses against known instruction encodings (`push` → `0xB5XX`, `pop` → `0xBCXX`/`0xBDXX`, `bx` → `0x47XX`, `bl` → `0xF0XX`–`0xF7XX`). If a -2 offset scores strictly better, all addresses are shifted. This corrects the systematic drift in `non_word_aligned_thumb_func_start` functions.

### Step 3: Convert trailing data (`_convert_trailing_data`) — DISABLED

~~After the pointer-pair passes, a fourth pass converts any remaining instruction lines after the function's last return instruction.~~

This pass is **disabled** (see Bug 9 below). Its address computation was unreliable for non-word-aligned functions, producing wrong literal pool values that corrupted 10 functions. The pointer-pair passes (1–3) handle the bulk of data-as-code conversion; the remaining trailing data stays as instruction mnemonics which assemble to identical ROM bytes.

### Step 4: Fix `ldr` alignment (`_fix_ldr_alignment`)

After all assembly file generation and post-processing (steps 4–8), a final pass converts `ldr Rd, _LABEL` instructions to `.inst.n ENCODING` in functions using `non_word_aligned_thumb_func_start`. This bypasses the assembler's word-alignment check for `ldr` PC-relative targets (see Bug 8 below). The encoding is verified against the ROM by checking that the halfword at the computed address ±4 bytes is a valid Thumb6 `ldr` with the correct destination register.

## Identifying false positives

### Why false positives occur

Any Thumb instruction whose encoding happens to be `0x08XX` looks like a ROM address high halfword. `lsrs Rd, Rm, #32` encodes as `0x08XX` in the [Thumb Format 1 (move shifted register)](https://azeria-labs.com/arm-instruction-set-part-3/) encoding. This is a **legitimate instruction** — it extracts the sign bit of a register. Many functions use `lsrs r0, r0, #0x20` as a real shift operation.

### How the checks distinguish real code from data

1. **Real `lsrs #0x20`**: the ROM byte at the instruction's address matches the encoding (`0x0800 | Rd | Rm<<3`), BUT the preceding halfword does NOT form a valid ROM pointer. Example: `lsrs r0, r0, #0x20` at a normal code address where the preceding instruction is `adds r1, #5` (= `0x3105`) → combined `0x0800_3105` is a valid ROM address range but the encoding check AND near-pool check both fail.

2. **Data `lsrs #0x20`**: the ROM byte matches, AND the preceding halfword forms a valid ROM pointer (e.g., `0x0804_437C`), AND the line is near existing data directives. All 5 checks pass → convert.

3. **Address drift**: the anchored computation gives an address 2 bytes off. The ROM byte at the wrong address may or may not be `0x08XX`. The encoding check catches this — if the ROM byte is `0x0808` but the instruction text says `lsrs r7, r0, #0x20` (which encodes as `0x0807`), the mismatch triggers a skip.

## Bugs found and fixed

### Bug 1: Regex mismatch on `existing_high` detection

The second pass looks for already-converted `.2byte 0x08XX` lines (from previous conversion) whose preceding low halfword was not yet converted. The regex used `re.match(r"\t\.2byte 0x08...", line.strip())` — but `.strip()` removes the leading tab, so the `\t` in the pattern never matched. The `existing_high` set was always empty, preventing re-runs from picking up missing low halfwords.

**Fix:** Remove `\t` from the regex: `re.match(r"\.2byte 0x08[0-9A-Fa-f]{2}$", line.strip())`.

### Bug 2: Narrow near-pool window for low halfword pairing

The `near_pool` set (lines within ±5 of a data directive) was built only from the original `.4byte`/`.2byte` entries in the raw Luvdis output. In functions with large jump tables (like `HandlePauseMenuInput` with 120+ entries), only the first few low halfwords near the literal pool were within range. Low halfwords more than 5 lines from the nearest original `.4byte` were silently skipped.

For example, with a literal pool ending at line 116:
- Lines 117–121: within ±5 of `.4byte` → low halfwords converted ✓
- Lines 123+: outside ±5 → low halfwords skipped ✗

**Fix:** After building the `convert` set (high halfwords that will become `.2byte`), expand `near_pool` to include lines ±5 from each `convert` entry. This propagates the "near data" property through the entire jump table, allowing all low halfwords to be paired.

**Impact:** HandlePauseMenuInput went from 120 unconverted `add r3, sp` instructions to 0. Across all files, total `.2byte` conversions increased from ~900 to ~12,400.

### Bug 3: `.inst` directive not counted as 2 bytes

Luvdis emits `.inst 0xNNNN` for halfwords it can't decode as standard Thumb instructions. The `_line_byte_size` function treated `.inst` as 0 bytes (because it starts with `.`), causing address computation to drift past every `.inst` line. Subsequent ROM byte verification would read from wrong addresses, silently skipping valid conversions.

Only 15 `.inst` lines exist across 6 files, but the drift affected all lines after each `.inst` within a label-anchor gap.

**Fix:** `_line_byte_size_strict` (used by `_compute_addresses_anchored` for data conversion) now returns 2 for `.inst` lines. The base `_line_byte_size` (used by split/merge phases) is unchanged to preserve function boundary decisions.

### Bug 4: IWRAM pointer low halfword misidentified as ROM high halfword

When a `lsrs Rd, Rm, #0x20` instruction (encoding `0x08XX`) is the LOW halfword of an IWRAM pointer (e.g., `0x0300082C`), the first pass incorrectly treated it as the HIGH halfword of a ROM pointer. The preceding halfword happened to form a value in the `0x08XXXXXX` ROM range, passing the pointer validation.

**Fix:** Before accepting a `lsrs #0x20` as a high halfword, check if the NEXT instruction is `lsls r0, r0, #0x0C` (encoding `0x0300`). If so, the `lsrs` is a low halfword of an IWRAM pointer — skip it and let the pairing pass handle it.

### Bug 5: Consecutive `0x08XX` entries couldn't pair

When both halfwords of a ROM pointer encode as `0x08XX` (e.g., `0x08060808` = `lsrs r0, r1, #0x20` + `lsrs r6, r0, #0x20`), both entered the `convert` set as high halfword candidates. The low halfword pairing requires `k not in convert`, so neither could be the other's partner.

**Fix:** A third pass scans consecutive `convert` entries and pairs them if they form a valid GBA pointer. Only 1 case existed (VBlankDMA_Level18), but the fix handles it generically.

### Bug 6: `non_word_aligned_thumb_func_start` address drift

`_compute_addresses_anchored` anchored to function label addresses (`NAME: @ XXXXXXXX`), but for `non_word_aligned_thumb_func_start` functions these addresses were 2 bytes too high. The drift originated from Luvdis including `.align 2, 0` padding in address tracking that the `non_word_aligned` macro does not emit, or from accumulated drift in preceding functions' data-as-code.

All ROM byte checks in `_find_high_halfwords` and `_find_low_halfwords` failed for these functions because they were reading the wrong ROM offsets. This blocked conversion of ~116 functions including VBlankHandlerMinimal and VBlankHandler.

**Fix:** `_compute_addresses_anchored` now accepts optional `rom_data` and calls `_validate_addresses`, which scores the current addresses and a -2 offset against known instruction encodings. If -2 scores strictly better, all addresses are shifted.

### Bug 7: `DeadCode_0804bb86` forced `$t` in FreeGfxBuffer's literal pool

`FUN_0804bb86` (renamed `DeadCode_0804bb86`) was detected by Luvdis as a function entry point at `0x0804BB86`, but it's actually the high halfword (`0x0300`) of FreeGfxBuffer's literal pool `0x030034A0`. The `non_word_aligned_thumb_func_start` macro in its `.s` file emitted a `$t` (Thumb code) mapping symbol at offset `0x3a92` in `gfx.o`, in the middle of what should be a `$d` (data) region.

This caused objdiff to disassemble FreeGfxBuffer's literal pool as Thumb instructions on the target side (`.hword 0x34a0`) while the compiled side correctly showed `.word 0x030034a0`. The bytes were bit-for-bit identical, but the ARM ELF mapping symbols told the disassembler to interpret them differently.

**Fix:** `_apply_fixups` replaces `DeadCode_0804bb86.s` with a bare `.global` label stub (no `thumb_func_start`, no `.thumb`). The stub is **conditional**: when FreeGfxBuffer is compiled from C, the compiler emits a full `.4byte 0x030034A0` and the stub is empty; when FreeGfxBuffer uses `INCLUDE_ASM`, its `.byte 0xA0, 0x34` only covers 2 bytes and the stub emits `.2byte 0x0300` for the remaining high halfword.

### Bug 8: `ldr` alignment errors with `non_word_aligned_thumb_func_start`

GNU `arm-none-eabi-as` (all versions) rejects Thumb `ldr Rd, [PC, #imm]` instructions when the literal pool target is at a non-word-aligned section offset. The Luvdis fork emits `non_word_aligned_thumb_func_start` for functions at non-4-byte-aligned ROM addresses, which omits `.align 2, 0`. Without this alignment, literal pool `.4byte` entries can land at section offsets that are `2 mod 4`, causing the assembler to reject all `ldr` pool references in those functions.

This is NOT specific to newer assemblers — the check (`if (value & 3) as_bad(...)`) exists in GNU binutils since at least version 2.38. The CI for commit `c841d4c` passed because its specific function merges happened to produce word-aligned pools; subsequent commits changed the merge pattern and triggered the error.

**Key insight:** The `non_word_aligned_thumb_func_start` macro is *required* for correct ROM layout — using `thumb_func_start` (with `.align 2, 0`) would insert padding bytes that shift function addresses. But the assembler cannot verify `ldr` target alignment when the function's section offset is unknown at assembly time (it depends on all preceding `INCLUDE_ASM` blocks in the same compilation unit).

**Fix:** `_fix_ldr_alignment()` runs as step 8.5 (after all other assembly processing) and converts `ldr Rd, _LABEL` to `.inst.n ENCODING` in all functions using `non_word_aligned_thumb_func_start`. The encoding is looked up from the ROM at the computed address ±4 bytes, verified by checking that the ROM halfword is a valid Thumb6 `ldr` encoding (`0b01001` in bits 15–11) with the correct `Rd` register. This bypasses the assembler's alignment check while producing identical bytes.

**Impact:** 1,489 `ldr` instructions converted across ~116 non-word-aligned functions.

### Bug 9: `_convert_trailing_data` corrupted literal pools

The trailing data conversion pass (Pass 4 of `_apply_data_regions`) was designed to convert instruction mnemonics after the last return instruction to data directives. However, it computed ROM addresses using `_compute_addresses_anchored` which has unreliable offsets for non-word-aligned functions — the label addresses drift by 2+ bytes due to accumulated data-as-code in preceding functions.

This caused the pass to read ROM bytes at wrong addresses, writing incorrect `.2byte` values into 10 literal pool regions across functions including `EntityBossPhaseC`, `ProcessStaticBGScroll`, `StreamCmd_SetTimerAndMode`, `InitFadeTransition`, and `MainGameFrameLoop`. The corrupted pools had shifted data, missing entries, and bleeding across function boundaries.

Additionally, extending `.byte` literal pool entries to `.4byte` (e.g., `_0804BB84: .byte 0xA0, 0x34` → `.4byte 0x030034A0`) added 2 extra bytes per entry, causing a 4-byte ROM size increase that shifted all subsequent sections.

**Fix:** Pass 4 (`_convert_trailing_data`) is disabled. The pointer-pair detection passes (1–3) handle the bulk of data-as-code conversion with ROM-verified addresses and are not affected by the drift. The remaining unconverted trailing data (alignment NOPs, IWRAM literal pools without pointer-pair patterns) stays as instruction mnemonics — these assemble to identical bytes and only affect ARM ELF mapping symbols, not the ROM content.

### Improvement: `.4byte` merging for readability

Paired halfwords that form 32-bit ROM pointers are now emitted as a single `.4byte` directive instead of two `.2byte` directives. This makes jump tables immediately recognizable:

```asm
@ Before (.2byte pairs — opaque):       @ After (.4byte — readable):
    .2byte 0x4204                            .4byte 0x08044204
    .2byte 0x0804                            .4byte 0x0804437C
    .2byte 0x437C                            .4byte 0x08044388
    .2byte 0x0804
    .2byte 0x4388
    .2byte 0x0804
```

This improves the output for decompilation tools: Mizuchi (AI decompilation) can immediately identify `.4byte 0x08XXXXXX` entries after a `mov pc, r0` as a jump table (i.e. a `switch` statement), whereas `.2byte` pairs require manual reassembly to discover the pointer values. m2c cannot handle the `mov pc, r0` dispatch pattern regardless, but `.4byte` still helps human readers.

Merging also applies to IWRAM literal pool entries. For example, `_pool_state: .4byte 0x03005220` is immediately recognizable as an IWRAM address, whereas `_pool_state:` followed by `tst r4, r0` + `lsrs r4, r0, #0x20` is not (see [m2c literal pool impact](#m2c-literal-pool-impact) above).

**Constraint:** merging only happens when the low and high halfword are on directly adjacent lines. When a label appears between them (e.g., a literal pool label at the high halfword's address), they stay as separate `.2byte` directives to preserve label alignment.

## Results

### Conversion statistics

| Metric | Count |
|---|---|
| Ghidra data regions detected | 660 |
| Files with conversions | 136 of 475 |
| High halfwords converted (`0x08XX`) | 3,636 |
| Low halfwords converted (paired) | 8,786 |
| **Total instructions converted to data** | **12,422** |
| ROM SHA1 match after conversion | **Yes** |

### Verification

A comprehensive audit of all 477 assembly files verified correctness and identified remaining work.

#### False positive check (0 found)

All 8,739 `.2byte` directives were verified against ROM bytes using label-anchored address computation. Every `.2byte` value matches the ROM — **zero false positives**. The ROM SHA1 matches after conversion.

50 verification-script "mismatches" were initially flagged, all in `non_word_aligned_thumb_func_start` functions (16 of 17 affected files). These were caused by address computation drift (the label addresses were 2 bytes too high). The `_validate_addresses` fix now corrects this drift by scoring instruction encodings against ROM bytes.

#### Converted ROM pointer halfwords (complete)

- **0 remaining unconverted ROM high halfwords** (`lsrs #0x20` with confirmed ROM pointer pair)
- **0 remaining ROM low halfwords** sandwiched between ROM pointer data
- **281 legitimate `lsrs #0x20` instructions** remain — confirmed as real code by encoding mismatch with ROM bytes

#### IWRAM pointer data-as-code

IWRAM pointers (`0x03XXXXXX`) are handled by **pointer-pair detection** — `_find_high_halfwords` detects `lsls r0, r0, #0x0C` (encoding `0x0300`) as IWRAM high halfwords, and `_find_low_halfwords` pairs them with the preceding low halfword. This covers jump table entries and literal pool entries where both halfwords are adjacent.

Trailing IWRAM literal pool entries that don't match pointer-pair patterns remain as instruction mnemonics. These assemble to identical ROM bytes — the only difference is the ARM ELF `$t`/`$d` mapping symbols, which affects objdiff disassembly display but not the actual binary content.

#### Address computation fix for `non_word_aligned_thumb_func_start`

Functions using `non_word_aligned_thumb_func_start` had label addresses 2 bytes too high — Luvdis included `.align 2, 0` padding bytes that the macro does not emit, or accumulated drift from data-as-code in preceding functions. This caused ROM byte validation to fail for all data conversions in these functions.

**Fix**: `_compute_addresses_anchored` now accepts optional `rom_data` and calls `_validate_addresses`, which scores the current addresses and a -2 offset against known instruction encoding patterns (`push`, `pop`, `bx`, `bl`). If -2 scores strictly better, all addresses are shifted.

#### `DeadCode_0804bb86` fixup

`FUN_0804bb86` (renamed `DeadCode_0804bb86`) is the high halfword (`0x0300`) of FreeGfxBuffer's literal pool `0x030034A0`. Luvdis detected it as a separate function entry point. The `non_word_aligned_thumb_func_start` macro in its `.s` file forced a `$t` (Thumb code) mapping symbol in the middle of FreeGfxBuffer's literal pool data, causing objdiff to disassemble the pool as instructions.

**Fix**: `_apply_fixups` replaces `DeadCode_0804bb86.s` with a conditional stub — empty when FreeGfxBuffer is compiled from C (the compiler emits the full `.4byte`), or `.2byte 0x0300` when FreeGfxBuffer uses `INCLUDE_ASM` (its `.byte` only covers 2 bytes).

#### `ldr` alignment fixup

Functions using `non_word_aligned_thumb_func_start` have literal pool `.4byte` entries at section offsets that may be `2 mod 4` — rejected by the assembler for Thumb `ldr PC-relative`. The `_fix_ldr_alignment()` step (8.5) converts these `ldr` instructions to raw `.inst.n` halfword encodings looked up from the ROM, bypassing the alignment check while producing identical bytes.

#### Summary

| Category | Count | Status |
|---|---|---|
| ROM pointer pairs (0x08XX) | 12,422 | **Complete** (passes 1–3) |
| IWRAM pointer pairs (0x03XX) | ~900 | **Complete** (passes 1–3) |
| Trailing literal pool data | ~355 | **Partial** — pass 4 disabled (see Bug 9); remaining items assemble to identical ROM bytes |
| `ldr` alignment fixup | 1,489 | **Complete** (step 8.5) |
| **ROM SHA1** | | **OK** |

#### Current state

- **ROM matches** (`make compare` passes)
- Pointer-pair detection (passes 1–3) converts jump tables and literal pool entries that form recognizable GBA pointer patterns
- Trailing data after function returns (alignment NOPs, non-pointer literal pool entries) remains as instruction mnemonics — these produce identical ROM bytes but different ARM ELF mapping symbols (`$t` instead of `$d`)
- `ldr` instructions in non-word-aligned functions are emitted as `.inst.n` raw encodings to avoid assembler alignment rejection
- 75 matched functions (including decompiled FreeGfxBuffer and VBlankHandlerMinimal)

### Functions with the most conversions

| Function | `.2byte` directives | Type |
|---|---|---|
| `code_1/EntityBehaviorMasterUpdate.s` | 3,111 | Entity behavior jump tables |
| `code_0/SetupOAMSprite.s` | 1,986 | OAM sprite type dispatch tables |
| `code_0/VBlankCallback_Credits.s` | 669 | Credits rendering dispatch |
| `code_1/EntityUpdateDispatch.s` | 475 | Entity update jump tables |
| `code_1/PlayerMainUpdate.s` | 452 | Player state machine dispatch |
| `code_0/RenderCharacterTiles.s` | 410 | Character tile rendering |
| `code_0/UpdateUIState.s` | 287 | UI state machine dispatch |
| `code_3/UpdatePlayerNormal.s` | 256 | Player normal state dispatch |
| `code_0/HandlePauseMenuInput.s` | 255 | Pause menu dispatch |
| `code_1/EntityBossPhaseC.s` | 237 | Boss phase C dispatch |

## Impact on Mizuchi embeddings and similarity search

### How `index-codebase` processes assembly

Mizuchi's `index-codebase` command scans all assembly functions, preprocesses their code via `preprocessForEmbedding()`, and generates vector embeddings using `jina-embeddings-v2-base-code`. The preprocessing pipeline (`src/shared/indexer/embedder.ts`):

1. **Strip comments** — removes `@` and `/* */` annotations
2. **Extract function body** — removes `thumb_func_start`, labels, `.align`
3. **Normalize instructions** — `adds` → `add`, `ldr r0, _08003DB0` → `ldr r0, [pool]`, `.4byte` → `.word`

The embedding model receives this normalized text and encodes it as a 768-dimension vector. `MizuchiDb` then uses cosine similarity over these vectors for "find similar function" queries.

### Data-as-code pollutes the embedding vector

When data-as-code is not converted, the fake instructions pass through preprocessing and become part of the embedding input. The model treats them as real instructions, distorting the function's vector.

**HandlePauseMenuInput** — a function with a 128-entry jump table:

| Metric | Before (data-as-code) | After (`.4byte` conversion) |
|---|---|---|
| Embedding input lines | ~522 | 370 |
| Fake instruction lines | ~306 (58.6%) | 0 (0%) |
| `.word` data directives | 0 | 152 |
| True instruction lines | 216 | 216 |

Before conversion, 58.6% of the embedding input consists of fake instructions decoded from data bytes:

```
@ Before — fake instructions in the embedding input
push {r4, r5, r6, r7, lr}
ldr r0, [pool]
...
mov pc, r0               ← real code ends here
lsl r0, r0, #0x00
.word 0x03002920
.word 0x0800A8F8
tst r4, r0               ← DATA: actually 0xAB0E (low half of 0x0800AB0E)
lsr r4, r0, #0x20        ← DATA: actually 0x0800 (high half)
tst r4, r0               ← DATA: another pointer entry
lsr r4, r0, #0x20        ← DATA: ...repeats 120+ times
```

After conversion, the embedding input correctly reflects the function's structure:

```
@ After — clean data directives
push {r4, r5, r6, r7, lr}
ldr r0, [pool]
...
mov pc, r0
lsl r0, r0, #0x00
.word 0x03002920
.word 0x0800A8F8
.word 0x0800AB0E          ← recognized as jump table entry
.word 0x0800AB0E
.word 0x0800ABCE
...                        ← 128 entries, all .word
```

### How this distorts similarity search

The data-as-code instructions create false semantic signals in the embedding:

- **`tst rN, rM`** (×120+) — the model encodes this as "heavy use of bitwise test operations", which is completely wrong for a switch-dispatch function
- **`lsr rN, rM, #0x20`** (×120+) — suggests extensive 32-bit shift operations
- **`mul rN, rM`**, **`bic rN, rM`**, **`add rN, sp, #imm`** — each fake instruction type adds noise

This means:
1. **Similar functions match poorly** — another `switch` dispatch function with a different-sized jump table would have a different ratio of fake `tst`/`lsr` lines, reducing the cosine similarity score
2. **Unrelated functions match falsely** — a function that genuinely uses `tst` for bitmask checks would appear similar to HandlePauseMenuInput because both have high `tst` frequency
3. **Jump table size dominates the embedding** — a 128-entry table contributes 306 fake instruction lines vs 216 real instruction lines. The embedding reflects the table size more than the function's actual logic

After conversion, the `.word` directives are semantically neutral data — the embedding model correctly focuses on the 216 real instructions that define the function's behavior.

## References

- [ARM Thumb instruction set — Azeria Labs](https://azeria-labs.com/arm-instruction-set-part-3/) — Thumb instructions are 16-bit; encoding formats for data-processing instructions
- [ARM mapping symbols — GNU Assembler docs](https://sourceware.org/binutils/docs/as/ARM-Mapping-Symbols.html) — `$t` (Thumb code), `$d` (data), `$a` (ARM code) symbols distinguish code from inline data
- [ARM ELF ABI specification — section 5.5.5](https://github.com/ARM-software/abi-aa/blob/main/aaelf32/aaelf32.rst) — Formal definition of mapping symbols in ELF object files
- [Ghidra](https://ghidra-sre.org/) — The reverse engineering tool used for data region detection (ARM Constant Reference Analyzer, Decompiler Switch Analysis)
- [Luvdis](https://github.com/aarant/luvdis) — GBA disassembler that produces the initial assembly files
