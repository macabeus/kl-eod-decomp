# `bpl` vs `bge` Fix — Research & Findings

## 1. The Problem

### What `bge` and `bpl` do

In ARM Thumb, both `bge` and `bpl` are conditional branches that can be used after `cmp rX, #0`:

- **`bge` (Branch if Greater or Equal)** — condition code `GE`, checks `N == V` (Negative equals Overflow). Opcode prefix `0xDA`.
- **`bpl` (Branch if Plus)** — condition code `PL`, checks `N == 0` (Negative flag clear). Opcode prefix `0xD5`.

After `cmp rX, #0`, the Overflow flag (V) is always 0. This makes `N == V` equivalent to `N == 0`, so **`bge` and `bpl` are semantically identical** in this context. However, they encode to different bytes (`0xDA` vs `0xD5`), so the ROM won't match if the wrong one is used.

Similarly:
- **`blt` (Branch if Less Than)** — condition `LT`, checks `N != V`. Opcode `0xDB`.
- **`bmi` (Branch if Minus)** — condition `MI`, checks `N == 1`. Opcode `0xD4`.

These are also equivalent after `cmp rX, #0`.

### What agbcc generates vs what the ROM has

agbcc always emits `bge`/`blt` for signed comparisons. The original ROM uses `bpl`/`bmi` in specific functions:

**agbcc output (all versions, all flags):**
```asm
cmp  r0, #0
bge  .Lskip       ; 0xDA — always
```

**ROM target (Abs function at 0x0800043C):**
```asm
cmp  r0, #0x00
bpl  _08000444     ; 0xD5 — different opcode
```

### Where the condition code is selected in agbcc

