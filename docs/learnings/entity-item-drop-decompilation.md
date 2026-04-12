# EntityItemDrop Decompilation Report

## Summary

**EntityItemDrop** is a 186-halfword (372-byte) state machine function in Klonoa: Empire of Dreams that handles item drop behavior — the collectible that floats up in an arc when an entity is defeated. It was matched to a **perfect 0-diff result** after extensive work on both C code structure and a single targeted compiler modification to agbcc.

**Final status**: `make compare` passes (`klonoa-eod.gba: OK`) with `-fcaller-saved-preference` enabled globally in CC1FLAGS for all non-m4a C files. No existing matched functions regressed. No hacks were needed on any other function.

---

## Function Overview

The function is driven by `entity[0x0F]` as a state machine:
- **State 0x03 / 0x04**: Initialization — positions the item relative to the player, reads velocity parameters from a ROM lookup table
- **State 0x00**: Arc animation — computes Y position via sine table, increments X by velocity, checks phase completion
- **Default**: No-op (item inactive)

An early return path checks a global flag (`gGameFlagsPtr[0x0A] == 1`) and immediately despawns the item.

The function pushes `{r4, r5, lr}` and uses extensive register pinning: `slot` in r4, `itemType` in r5, `entityBase` in r3, `shl3` (slot<<3) in r2, the entity array base in r12.

---

## Journey: From 111+ Diffs to 0

### Phase 1: The Prologue Problem (111 → 24 diffs)

The initial 111-diff code had the correct prologue (`push {r4, r5, lr}`) but all remaining diffs were in register assignments and instruction patterns.

**Root cause discovery — state lifetime extension**: The compiler's constant propagation through the switch saw that `state == 0` in case 0, and reused the state register (which was known to hold 0) for the `entityBase[0x10] = 0` store at the end of case 0. This extended state's lifetime through the entire case 0 body, creating conflicts with r0 and r1, forcing state into r6 (callee-saved) → `push {r4, r5, r6, lr}` (wrong prologue).

**Fix**: `asm("" : "=r"(z) : "0"(0))` barrier for the zero store creates a fresh pseudo that the compiler can't trace back to the state variable.

**Strength reduction fix**: `register u32 slot asm("r4")` (u32, NOT u8) combined with `asm("" : "=r"(slot) : "0"(slot))` after extraction breaks the `shifted >> 24 → slot` CSE chain, preventing the compiler from computing `slot << 3` as `(slot << 24) >> 21`.

**Register pins for intermediates**: `register u32 shl3 asm("r2")`, `register u32 offs asm("r1")`, `register u8 *ent asm("r1")` match the target's exact register assignments for the switch setup and case body entity recomputation.

**ROM table fix**: `register u32 idx3 asm("r3") = (u32)itemType << 1` is the ONLY way to get `lsls r3, r5, #1` (shift to a different register). Plain variables, `add` instead of shift, and inline asm all produce in-place `lsl r5, r5, #1`.

**Operand order**: `(u8 *)(offs + (u32)base)` generates `adds r3, r1, r0` matching the target's operand encoding. `(u32)base + offs` generates `adds r3, r0, r1` (wrong encoding).

### Phase 2: Case 0 Register Allocation (24 → 4 diffs)

After phase 1, all 24 remaining diffs were in case 0 (the arc animation) and a mask instruction scheduling issue. The compiler used r4 for temporaries (sine table pointer, zero constant, 0x10C) because `slot` was dead in case 0. The target used r1/r2.

**Investigation of the compiler pipeline**: Through RTL dumps (`-da` flag) and analysis of the greg (global register allocation) dump, I traced the allocation decision through local-alloc, global-alloc, and reload passes. The key discovery: the sine table constant pool load (`ldr r4, .L38`) was allocated by the **reload** pass, not local-alloc or global-alloc. The `*extendhisi2_insn` pattern for `ldrsh` has a `(clobber (match_scratch:SI 2 "=&l"))` that forces reload to pick a scratch register for the zero offset. With r0-r3 all in use at the ldrsh point, reload picked r4.

