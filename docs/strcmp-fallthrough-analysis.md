# StrCmp Fall-Through Analysis: Missing `return` Insn Pattern in Thumb Backend

## The Pattern

In the Klonoa ROM, `StrCmp` (0x08000448) has separate `bx lr` at each return point:

```asm
StrCmp:                         @ 0x08000448
    ldrb r2, [r0, #0x00]       @ load *str1
    ldrb r3, [r1, #0x00]       @ load *str2
    cmp r2, r3
    bne _0800045C               @ mismatch → jump to return 1
    adds r0, #0x01
    adds r1, #0x01
    cmp r3, #0x00
    bne StrCmp                  @ loop if not null
    movs r0, #0x00              @ match: return 0
    bx lr                       @ ← inline return
_0800045C:
    movs r0, #0x01              @ mismatch: return 1
    bx lr                       @ ← inline return
```

agbcc compiles the equivalent C to a **shared epilogue**:

```asm
    mov r0, #0x0
    b .L9                       @ ← branch to shared return
    mov r0, #0x1
.L9:
    bx lr                       @ ← shared return (single copy)
```

The `b .L9` (0xE000) differs from the original `bx lr` (0x4770) — a 2-byte encoding mismatch.

## Root Cause: Missing `define_insn "return"` in `thumb.md`

### The optimization

GCC has a **jump-to-return redirect** optimization in `jump.c` (lines 371–377):

```c
/* If a jump references the end of the function, try to turn
   it into a RETURN insn, possibly a conditional one.  */
if (JUMP_LABEL (insn)
    && (next_active_insn (JUMP_LABEL (insn)) == 0
        || GET_CODE (PATTERN (next_active_insn (JUMP_LABEL (insn))))
            == RETURN))
    changed |= redirect_jump (insn, NULL_RTX);
```

When `redirect_jump` is called with `NULL_RTX`, it converts the jump into a `(return)` RTL instruction via `redirect_exp` → `gen_rtx_RETURN(VOIDmode)`. This effectively **inlines the return** at each exit point instead of branching to a shared epilogue.

This optimization requires the backend to define a `(define_insn "return" ...)` pattern in its machine description file. When present, GCC's build system auto-generates `HAVE_return` and `gen_return()`, enabling the redirect.

### ARM mode has it; Thumb mode does not

In agbcc (GCC 2.95 fork):

- **ARM mode** (`gcc_arm/config/arm/arm.md`, line 4340): **HAS** the pattern:
  ```
  (define_insn "return"
    [(return)]
    "USE_RETURN_INSN (FALSE)"
    ...
    return output_return_instruction (NULL, TRUE, FALSE);
  ```

- **Thumb mode** (`gcc/thumb.md`): **DOES NOT** have a `define_insn "return"` pattern. Only has `define_expand "epilogue"` and `define_insn "*epilogue_insns"` using `unspec_volatile`.

Without `HAVE_return`, the jump optimizer's redirect logic cannot produce valid `(return)` insns, and all return paths must branch to the shared epilogue label. This is why agbcc always emits `b .LN` instead of `bx lr` when there are multiple return paths.

### Modern GCC also lacks it for Thumb-1

Even in modern GCC (4.x+), the ARM backend's `USE_RETURN_INSN` macro explicitly returns 0 for Thumb-1 mode:

```c
#define USE_RETURN_INSN(ISCOND) (TARGET_32BIT ? use_return_insn (ISCOND, NULL) : 0)
```

This means **no version of GCC has ever supported inline return instructions for Thumb-1 mode**. The optimization only exists for ARM 32-bit mode and Thumb-2 (ARMv6T2+).

### What the original Klonoa compiler likely did

The Klonoa ROM was compiled with a toolchain that produced inline `bx lr` at each return point for leaf functions. Given that no standard GCC version supports this for Thumb-1, the likely explanation is one of:

1. **A custom post-processing pass** similar to our `fix_leaf_returns.py` — replacing `b .LN` with `bx lr` when `.LN` is a leaf return label
2. **A patched GCC** with a `define_insn "return"` pattern added to `thumb.md` (a straightforward ~5-line patch, though making it work correctly requires handling the `unspec_volatile` epilogue format)
3. **A different compiler** (e.g., ARM's own SDT compiler) that has this optimization built in

## Official terminology

The GCC documentation refers to this as the **"`return` instruction pattern"** ([GCC Internals: Jump Patterns](https://gcc.gnu.org/onlinedocs/gccint/Jump-Patterns.html)). The optimization that uses it is variously called:

- **Jump-to-return redirect** (in `jump.c`)
- **Epilogue duplication** / **return inlining** (informal)
- **Shrink-wrapping** in modern GCC (which includes `convert_jumps_to_returns()` in `shrink-wrap.cc`, enabled by `-fshrink-wrap` at `-O1`)

See also:
- [GCC Internals: Function Entry](https://gcc.gnu.org/onlinedocs/gccint/Function-Entry.html) — documents `define_insn "return"` pattern
- [Richard Henderson's original "direct return" patch (Feb 2000)](https://gcc.gnu.org/pipermail/gcc-patches/2000-February/027217.html)
- [agbcc `thumb.md`](https://github.com/pret/agbcc/blob/master/gcc/thumb.md) — no `"return"` pattern
- [agbcc ARM `arm.md`](https://github.com/pret/agbcc/blob/master/gcc_arm/config/arm/arm.md) — has `"return"` pattern at line 4340
- [Modern GCC ARM `arm.h`](https://github.com/gcc-mirror/gcc/blob/master/gcc/config/arm/arm.h) — `USE_RETURN_INSN` returns 0 for Thumb-1

## Cross-Project Survey

### Klonoa: Empire of Dreams (this project)

**1 function affected: StrCmp only.**

StrCmp is the only function in the entire ROM where a leaf function has separate `bx lr` at each return point. 4 other leaf functions with multiple return values (`IsSelectButtonPressed`, `CalcBGScrollMapSize`, `SoundInfoInit`, `FUN_080514d4`) have shared epilogues in the ROM — matching agbcc's natural output.

"ReturnOne" (`FUN_0800045e`) is NOT a real function — it's StrCmp's `return 1` epilogue that Luvdis detected as a separate function entry point. It's never called from anywhere.

### FireEmblem7J (`/Users/macabeus/ApenasMeu/decompiler/FireEmblem7J`)

**0 functions affected.** Uses `old_agbcc` (GCC 2.9). 148 leaf functions in `asm/` have the shared `b _ADDR` → `bx lr` epilogue pattern — exactly matching what `old_agbcc -O2` produces. No leaf functions have separate `bx lr` at each return point.

### Sonic Advance 3 (`/Users/macabeus/ApenasMeu/decompiler/sa3`)

**0 functions affected.** Uses `agbcc`. 232 leaf functions, all with a single `bx lr` at the end. No shared epilogue pattern at all — all leaf functions have simple linear flow.

### Pokémon Emerald (`/Users/macabeus/ApenasMeu/decompiler/pokeemerald`)

**0 functions affected.** Fully decompiled (no remaining assembly). Uses both `agbcc` and `old_agbcc`. 6 leaf functions compiled by `old_agbcc` (all in `src/m4a.c`) have the shared `b .LN → bx lr` epilogue pattern — matching `old_agbcc` output exactly. No functions compiled by regular `agbcc` exhibit this pattern.

### Summary

| Project | Compiler | Leaf functions with shared epilogue | Functions needing inline return fix |
|---|---|---|---|
| Klonoa | agbcc | 4 | **1** (StrCmp) |
| FireEmblem7J | old_agbcc | 148 | 0 |
| Sonic Advance 3 | agbcc | 0 | 0 |
| Pokémon Emerald | agbcc + old_agbcc | 6 (old_agbcc only) | 0 |

**Only Klonoa's StrCmp has separate `bx lr` at each return point.** All other GBA decomp projects have leaf functions that match agbcc/old_agbcc's shared-epilogue output exactly.

## Solution

A post-processing script (`scripts/fix_leaf_returns.py`) replaces `b .LN` with `bx lr` when:
- The function is in `system.s` (the only affected module)
- The function is a leaf (no push/pop)
- It has two distinct return values (`mov r0, #0` and `mov r0, #1`)
- The `b .LN` target label's only content is `bx lr`
- The `b` is preceded by `mov rN, #IMM` (return value assignment)

This is called from the Makefile between the agbcc compilation and assembly steps, analogous to the existing `sed` post-processing used for the TST compilation unit.

### Why not patch agbcc?

Adding `define_insn "return"` to `thumb.md` doesn't work because the Thumb backend emits the epilogue as `unspec_volatile` (via `thumb_unexpanded_epilogue`), not as a `(return)` RTL pattern. The jump optimizer's redirect logic looks for `RETURN` RTL patterns but finds `unspec_volatile` instead. A proper fix would require restructuring the Thumb backend's epilogue emission — a significant change for a single function.

### Equivalent C

```c
u8 StrCmp(u8 *str1, u8 *str2) {
    register u8 c1 asm("r2");
    register u8 c2 asm("r3");
    do {
        c1 = *str1;
        c2 = *str2;
        if (c1 != c2)
            return 1;
        str1++;
        str2++;
    } while (c2 != 0);
    return 0;
}
```

With the post-processing fix, this produces a byte-exact match. The `register asm("r3")` hint is needed to get `cmp r3, #0` (null check on the second string's byte) instead of `cmp r2, #0`.