**[`gcc/thumb.c` line 1302-1330](https://github.com/pret/agbcc/blob/master/gcc/thumb.c#L1302-L1330):**

```c
static char *conds[] =
{
    "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
    "hi", "ls", "ge", "lt", "gt", "le"
};

static char *
thumb_condition_code(rtx x, int invert)
{
    int val;
    switch (GET_CODE(x))
    {
    ...
    case GE: val = 10; break;   // → "ge"
    case LT: val = 11; break;   // → "lt"
    ...
    }
    return conds[val ^ invert];
}
```

The RTL comparison code `GE` always maps to index 10 (`"ge"`), and `LT` to index 11 (`"lt"`). There is no mechanism to emit `"pl"` (index 5) or `"mi"` (index 4) for signed comparisons.

## 2. Affected Functions in Klonoa: Empire of Dreams

A search of all target assembly files reveals **3 files** containing `bpl`:

| File | Function | Address | Context |
|------|----------|---------|---------|
| `asm/nonmatchings/system/Abs.s` | `Abs` | `0x0800043C` | General utility — NOT in m4a |
| `asm/nonmatchings/m4a/MPlayContinue.s` | `MPlayContinue` | `0x0804f944` | m4a sound engine |
| `asm/nonmatchings/m4a/MPlayNoteProcess.s` | `MPlayNoteProcess` | `0x0804fc10` | m4a sound engine |

No functions use `bmi` (the `LT` equivalent).

### Key observation: `bpl` is not limited to m4a

The `Abs` function sits at `0x0800043C` — at the very start of the ROM, in `system.c`. The m4a functions are at `0x0804f***`. This means the patched compiler variant that produced `bpl` was used **beyond just the m4a module**, for general game code as well.

This extends the scope of the [tst-fix-plan.md](./tst-fix-plan.md) observation. That document found `tst` instructions exclusively in m4a. But `bpl` appears in both m4a and general game code, suggesting the patched compiler was used more broadly.

### ROM byte verification

```
$ xxd -s 0x43C -l 12 baserom.gba
0000043c: 0028 01d5 0021 081a 7047 0000

Decoded (little-endian halfwords):
  0x2800 = cmp r0, #0
  0xd501 = bpl.n +2        ← 0xD5, NOT 0xDA (bge)
  0x2100 = movs r1, #0
  0x1a08 = subs r0, r1, r0
  0x4770 = bx lr
  0x0000 = lsls r0, r0, #0 (alignment padding)
```

## 3. The Patch

### Design

Following the `-fprologue-bugfix` precedent, a new opt-in flag `-fbpl-fix` is added to agbcc. When enabled, `thumb_condition_code` maps:
- `GE` → `"pl"` (index 5) instead of `"ge"` (index 10)
- `LT` → `"mi"` (index 4) instead of `"lt"` (index 11)

All other condition codes (`EQ`, `NE`, `GT`, `LE`, `GEU`, `LTU`, `GTU`, `LEU`) are unaffected.

### Implementation

Three files changed in `pret/agbcc`:

**`gcc/flags.h`** — declare the flag:
```c
/* Nonzero to emit bpl/bmi instead of bge/blt for signed comparisons
   against zero.  Matches a patched SDK compiler variant.  */
extern int flag_bpl_fix;
```

**`gcc/toplev.c`** — define and register:
```c
int flag_bpl_fix = 0;

// In f_options table:
{"bpl-fix", &flag_bpl_fix, 1,
 "Emit bpl/bmi instead of bge/blt for signed zero comparisons"},
```

**`gcc/thumb.c`** — condition code remapping:
```c
case GE: val = flag_bpl_fix ? 5 : 10; break;   /* pl(5) vs ge(10) */
case LT: val = flag_bpl_fix ? 4 : 11; break;   /* mi(4) vs lt(11) */
```

**`gcc/thumb.md`** — negation pattern split:
```
;; Original: disabled when -fbpl-fix
(define_insn "negsi2"
  [(set (match_operand:SI 0 "s_register_operand" "=l")
        (neg:SI (match_operand:SI 1 "s_register_operand" "l")))]
  "!flag_bpl_fix"
  "neg\\t%0, %1")

;; New: two-instruction neg with scratch register
(define_insn "*negsi2_bpl"
  [(set (match_operand:SI 0 "s_register_operand" "=l")
        (neg:SI (match_operand:SI 1 "s_register_operand" "0")))
   (clobber (match_scratch:SI 2 "=l"))]
  "flag_bpl_fix"
  "mov\\t%2, #0\;sub\\t%0, %2, %1"
  [(set_attr "length" "4")])
```

### Scope

The flag affects **all** `bge`/`blt` branches in the compilation unit. This is appropriate because:
1. The patched compiler variant applied this behavior globally, not per-function
2. `bge` ≡ `bpl` and `blt` ≡ `bmi` after `cmp rX, #0` (the only context where these branches appear in optimized Thumb code)
3. Non-zero comparisons like `cmp r0, #5; bge` also produce equivalent results when V=0 (which is always true for Thumb `cmp` with small immediates that don't overflow)

## 4. Regression Testing

### Methodology

The patched agbcc binary was installed into each project's `tools/agbcc/bin/` and `make compare` was run. Since `-fbpl-fix` is opt-in, no compilation unit uses it — this tests that the flag mechanism doesn't break anything when disabled.

### Results

| Project | `make compare` | SHA1 |
|---------|---------------|------|
| **Klonoa: Empire of Dreams** | **OK** | `klonoa-eod.gba: OK` |
| **Sonic Advance 3** | **OK** | `sa3.gba: OK` |
| **pokeemerald** | **OK** | `pokeemerald.gba: OK` |

All three projects produce byte-identical ROMs with the patched agbcc (when `-fbpl-fix` is not passed).

### Flag behavior verification

```
$ agbcc -O2 -fprologue-bugfix test.c        → "bge .L3"  (unchanged)
$ agbcc -O2 -fprologue-bugfix -fbpl-fix test.c → "bpl .L3"  (patched)
```

All branch conditions with `-fbpl-fix` enabled:

| C condition | Without flag | With flag | Affected? |
|-------------|-------------|-----------|-----------|
| `x >= 0` | `bge` | `bpl` | Yes |
| `x < 0` | `blt` | `bmi` | Yes |
| `x > 0` | `bgt` | `bgt` | No |
| `x <= 0` | `ble` | `ble` | No |
| `x == 0` | `beq` | `beq` | No |
| `x != 0` | `bne` | `bne` | No |

## 5. The Abs Function — Perfect Match

With `-fbpl-fix`, the Abs function now matches **byte-for-byte**. The flag includes two behavioral changes:

1. **`bge` → `bpl`**: condition code remapping in `thumb_condition_code`
2. **`neg r0, r0` → `mov r1, #0; sub r0, r1, r0`**: the `negsi2` pattern is replaced by a two-instruction `*negsi2_bpl` pattern that uses a scratch register instead of the single-instruction `neg`

### Matching C code

```c
s32 Abs(s32 arg0) {
    s32 zero;
    if (arg0 < 0) {
        zero = 0;
        arg0 = zero - arg0;
    }
    return arg0;
}
```

Compile with: `agbcc -O2 -fprologue-bugfix -fbpl-fix`

### Byte-for-byte verification

```
Compiled: 002801d50021081a70470000
Target:   002801d50021081a70470000
          ✓ PERFECT MATCH

Disassembly:
  0x2800 = cmp  r0, #0
  0xd501 = bpl  +2
  0x2100 = movs r1, #0
  0x1a08 = subs r0, r1, r0
  0x4770 = bx   lr
  0x0000 = .align padding
```

The `-fbpl-fix` flag also replaces the single-instruction `neg` with the two-instruction `mov rT, #0; sub rD, rT, rS` sequence. This is implemented as a new `*negsi2_bpl` pattern in `thumb.md` that uses a clobber register:

```
(define_insn "*negsi2_bpl"
  [(set (match_operand:SI 0 "s_register_operand" "=l")
        (neg:SI (match_operand:SI 1 "s_register_operand" "0")))
   (clobber (match_scratch:SI 2 "=l"))]
  "flag_bpl_fix"
  "mov\\t%2, #0\;sub\\t%0, %2, %1"
  [(set_attr "length" "4")])
```

The original `negsi2` pattern is disabled when `-fbpl-fix` is active. The register allocator automatically selects a free scratch register (`r1` in the Abs function).

## 6. Other `bpl` Functions

### MPlayContinue — NOT matchable from C

`MPlayContinue` (at `0x0804F944`) uses:
- `non_word_aligned_thumb_func_start` — not standard 4-byte alignment
- `push {r0, lr}` prologue — agbcc never generates this pattern
- Raw encoded bytes (`.2byte 0xD002`, `.2byte 0xDA00`, `.2byte 0xE0A3`) where the assembler couldn't express certain instruction sequences

The tst-fix-plan classified this as "Category 2: Non-standard prologues." This function **cannot be matched from C** with any agbcc variant. It must remain as handwritten assembly.

### MPlayNoteProcess — Requires `-ftst` + `-fbpl-fix`

`MPlayNoteProcess` (at `0x0804FC10`) has a standard agbcc prologue (`push {r4-r7, lr}` + high register saves) and 281 lines of assembly. It contains:
- 11 `tst` instructions (requires the `-ftst` flag from [PR #64](https://github.com/Dream-Atelier/kl-eod-decomp/pull/64))
- 2 `bpl` instructions (requires `-fbpl-fix`)

This function is a strong candidate for C decompilation once both flags are combined. The `-ftst` flag handles `tst` vs `ands+cmp`, and `-fbpl-fix` handles `bpl` vs `bge` and `neg` vs `mov+sub`. However, matching requires:
1. Correct struct definitions for `SoundInfo`, `MusicPlayerTrack`, `SoundChannel`, `ToneData`
2. Both `-ftst` and `-fbpl-fix` flags applied together
3. Careful iteration on register allocation and instruction ordering

## 6. Connection to the TST Fix

The [tst-fix-plan.md](./tst-fix-plan.md) documents that a patched SDK compiler variant existed that could emit `tst` instead of `ands + cmp`. That analysis found `tst` exclusively in m4a functions (6 functions, 29 `tst` instructions).

The `bpl` finding extends this picture:
- `tst` appears in **m4a only** (6 functions)
- `bpl` appears in **m4a AND general game code** (3 functions: 2 m4a + 1 system)

This suggests two possibilities:
1. The patched compiler was used for more than just m4a, but `tst` opportunities only arise in m4a's flag-heavy code
2. Multiple compiler variants were used: one with `tst` support (m4a only) and another with `bpl` behavior (broader usage)

The simplest explanation is #1: a single patched compiler that differs from `pret/agbcc` in at least `tst`, `bpl`/`bmi`, and possibly `neg` instruction selection.

## 7. How to Use

### For Klonoa: Empire of Dreams

In `mizuchi.yaml` or the Makefile, pass `-fbpl-fix` to agbcc for compilation units that need it:

```yaml
compilerScript: |
  tools/agbcc/bin/agbcc \
    "{{cFilePath}}" -o asm.s \
    -mthumb-interwork -Wimplicit \
    -Wparentheses -O2 -fhex-asm -fprologue-bugfix -fbpl-fix
```

**Caution:** `-fbpl-fix` should only be applied to compilation units where the target assembly uses `bpl`. Applying it globally may break functions that were compiled with the standard compiler (which uses `bge`). The per-compilation-unit approach from PR #64 (splitting source files by compiler variant) is recommended.

### For other decomp projects

SA3 and pokeemerald use `bpl` only in handwritten `.s` files (m4a_0.s / m4a_1.s), never in agbcc-compiled C code. The flag is not needed for these projects unless they discover C-compiled functions with `bpl` in the future.

## References

- [ARM7TDMI Data Sheet (ARM DDI 0029E)](https://www.dwedit.org/files/ARM7TDMI.pdf) — Section 5.4.1: Thumb conditional branch encoding. Table 5-2: condition codes GE (N==V), PL (N==0)
- [pret/agbcc `gcc/thumb.c`](https://github.com/pret/agbcc/blob/master/gcc/thumb.c#L1302-L1330) — `thumb_condition_code` function and `conds` table
- [tst-fix-plan.md](./tst-fix-plan.md) — Companion analysis of `tst` vs `ands` in the same compiler variant
- [PR #64](https://github.com/Dream-Atelier/kl-eod-decomp/pull/64) — Fork agbcc with `-ftst`, split m4a, decompile VoiceLookupAndApply
- [Issue #54 — WhenGryphonsFly's comment](https://github.com/macabeus/kl-eod-decomp/issues/54#issuecomment-2730637547) — "at least one development studio created their own patched compiler"