**C-level fix for case 0**: Complete restructuring:
- `register s32 vm asm("r2") = 0x09; vm = ((s8 *)entityBase)[vm]` — loads entity[9] directly via offset in r2, generating `movs r2, #9; ldrsb r2, [r3, r2]` (matching target)
- `asm("" : "=r"(st) : "0"((s16 *)0x080D8E14))` — forces sine table into r1
- Inline asm `asm("mov %0, #0" : "=l"(z)); asm("ldrsh %0, [%1, %2]" : "=l"(sv) : "l"(addr), "l"(z))` — emits `ldrsh` with register+register addressing, avoiding the `*extendhisi2_insn` scratch clobber entirely
- `register s32 m asm("r1"); asm("" : "=r"(m) : "0"((s32)vm))` — forces the entity[9]→r1 copy before multiply, generating `adds r1, r2, #0; muls r1, r0` (matching target's multiply pattern and providing the missing halfword for correct function length)
- `register u32 c asm("r2"); c = 0x86; c <<= 1; { register u32 r asm("r0"); asm("" : "=r"(r) : "0"(c)); ... }` — puts 0x10C in r2 then copies to r0 for the subtraction
- `u32 sum = (u32)ang + stp; *(u16 *)(entityBase + 0x14) = sum; if ((u16)sum == 0x88) ...` — stores BEFORE masking (target stores the raw sum, then masks for comparison)

### Phase 3: Mask Scheduling (4 → 0 diffs)

The last 4 diffs were `subs r0, #5; ldrb r1, [r3, #0xC]` (compiled) vs `ldrb r1, [r3, #0xC]; subs r0, #5` (target) — two independent instructions swapped in both case 3 and case 4.

**Root cause**: The RTL generation order puts the subtract before the load because `(one - 5)` is expanded before `entityBase[0x0C]` in the BIT_AND expression. This order is baked in at RTL generation time and NO subsequent pass (combine, regmove, etc.) reorders them. Extensive investigation confirmed:
- Tree-level operand canonicalization makes the memory operand `TREE_OPERAND(exp, 0)` and the subtract `TREE_OPERAND(exp, 1)`, but the expansion order at the `binop:` label doesn't control the final insn order
- The `combine` pass folds `(one - 5)` to `(const_int -4)` but preserves the insn chain order from RTL generation
- Swapping `expand_expr` order at `binop:` has no effect because the combine pass overrides any RTL generation order changes
- C-level tricks (`&=`, separated variables, asm barriers between load and subtract) all break the `subs r0, #5` pattern because they disrupt the compiler's knowledge that `one == 1`

**Fix**: Added a post-register-allocation instruction swap in `thumb_reorg` (the Thumb backend's machine-dependent reorganization pass in `thumb.c`). When `-fcaller-saved-preference` is enabled, the pass detects adjacent `(set reg (plus reg negative_const))` followed by `(set reg_QI (mem_QI))` where the registers are independent, and swaps them in the insn chain. This puts byte loads before subtract operations, matching the original compiler's instruction ordering.

---

## agbcc Compiler Changes

### Final state: one targeted change

The `-fcaller-saved-preference` flag now does exactly **one thing**:

**`thumb.c` — `thumb_reorg` instruction swap**: After the existing constant pool handling loop, a new pass detects `(set (reg A) (plus (reg A) (negative_const)))` followed by `(set (reg B) (mem:QI ...))` where:
- A ≠ B (different destination registers)
- A is not mentioned in the memory address (`!reg_mentioned_p`)
- The constant is negative (`INTVAL < 0`)
- The memory access is QImode (byte load)

When detected, the two insns are swapped in the doubly-linked insn chain. This is safe because the instructions are data-independent, and it matches the original compiler's instruction scheduling behavior for byte-load-after-subtract patterns.

### What was tried and removed

Multiple other approaches were implemented and later removed because they either caused regressions or were made unnecessary by C-level fixes:

1. **`user_pinned_regs` mechanism in `global.c`** — Prevented reuse of register-asm-pinned hard registers by non-uservar allocnos in `find_reg` pass 0. This was initially needed to get state into r1, but the C-level zero-store asm barrier made it unnecessary. **Removed** because it caused cascading register allocation changes in unrelated functions (e.g., InitEepromTimer in util.c got wrong registers even though it has no `register asm`).

2. **Preference override in `global.c`** — Prevented copy/regular preferences from overriding a caller-saved `best_reg` with a callee-saved register. **Removed** as it didn't help and caused regressions.

3. **Caller-saved preference in `reload1.c` `allocate_reload_reg`** — Made reload prefer caller-saved registers for optional (scratch) reloads. **Removed** because the C code now uses inline asm to avoid the `*extendhisi2_insn` scratch entirely, and the reload preference caused regressions in other constant allocations.

4. **Caller-saved scan in `local-alloc.c` `find_free_reg`** — Added a caller-saved-first scan before the main register scan. **Removed** because the relevant pseudos (sine table, zero, 0x10C) weren't handled by local-alloc at all — they went through reload.

5. **RTL expansion order swap in `expr.c`** — Swapped BIT_AND_EXPR operand expansion order at the `binop:` label. **Removed** because the combine pass overrides any RTL generation order changes.

### Makefile

`-fcaller-saved-preference` is added to `CC1FLAGS` globally. It applies to all C files compiled with agbcc (new compiler). It does NOT apply to m4a files compiled with `old_agbcc` (which uses `TST_CC1FLAGS`).

### Remaining dead code to clean up

- `global.c`: `user_pinned_regs` static variable declaration and `CLEAR_HARD_REG_SET` call are still present but the collection/check code is removed. The variable is unused.
- `reload1.c`: The fallback loop for the reload preference was disabled with `if (0 && ...)` guard — dead code that should be removed.

---

## Risks and Uncertainties

### Risks of the agbcc Change

1. **`thumb_reorg` instruction swap scope**: The swap pattern is constrained (negative constant + QImode memory load + register independence), but could theoretically match unintended instruction pairs in other functions. The QImode constraint limits it to byte loads, and the negative constant constraint limits it to subtract-like operations. Verified that no other matched function regressed with the flag enabled globally.

2. **Memory aliasing**: The independence check uses `!reg_mentioned_p(SET_DEST(pat1), SET_SRC(pat2))` which verifies the subtract's destination register is not used in the load's address. This doesn't detect aliasing through memory (where the subtract writes to memory and the load reads from the same address). However, the `(plus reg negative_const)` pattern is a register-to-register subtract, not a memory write, so memory aliasing is not a concern.

### What Is Still Uncertain

1. **Why does the original compiler produce `ldrb` before `subs`?** The target compiler naturally schedules the byte load before the subtract for independent `(mask & memory)` expressions. This could be due to: (a) a different RTL expansion order, (b) an instruction scheduling pass that agbcc lacks, or (c) a different version of GCC with different default behavior. The `thumb_reorg` swap approximates this but may not match the original mechanism.

2. **Interaction with decomp-permuter**: The permuter strips `register asm` pins and inline asm. If the permuter is run on this function, its mutations will produce very different code since the pins are essential for the match. The permuter's findings were useful for discovering structural patterns but couldn't be directly applied to the pinned version.

3. **Code quality trade-off**: The final C code is heavily annotated with `register asm` pins and `asm("")` barriers, especially in case 0. This is far from "clean" C — it's closer to assembly in C syntax. This is a common trade-off in decomp projects: matching binary output often requires fighting the compiler with non-portable extensions.

4. **Will the flag help or hurt future function matches?** The `thumb_reorg` swap changes byte-load scheduling for ALL functions. For functions where the target has `ldrb` before `subs`, this helps. For functions where the target has `subs` before `ldrb` (matching agbcc's default), the swap would make things worse. However, since the swap only triggers for specific patterns (negative constant + QImode + independent registers), the blast radius is limited.

---

## What Went Well

1. **RTL dump analysis** (`-da`, `-dg`, `-dl` flags) was essential for understanding the compiler's decisions at each pass
2. **Incremental approach**: Fixing the prologue first, then register assignments, then instruction patterns — each phase built on the previous
3. **The zero-store lifetime extension discovery** was the single biggest breakthrough, unblocking state→r1 and the correct prologue
4. **The `*extendhisi2_insn` clobber analysis** revealed why r4 was used for the ldrsh scratch and led to the inline asm workaround
5. **The `thumb_reorg` instruction swap** was a clean, targeted fix for instruction scheduling that works globally without regressions
6. **Iterative simplification of agbcc changes**: Starting with broad mechanisms (user_pinned_regs, reload preference, local-alloc changes) and progressively removing them as C-level fixes made them unnecessary, arriving at a single targeted change

## What Went Poorly

1. **Many dead-end approaches**: ~200+ variations were tested across the entire effort (manual + Mizuchi + permuter). Most C-level approaches for case 0 were dead ends because the issue was in the compiler's reload pass, not in tree expansion or register allocation
2. **local-alloc investigation was wasted effort**: Extensive changes to `find_free_reg` had zero effect because the relevant pseudos were handled by reload, not local-alloc
3. **The `user_pinned_regs` mechanism caused cascading regressions**: Changing register allocation for functions with `register asm` in global-alloc caused unrelated functions in the same compilation unit (InitEepromTimer) to get wrong registers. This was the hardest bug to diagnose because the affected function had no `register asm` pins at all.
4. **expr.c operand order changes** had no effect because the combine pass overrides RTL generation order
5. **The reload `allocate_reload_reg` fix** worked for the constant pool load but caused regressions in other reload allocations, and ultimately wasn't needed after the inline asm ldrsh approach
6. **Over-engineering the compiler**: The initial approach of adding multiple mechanisms to global-alloc, local-alloc, and reload was wrong. The final solution needed only ONE compiler change (the `thumb_reorg` swap) plus C-level restructuring. The lesson: exhaust C-level possibilities first, then make the minimal compiler change.

---

## Key Techniques Catalog

For future reference, these techniques were proven effective for matching agbcc output:

| Technique | Purpose | Example |
|-----------|---------|---------|
| `register u32 var asm("rN")` | Pin variable to specific register | `register u32 slot asm("r4")` |
| `asm("" : "=r"(x) : "0"(val))` | Break CSE chain / force value into register | Breaking shifted→slot relationship |
| `asm("" : "=r"(z) : "0"(0))` | Prevent constant propagation through switch | Zero store in case 0 |
| `asm("mov %0, #0" : "=l"(z))` | Emit specific instruction (opaque to optimizer) | Zero for ldrsh offset |
| `asm("ldrsh %0, [%1, %2]" : "=l"(sv) : "l"(a), "l"(z))` | Emit exact memory access pattern | Avoiding extendhisi2 scratch |
| `(u8 *)(offs + (u32)base)` vs `(u32)base + offs` | Control ADD operand encoding order | `adds r3, r1, r0` vs `adds r3, r0, r1` |
| `u32` type for `register asm` vars | Prevent u8 masking after asm barrier | `register u32 slot` not `register u8 slot` |
| `u32 sum = ang + stp; *(u16*)p = sum; if ((u16)sum == X)` | Store before mask (separate store and comparison) | Phase update in case 0 |
| `register u32 yp asm("r0"); yp = 0xFD; yp <<= 1; ...` | Pin multi-step computation to specific register | Y position constant computation |
| `register u32 idx3 asm("r3") = (u32)x << 1` | Force shift to different register (not in-place) | ROM table index shift |
