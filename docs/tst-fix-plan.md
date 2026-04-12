# Phase 1: `tst` vs `ands` Fix — Research & Plan

## 1. Understanding the Bug

### What `tst` and `ands` do

In Thumb, both `tst Rn, Rm` and `ands Rd, Rn, Rm` compute the bitwise AND of two registers and set the CPU flags (N, Z). The difference:

- **`tst Rn, Rm`** — sets flags only, **discards the result**. No destination register is written. ([ARM reference: "TST updates only the APSR. It does not modify the contents of either register."](https://ece353.engr.wisc.edu/arm-assembly/conditional-instructions/)) Raymond Chen describes `tst` as the ["discarding version" of AND](https://devblogs.microsoft.com/oldnewthing/20210608-00/?p=105290): it sets flags for `Rn & op2` without storing the result.
- **`ands Rd, Rn, Rm`** — sets flags AND **writes the result to Rd**. Clobbers a register.

For `if (val & MASK)` where the AND result is never used, `tst` is the correct instruction. `ands` wastes a register on a value that's immediately thrown away.

**Note on Thumb encoding:** In Thumb mode, there is no non-flag-setting `and` — the instruction always sets CPSR condition codes. The [ARM7TDMI Data Sheet (ARM DDI 0029E)](https://www.dwedit.org/files/ARM7TDMI.pdf), Section 5.4.1 (page 5-11) states: *"Note: All instructions in this group set the CPSR condition codes."* Table 5-5 on the same page shows Thumb `AND Rd, Rs` has ARM equivalent `ANDS Rd, Rd, Rs` (with the S suffix), while `TST Rd, Rs` has action *"Set condition codes on Rd AND Rs"* (no destination written). agbcc emits `and r0, r0, r1` in its assembly output (GCC 3-operand syntax), which assembles to opcode `0x4008` (`ands r0, r1` in UAL disassembly). Despite the flags already being set by `ands`, agbcc still emits a redundant `cmp r0, #0` afterwards because the compiler doesn't know that `ands` set the flags it needs. A `tst`-aware compiler would emit just `tst r0, r1` (0x4208) — one instruction instead of two, and without clobbering r0.

### Where the bug lives in agbcc

**[`gcc/thumb.h` line 36-37](https://github.com/pret/agbcc/blob/master/gcc/thumb.h#L36-L37):**
```c
/* ??? There is no pattern for the TST instuction.  Check for other unsupported
   instructions.  */
```

This is the original developer's own TODO comment acknowledging the missing pattern. (Note the typo "instuction" in the original source.)

**[`gcc/thumb.md` lines 818-821](https://github.com/pret/agbcc/blob/master/gcc/thumb.md#L818-L821) — the misnamed `tstsi` pattern:**
```
(define_insn "tstsi"
  [(set (cc0) (match_operand:SI 0 "s_register_operand" "l"))]
  ""
  "cmp\\t%0, #0")
```

Despite being called "tstsi", this pattern emits **`cmp %0, #0`**, not `tst`. It matches RTL that says "set condition codes from a register value" — i.e., test if a register is zero. This is used when the compiler already has a value in a register and needs to compare it to zero. It's NOT the `tst Rn, Rm` instruction.

**What's missing:** There is no pattern matching:
```
(set (cc0) (and:SI (reg) (reg)))
```
This RTL would express "set condition codes from the bitwise AND of two registers, discarding the result". That's exactly what Thumb `tst Rn, Rm` does.

**[`gcc/thumb.md` lines 712-717](https://github.com/pret/agbcc/blob/master/gcc/thumb.md#L712-L717) — the `andsi3` pattern:**
```
(define_insn "*andsi3_insn"
  [(set (match_operand:SI 0 "s_register_operand" "=l")
	(and:SI (match_operand:SI 1 "s_register_operand" "%0")
		(match_operand:SI 2 "s_register_operand" "l")))]
  ""
  "and\\t%0, %0, %2")
```

This only handles the case where the AND result is stored in a register. There's no variant that sets `cc0` without a destination register.

### What agbcc generates vs what the target has

For C code like `if (*flags & 0x80)`:

**agbcc generates:**
```asm
mov  r0, #0x80
and  r0, r0, r1    ; r0 = r1 & 0x80 (clobbers r0)
cmp  r0, #0        ; set flags from r0
beq  .Lskip
```

**Target (original SDK compiler) generates:**
```asm
movs r0, #0x80
tst  r0, r1        ; flags = r0 & r1 (r0 NOT clobbered)
beq  .Lskip
```

The `tst` version is 1 instruction shorter (no `cmp`) and doesn't clobber the mask register.

## 2. Affected Functions

### Real `tst` instructions in the ROM

**29 `tst` instructions across 6 files — ALL in the m4a (sound engine) module.**

All names below are our proposed labels — the original ROM has no symbol names.

| Address | Our name | `tst` | Lines | pokeemerald equivalent (by behavior) |
|---|---|---|---|---|
| `FUN_0804fc10` | `MPlayNoteProcess` | 11 | 281 | `ply_note` (279 lines, note processing) |
| `FUN_0804f944` | `MPlayContinue` | 8 | 327 | **`MPlayMain`** (338 lines, main player tick) |
| `FUN_0804f25e` | `MPlayMain` | 6 | 565 | No equivalent — leaf DMA-clear function |
| `FUN_0804fe10` | `MPlayChannelRelease` | 2 | 36 | `TrackStop` (39 lines) |
| `FUN_0804fb9c` | `SoundContextRef` | 1 | 39 | Possibly `ClearChain` variant |
| `FUN_0804f6f4` | `VoiceLookupAndApply` | 1 | 28 | **No equivalent** — unique to this m4a version |

**Naming issue:** Our `MPlayContinue` is actually the main player tick (≡ pokeemerald's `MPlayMain`). Our `MPlayMain` is a leaf DMA-clear helper, not the main player. These names should be swapped if following pokeemerald's convention.

There is also 1 `tst` in `code_3/RunSceneScript.s` at line 22, but this is **data decoded as instructions** (it appears inside a literal pool region between `.4byte` entries). It's not a real `tst` instruction.

**Zero** non-m4a functions in the game use `tst`. The m4a module is the standard GBA sound engine shared across many games.

### Current `asm()` / `register` workarounds in C sources

These exist but are **NOT caused by tst/ands**:

| File | Count | Purpose |
|---|---|---|
| `src/m4a.c` | 20 `asm()`, 3 `register asm` | Register allocation hacks for m4aSongNumStart etc. |
| `src/engine.c` | 12 `asm()`, 3 `register asm` | DMA loop register pinning |
| `src/code_1.c` | 8 `asm()` | Register ordering for scene state |
| `src/code_3.c` | 4 `asm()` | Register ordering for script dispatch |
| `src/system.c` | 1 `register asm` | Shift register pinning |

None of these workarounds are for `tst` vs `ands`. They're for general register allocation issues where agbcc picks different registers than the original compiler.

### How other decomp projects handle this

#### Sonic Advance 3 (SAT-R/sa3)

The [Sonic Advance 3 decomp](https://github.com/SAT-R/sa3) builds and produces a byte-identical ROM. Its m4a module is split into C and handwritten asm:

- **[`src/lib/m4a/m4a.c`](https://github.com/SAT-R/sa3/blob/main/src/lib/m4a/m4a.c)** — 57 C functions (MPlayContinue, CgbSound, m4aSoundInit, etc.) compiled with `old_agbcc`. These functions use `ands`, not `tst`, **and the ROM matches** — proving the original SDK compiler also produced `ands` for these functions.
- **[`src/lib/m4a/m4a0.s`](https://github.com/SAT-R/sa3/blob/main/src/lib/m4a/m4a0.s)** — 34 handwritten assembly functions (MP2KPlayerMain, SoundMain, TrackStop, etc.) with 36 `tst` instructions across 6 functions.

The [sa3 Makefile](https://github.com/SAT-R/sa3/blob/main/Makefile) (line 358) confirms the split: `$(C_BUILDDIR)/lib/m4a/m4a.o: CC1 := $(CC1_OLD)` — the C functions use `old_agbcc`, while the asm functions are assembled directly.

**Key difference with Klonoa:** In sa3, the simple "unpause" function `MPlayContinue` is C code compiled with `old_agbcc` → produces `ands` → ROM matches. In Klonoa, the function we call `MPlayContinue` (`FUN_0804f944`) is NOT the same function — it's the 327-line main player tick (≡ pokeemerald's `MPlayMain` by behavior). Klonoa uses a **different m4a library version** where functions were reorganized.

#### pokeemerald

pokeemerald uses the same C/asm split ([Makefile line 296](https://github.com/pret/pokeemerald/blob/master/Makefile#L296): `$(C_BUILDDIR)/m4a.o: CC1 := tools/agbcc/bin/old_agbcc$(EXE)`).

**Critical discovery: Klonoa's `FUN_0804f944` (our "MPlayContinue") is behaviorally identical to pokeemerald's `MPlayMain`.** Both have exactly 8 `tst` instructions with identical mask values (0x80, 0xC7, 0x40, 0x0F, 0x03, 0x0C) and the same `push {r0, lr}` prologue. pokeemerald's actual `MPlayContinue` is a completely different function — a 13-instruction "unpause" routine in [`src/m4a.c`](https://github.com/pret/pokeemerald/blob/master/src/m4a.c). We named our function `MPlayContinue` by mistake; it should be `MPlayMain` if following pokeemerald's convention.

pokeemerald's decomp represents `MPlayMain` as assembly in [`src/m4a_1.s`](https://github.com/pret/pokeemerald/blob/master/src/m4a_1.s) (because agbcc can't reproduce it), and that copy also uses `push {r0, lr}`. This means `push {r0, lr}` is simply how this function exists in the SDK library binary — it doesn't tell us whether the **original SDK source** was C or handwritten assembly.

`FUN_0804f6f4` (our "VoiceLookupAndApply") has no equivalent in pokeemerald or sa3. It is unique to this game's m4a library version.

### Evidence for two competing hypotheses

The 6 tst-containing functions in Klonoa fall into two categories:

#### Category 1: Standard compiler prologues (3 functions)

`VoiceLookupAndApply`, `SoundContextRef`, and `MPlayNoteProcess` all have standard agbcc-style prologues (`push {r4, r5, lr}`, `push {r4, r5, r6, lr}`, etc.).

**`FUN_0804f6f4` is the smoking gun.** I manually translated the 28-line assembly into C by reading each instruction and mapping registers to variables, struct offsets to fields (0x00 → `status`, 0x20 → `voices`, 0x34 → `next`), and control flow to `if`/`goto` statements. The resulting C code:

```c
typedef struct SoundChannel {
    u8 status;                   // offset 0x00 (ldrb r1, [r4, #0x00])
    u8 pad[0x1F];
    struct SoundChannel *voices; // offset 0x20 (ldr r4, [r5, #0x20])
    u8 pad2[0x10];
    struct SoundChannel *next;   // offset 0x34 (ldr r4, [r4, #0x34])
} SoundChannel;

void VoiceLookupAndApply(u32 unused, SoundChannel *channel) {
    SoundChannel *voice = channel->voices;
    if (voice == 0) goto end;
loop:
    {
        u8 status = voice->status;
        if (status & 0xC7) {
            voice->status = status | 0x40;
        }
    }
    VoiceGetParams(voice);
    voice = voice->next;
    if (voice != 0) goto loop;
end:
    channel->status = 0;
}
```

When compiled with `old_agbcc -O2`, the output is **byte-for-byte identical** to the ROM target — same registers, same instruction order, same prologue/epilogue — **except** `tst r0, r1` (0x4208) in the target vs `ands r0, r1; cmp r0, #0` (0x4008, 0x2800) in agbcc:

```
Target (ROM):                     agbcc output:
0x00: b530  push {r4,r5,lr}      b530  push {r4,r5,lr}       ✓ identical
0x02: 1c0d  adds r5, r1, #0      1c0d  adds r5, r1, #0       ✓ identical
0x04: 6a2c  ldr r4, [r5, #32]    6a2c  ldr r4, [r5, #32]     ✓ identical
0x06: 2c00  cmp r4, #0           2c00  cmp r4, #0             ✓ identical
0x08: d00c  beq +24              d00d  beq +26                ~ offset shift
0x0a: 7821  ldrb r1, [r4]        7821  ldrb r1, [r4]          ✓ identical
0x0c: 20c7  movs r0, #0xc7       20c7  movs r0, #0xc7         ✓ identical
0x0e: 4208  tst r0, r1           4008  ands r0, r1            ← THE DIFFERENCE
                                  2800  cmp r0, #0             ← extra instruction
0x10: d002  beq +4               d002  beq +4                  ✓ identical
 ...  (remaining 11 instructions are identical)
```

The probability of handwritten assembly matching agbcc register allocation, instruction ordering, and patterns this exactly is effectively zero. **This function was compiled by a compiler variant that could emit `tst`.**

**Validation via Mizuchi run (30 AI attempts + 869,550 permuter iterations):** A full Mizuchi run attempted to decompile this function with both AI and brute-force approaches:

- **30 AI attempts** produced 26 distinct C code variants (different struct layouts, loop styles, cast patterns). All 25 successful objdiff comparisons produced the **exact same result: 21/23 instructions matching (91%), with the only 2 differences being `and r0, r1` → `tst r0, r1` and the extra `cmp r0, #0`**.
- **22 decomp-permuter runs** executed a total of **869,550 iterations** of randomized C code mutations. 19 runs that produced results ALL converged to `bestScore=400` — the score floor corresponding to the 2-instruction `tst`/`ands` difference. Starting from various base scores (400–2300), the permuter consistently optimized down to 400 and could not go lower. No mutation across 869,550 attempts found a way to make agbcc emit `tst`.

All 25 objdiff comparisons that produced results report the **identical 2 differences**:

```
Difference 1 (REPLACEMENT):
- Current: `and r0, r1`
- Target:  `tst r0, r1`

Difference 2 (DELETION):
- Current: `cmp r0, #0x0`
- Target:  `(empty)`
```

This was consistent across diverse C code approaches. A sample of the variants tried:

**Attempt 0** — `0xC7 & flags` (reversed operand order):
```c
if (0xC7 & flags) {
    voice->unk0 = flags | 0x40;
}
```

**Attempt 1** — `status & 0xC7` with typedef struct:
```c
if (status & 0xC7) {
    voice->status = status | 0x40;
}
```

**Attempt 2** — split struct into two types:
```c
struct VoiceNode { u8 unk0; u8 pad1[0x33]; struct VoiceNode *unk34; };
struct VoiceTrack { u8 unk0; u8 pad1[0x1F]; struct VoiceNode *unk20; };
```

**Attempt 9** — explicit `!= 0` comparison with cast:
```c
if ((flags & 0xC7) != 0) {
    node->unk0 = (u8)(flags | 0x40);
}
```

All produce the same 21/23 match. The condition expression (`& 0xC7`, `0xC7 &`, `!= 0`, with/without cast) makes no difference — agbcc always emits `and + cmp #0` where the target has `tst`.

This confirms the `tst` issue is the sole blocker for this function — not register allocation, not struct layout, not control flow. Neither human-guided AI decompilation nor automated brute-force mutation can work around a missing compiler instruction pattern.

The `ands` usage within the same functions (MPlayNoteProcess lines 105, 268; SoundContextRef line 19) is also consistent with a compiler: `ands` is used where the AND result is stored to a register or memory, `tst` where it's discarded. A compiler applies this optimization uniformly.

#### Category 2: Non-standard prologues (3 functions)

`MPlayContinue` (`push {r0, lr}`), `MPlayMain` (`add sp, #-4` with no push), and `MPlayChannelRelease` (`push {r4, r5}` without lr) have prologues that agbcc never generates.

**However, `push {r0, lr}` is NOT evidence of handwritten assembly.** pokeemerald's `MPlayMain` in [`m4a_1.s`](https://github.com/pret/pokeemerald/blob/master/src/m4a_1.s) — which is the **same function** as Klonoa's `MPlayContinue` (8 identical `tst` patterns, same masks) — also uses `push {r0, lr}`. This pattern is part of the original SDK m4a library. Whether it was originally compiler-generated or handwritten by the SDK developers is unknown, but its presence does not help distinguish the two hypotheses.

`MPlayMain` (the leaf DMA function starting with `add sp, #-4`) and `MPlayChannelRelease` (leaf function with `push {r4, r5}` without lr) remain ambiguous.

### Conclusion

1. **`FUN_0804f6f4` was definitely compiled** — byte-identical to agbcc except `tst` → `ands + cmp`. The compiler was a GCC variant with a `tst` instruction pattern. This function has no equivalent in pokeemerald or sa3 — it's unique to Klonoa's m4a version.

2. **`FUN_0804f944` (≡ pokeemerald's `MPlayMain` by behavior)** — same 8 `tst` masks, same `push {r0, lr}` prologue. The pokeemerald decomp represents this function as assembly in [`m4a_1.s`](https://github.com/pret/pokeemerald/blob/master/src/m4a_1.s) because they could not reproduce it from C with agbcc. But we don't know whether the **original SDK binary** was compiled from C (by a tst-capable compiler) or written as assembly by the SDK developers. The `push {r0, lr}` pattern is shared between pokeemerald and Klonoa, but it doesn't help distinguish — it's just how this function exists in the SDK library. **We cannot determine from the binary alone whether the original was compiled or handwritten.**

3. **This is a different m4a library version than sa3/pokeemerald** — `FUN_0804f6f4` is unique to Klonoa, and functions were reorganized across SDK versions. The behavioral equivalences are based on instruction patterns and flag mask values, not names (all Klonoa names are our proposed labels).

4. This is consistent with [WhenGryphonsFly's observation](https://github.com/macabeus/kl-eod-decomp/issues/54#issuecomment-2730637547) that there are "one or two known Nintendo-produced compiler revisions that we don't have" and that "at least one development studio created their own patched compiler."

### Suggested name corrections

Based on behavioral cross-reference with pokeemerald:

| Address | Current (our) name | Suggested name | Reason |
|---|---|---|---|
| `FUN_0804f944` | MPlayContinue | **MPlayMain** | Behaviorally ≡ pokeemerald's `MPlayMain` (main player tick) |
| `FUN_0804f25e` | MPlayMain | **SoundVramClear** | Leaf DMA-clear function, NOT the main player |
| `FUN_0804fc10` | MPlayNoteProcess | **ply_note** | Behaviorally ≡ pokeemerald's `ply_note` (note processing) |
| `FUN_0804fe10` | MPlayChannelRelease | **TrackStop** | Behaviorally ≡ pokeemerald's `TrackStop` (channel release) |

## 3. Patch Design

### What the RTL pattern needs to express

A new pattern that matches:
```
(set (cc0)
     (and:SI (match_operand:SI 0 "s_register_operand" "l")
             (match_operand:SI 1 "s_register_operand" "l")))
```

And emits: `tst\t%0, %1`

### Direct pattern vs combiner

A **direct `define_insn` pattern** is sufficient. No combiner pass is needed. The compiler's RTL already generates the `(set (cc0) (and ...))` form when it can prove the AND result is unused — it just has no matching instruction pattern, so it falls back to `ands` + `cmp`.

However, there's a subtlety: GCC 2.95's Thumb backend may not actually generate this RTL form. The [`cmpsi` expand pattern (lines 784-807)](https://github.com/pret/agbcc/blob/master/gcc/thumb.md#L784-L807) forces comparison operands through `force_reg`, which would allocate a destination register for the AND, preventing the `(set (cc0) (and ...))` form from appearing. A **`define_peephole2`** pattern that combines a preceding `ands` + `cmp #0` into `tst` might be more reliable.

### Proposed approach

**Option A — define_insn (preferred if the RTL form exists):**
```
(define_insn "*tstsi_and"
  [(set (cc0)
        (and:SI (match_operand:SI 0 "s_register_operand" "l")
                (match_operand:SI 1 "s_register_operand" "l")))]
  "TARGET_TST_FIX"
  "tst\\t%0, %1")
```

**Option B — define_peephole2 (more robust):**
```
(define_peephole2
  [(set (match_operand:SI 0 "s_register_operand" "")
        (and:SI (match_dup 0)
                (match_operand:SI 1 "s_register_operand" "")))
   (set (cc0) (compare (match_dup 0) (const_int 0)))]
  "TARGET_TST_FIX && peep2_reg_dead_p (2, operands[0])"
  [(set (cc0) (and:SI (match_dup 0) (match_dup 1)))]
)
```

This peephole matches `ands Rd, Rd, Rm; cmp Rd, #0` where Rd is dead after the sequence, and rewrites it to the pattern from Option A.

**Both options require:**
1. A new flag `TARGET_TST_FIX` (or `-ftst-fix`) gated by a `target_flags` bit
2. The `define_insn` from Option A to emit the actual `tst` instruction

### Risk assessment

[WhenGryphonsFly's comment on issue #54](https://github.com/macabeus/kl-eod-decomp/issues/54#issuecomment-2730637547) is relevant here. The patch is narrow (only affects flag-test patterns) and would not change register allocation. However, as they note:

> "High register allocation (and register allocation in general) is known to be extremely finicky. I would not use it as evidence in support of a compiler patch."

And:

> "The TST behavior *is* indicative of a compiler patch/revision, but without being able to verify that behavior myself [...] I do not know if that is an AI hallucination or not."

The `tst` behavior is NOT a hallucination — the 29 `tst` instructions are directly observable in the disassembled ROM binary (verified with `arm-none-eabi-objdump`). But WhenGryphonsFly's broader point stands: the m4a module is the **only** affected module, and fixing `tst` alone won't unblock decompilation of these functions due to other register allocation differences.

## 4. Test Matrix

### Game functions to validate

All 6 tst-containing functions from m4a:

1. `MPlayMain` — 6 tst, 565 lines (the core music tick)
2. `MPlayContinue` — 8 tst, 327 lines
3. `MPlayNoteProcess` — 11 tst, 281 lines
4. `MPlayChannelRelease` — 2 tst
5. `SoundContextRef` — 1 tst
6. `VoiceLookupAndApply` — 1 tst

For each: compile with `-ftst-fix`, run objdiff, check if `tst` lines now match.

### Regression checks

All currently-matched functions (66 files in `asm/matchings/`) must still match after the change. Run `make compare` with and without `-ftst-fix` to verify no regressions.

### Synthetic C test functions

```c
// 1. Basic flag check — should emit tst
int test_flag(u8 *p) { if (*p & 0x80) return 1; return 0; }

// 2. Multiple flag checks on same register
int test_multi_flags(u8 *p) {
    u8 v = *p;
    if (v & 0x80) return 1;
    if (v & 0x40) return 2;
    if (v & 0xC7) return 3;
    return 0;
}

// 3. Flag check with mask in register (not immediate)
int test_reg_mask(u8 val, u8 mask) { if (val & mask) return 1; return 0; }

// 4. Flag check where AND result IS used (should NOT emit tst)
int test_and_used(u8 val) { u8 masked = val & 0x0F; return masked + 1; }

// 5. Flag check in loop condition
void test_loop_flag(u8 *arr, int n) {
    for (int i = 0; i < n; i++) { if (arr[i] & 0x80) return; }
}

// 6. 32-bit flag check
int test_u32_flag(u32 *p) { if (*p & 0x80000000) return 1; return 0; }

// 7. Combined flag check and use (tst for check, ands for value)
int test_mixed(u8 val) {
    if (!(val & 0xC7)) return 0;
    if (val & 0x40) return 2;
    return 1;
}

// 8. Flag check on struct field
typedef struct { u8 status; u8 type; } Entry;
int test_struct_flag(Entry *e) { if (e->status & 0x83) return 1; return 0; }

// 9. Nested flag checks (common m4a pattern)
int test_nested(u8 flags, u8 type) {
    if (flags & 0xC0) { if (type & 0x10) return 1; }
    return 0;
}

// 10. Negative test — volatile (should still use ands if result stored)
volatile int g_result;
void test_volatile_and(u8 val) { g_result = val & 0x3F; }
```

For each: verify that with `-ftst-fix`:
- Tests 1-3, 5-9 emit `tst` instead of `and` + `cmp`
- Tests 4, 10 still emit `ands` (result is used)

## Honest Assessment

### The evidence is mixed

| Address | Our name | Prologue | `tst` | agbcc match? | Likely source |
|---|---|---|---|---|---|
| `FUN_0804f6f4` | VoiceLookupAndApply | `push {r4, r5, lr}` ✓ | 1 | Byte-identical except tst→ands | **Patched compiler** |
| `FUN_0804fb9c` | SoundContextRef | `push {r4, r5, r6, lr}` ✓ | 1 | Standard prologue, mixed tst/ands | Likely patched compiler |
| `FUN_0804fc10` | MPlayNoteProcess | `push {r4-r7, lr}` + high regs ✓ | 11 | Standard prologue, mixed tst/ands | Likely patched compiler |
| `FUN_0804f944` | MPlayContinue | `push {r0, lr}` | 8 | ≡ pokeemerald `MPlayMain` (same behavior) | **Unknown** — `push {r0, lr}` also in pokeemerald's asm |
| `FUN_0804f25e` | MPlayMain | `add sp, #-4` (no push) | 6 | Leaf DMA function, no standard prologue | Ambiguous |
| `FUN_0804fe10` | MPlayChannelRelease | `push {r4, r5}` (no lr) | 2 | Leaf function, no bl | Ambiguous |

### What the VoiceLookupAndApply comparison proves

The byte-level comparison against agbcc output is conclusive: 21 instructions, all identical registers and scheduling, **the only difference is `tst` (0x4208) vs `ands` (0x4008) + `cmp #0` (0x2800)**. This could not be handwritten assembly that coincidentally matches agbcc's register allocation.

**At least some of the tst-containing functions were compiled by a GCC variant that had a `tst` instruction pattern.** This is consistent with WhenGryphonsFly's note that ["at least one development studio created their own patched compiler"](https://github.com/macabeus/kl-eod-decomp/issues/54#issuecomment-2730637547) and that there are ["one or two known Nintendo-produced compiler revisions that we don't have"](https://github.com/macabeus/kl-eod-decomp/issues/54#issuecomment-2730637547).

### What this means for the patch

**A `-ftst-fix` patch IS justified.** VoiceLookupAndApply proves a tst-capable compiler existed and produced output byte-identical to agbcc. For the 3 functions with standard agbcc prologues, the patch should produce exact matches. For the 3 with non-standard prologues, the patch is necessary but may not be sufficient (the prologue differences would need separate work).

### Revised recommendation

1. **Implement the `-ftst-fix` patch** — VoiceLookupAndApply proves it would produce correct output. Start with this function as the validation case.
2. **Test all 6 tst functions** after the patch. The 3 with standard prologues (VoiceLookupAndApply, SoundContextRef, MPlayNoteProcess) should match from C. The 3 with non-standard prologues (MPlayContinue, MPlayMain, MPlayChannelRelease) may need handwritten assembly or additional compiler work.
3. **Use pokeemerald's m4a decomp as a reference** — `MPlayContinue` in Klonoa is the same function as [`MPlayMain` in pokeemerald](https://github.com/pret/pokeemerald/blob/master/src/m4a_1.s). The pokeemerald community's handwritten asm can be adapted for the functions that don't match from C.
4. **Before upstreaming to pret/agbcc** — present the VoiceLookupAndApply byte comparison as evidence. This is concrete, reproducible proof that a tst-capable GCC variant existed for the GBA SDK.

## References

- [ARM7TDMI Data Sheet (ARM DDI 0029E)](https://www.dwedit.org/files/ARM7TDMI.pdf) — Section 5.4.1, page 5-11: *"All instructions in this group set the CPSR condition codes."* Table 5-5: Thumb `AND Rd, Rs` ≡ ARM `ANDS Rd, Rd, Rs`; Thumb `TST Rd, Rs` action is *"Set condition codes on Rd AND Rs"*
- [ARM conditional instructions — ECE353, University of Wisconsin](https://ece353.engr.wisc.edu/arm-assembly/conditional-instructions/) — TST "updates only the APSR" and "does not modify the contents of either register"
- [The ARM processor (Thumb-2), part 7: Bitwise operations — Raymond Chen, The Old New Thing](https://devblogs.microsoft.com/oldnewthing/20210608-00/?p=105290) — TST described as "discarding version" of AND
- [pret/agbcc `gcc/thumb.h`](https://github.com/pret/agbcc/blob/master/gcc/thumb.h#L36-L37) — "There is no pattern for the TST instuction"
- [pret/agbcc `gcc/thumb.md`](https://github.com/pret/agbcc/blob/master/gcc/thumb.md#L818-L821) — `tstsi` pattern emits `cmp`, not `tst`
- [pret/pokeemerald `src/m4a_1.s`](https://github.com/pret/pokeemerald/blob/master/src/m4a_1.s) — handwritten assembly with `tst` instructions for MPlayMain; MPlayMain uses `push {r0, lr}` prologue (same as Klonoa's MPlayContinue)
- [pret/pokeemerald `src/m4a.c`](https://github.com/pret/pokeemerald/blob/master/src/m4a.c) — MPlayContinue as C: simple 13-instruction "unpause" function (different from Klonoa's MPlayContinue which ≡ pokeemerald's MPlayMain)
- [pret/pokeemerald `Makefile` line 296](https://github.com/pret/pokeemerald/blob/master/Makefile#L296) — `m4a.o` compiled with `old_agbcc`
- [SAT-R/sa3 `src/lib/m4a/m4a0.s`](https://github.com/SAT-R/sa3/blob/main/src/lib/m4a/m4a0.s) — Sonic Advance 3 handwritten m4a assembly with `tst`
- [SAT-R/sa3 `src/lib/m4a/m4a.c`](https://github.com/SAT-R/sa3/blob/main/src/lib/m4a/m4a.c) — Sonic Advance 3 C-compiled m4a functions (no `tst`)
- [SAT-R/sa3 Makefile line 358](https://github.com/SAT-R/sa3/blob/main/Makefile#L358) — `m4a.o` compiled with `old_agbcc`, asm functions assembled separately
- [Issue #54 — WhenGryphonsFly's comment](https://github.com/macabeus/kl-eod-decomp/issues/54#issuecomment-2730637547) — context on agbcc history and caution about compiler patches
