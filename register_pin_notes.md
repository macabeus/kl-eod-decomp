# Register Pin Removal Notes

All 15 register pins across 4 patterns were tested with 20+ C rewrites each (80+ total attempts). None could be removed. agbcc's Thumb register allocator has fixed policies that cannot be influenced by C-level constructs.

## Pattern 1: StrCpy — `register u32 c asm("r2")`

**Function**: `StrCpy` in `src/system.c`
**Target**: `ldrb r2, [r1]; strb r2, [r0]` — temp byte in r2, dst stays in r0

**Root cause**: agbcc copies dst (r0) to r2 and reuses r0 for the `ldrb` result. This is a fixed register allocator policy for byte-copy loops with two pointer parameters.

**All 20 attempts produce `add r2, r0, #0` (copy dst to r2):**

| # | Technique | Result |
|---|-----------|--------|
| 1 | `u32 c` (plain local) | r0 freed, dst→r2 |
| 2 | `register u32 c` (hint only) | same |
| 3 | `s32 c` | same |
| 4 | `u8 c` | same + lsl/lsr byte extension |
| 5 | `*dst++ = *src++` (post-increment) | same |
| 6 | Comma-operator `while` loop | same |
| 7 | `for(;;) { ... if (c==0) break; }` | same + different branch structure |
| 8 | Index-based `src[i]`/`dst[i]` | push {r4,lr} — became non-leaf |
| 9 | `*(vu8 *)src` volatile dereference | same |
| 10 | `*dst = c = *src` store-first | same |
| 11 | `void *` return type | same |
| 12 | `u8 *` return type | same |
| 13 | Separate `u8 *d = dst` pointer | same |
| 14 | `const u8 *src` parameter | same |
| 15 | Both ptrs copied to locals | same |
| 16 | `goto loop` instead of do-while | same |
| 17 | `*dst++ = (c = *src++)` nested assign | same |
| 18 | `int c` type | same |
| 19 | `unsigned c` type | same |
| 20 | `char c` type | same |

---

## Pattern 2: FixedMul4/FixedMul8 — `register s32 shifted asm("r1") = result`

**Functions**: `FixedMul4` in `src/math.c`, `FixedMul8` in `src/system.c`
**Target**: `muls r0, r1; adds r1, r0, #0` — copy multiply result to r1 for conditional rounding

**Root cause**: agbcc sees that `shifted = result` is a simple copy and optimizes it away, working directly on r0. No C construct forces the compiler to materialize a separate copy.

**All 20 attempts produce `mul r0, r0, r1` with no r0→r1 copy:**

| # | Technique | Result |
|---|-----------|--------|
| 1 | `s32 shifted = result` (plain) | copy eliminated |
| 2 | `register s32 shifted = result` | same |
| 3 | `(s32)b * (s32)a` (swap operands) | same |
| 4 | Ternary `(result < 0 ? result + 0xF : result) >> 4` | same |
| 5 | Mask `(r + ((r >> 31) & 0xF)) >> 4` | same |
| 6 | Orphan block `{ s32 shifted = result; ... }` | same |
| 7 | Reuse `b` parameter as shifted | mul r1,r1,r0 — wrong operand order |
| 8 | Two separate vars `cmp_val` + `shifted` | same |
| 9 | `volatile s32 result` | stack spill — push/add sp |
| 10 | `(s32)(u32)result` cast | same |
| 11 | `result + 0` | same |
| 12 | `result ^ 0` | same |
| 13 | `result \| 0` | same |
| 14 | `result & ~0` | same |
| 15 | `(result << 0) >> 0` | same |
| 16 | Declare `shifted` before `result` | same |
| 17 | `(s32)(s16)result` cast | mul r1,r1,r0 — truncates result |
| 18 | `neg = shifted < 0` separate flag | same |
| 19 | `result - 0` | same |
| 20 | `-(-result)` double negate | same |

---

## Pattern 3: m4aSongNumStart/m4aMPlayCommand/m4aSongNumStop — `register ... asm("r2") = gMPlayTable`

**Functions**: All three in `src/m4a.c`
**Target**: `ldr r2, =gMPlayTable; ldr r1, =gSongTable`

**Root cause**: In a non-leaf function with a `u16` parameter (which consumes r0 for lsl/lsr extension), agbcc assigns the first literal pool load to r3, not r2. This is a fixed policy regardless of declaration order, code structure, parameter type, or abstraction level.

**All 20 attempts produce `ldr r3, ...` for gMPlayTable:**

| # | Technique | Result |
|---|-----------|--------|
| 1 | `mplayTable` declared first, no pin | ldr r3 |
| 2 | `songTable` declared first | ldr r3 |
| 3 | pokeemerald-style struct indexing `&mp[song->ms]` | ldr r3 |
| 4 | Struct style, reversed declaration | ldr r3 |
| 5 | Fully inlined `&gSongTable[idx]` / `&gMPlayTable[..]` | ldr r3 |
| 6 | `u32` parameter type (no extension) | ldr r3 |
| 7 | `register` hint (no asm) | ldr r3 |
| 8 | Both `register` hints | ldr r3 |
| 9 | `void *` cast for mplayTable | ldr r3 |
| 10 | `u32` cast for mplayTable | ldr r3 |
| 11 | Orphan block for mplayTable | ldr r3 |
| 12 | Late reference to `gMPlayTable` (no local) | ldr r3 |
| 13 | `mp[pi].info` struct member access | ldr r3 |
| 14 | Precompute `songOff = idx * 8` first | ldr r3 |
| 15 | `u8 *` pointer type | ldr r3 |
| 16 | Extra `u32 i = idx` local | ldr r3 |
| 17 | Separate assign statements (`mp = ...; st = ...;`) | ldr r3 |
| 18 | Three separate assigns + orphan block | ldr r3 |
| 19 | `s16` parameter type | ldr r3 |
| 20 | `s32` parameter type | ldr r3 |

---

## Pattern 4: ClearVideoState/ClearOamBufferExtended/ClearOamEntries6Plus — `asm("r0")`, `asm("r1")`, `asm("r2")`

**Functions**: All three in `src/engine.c`
**Target**: `ldr r0, =buffer; movs r1, #count; movs r2, #0`

**Root cause**: In no-parameter functions, agbcc assigns literal pool loads to r2 (not r0) and uses r0 for the inner loop counter. The zero constant goes to r3 instead of r2. This policy is fixed regardless of declaration order, types, loop structure, or pointer style.

**All 20 attempts assign pointer to r2 instead of r0:**

| # | Technique | First 3 instructions |
|---|-----------|---------------------|
| 1 | Plain locals | ldr r2; mov r1, #0x62; mov r3, #0 |
| 2 | Literal 0 (no zeroFill var) | ldr r2; mov r1, #0x62; mov r3, #0 |
| 3 | Count declared first | mov r2, #0x62; ldr r1; mov r3, #0 |
| 4 | Zero declared first | mov r3, #0; ldr r2; mov r1, #0x62 |
| 5 | Zero first, count second | mov r3, #0; mov r2, #0x62; ldr r1 |
| 6 | Hardcoded address | ldr r2; mov r1; mov r3 |
| 7 | `register` hints (no asm) | ldr r2; mov r1; mov r3 |
| 8 | `u8 *` with `+= 4` stride | ldr r2; mov r1; mov r3 |
| 9 | Hoisted `bytesLeft` to function scope | ldr r2; mov r1; mov r3 |
| 10 | `while` loop instead of `do-while` | ldr r2; mov r1; mov r3 |
| 11 | `for` loop outer | ldr r2; mov r3; mov r1 |
| 12 | `gEntityArray + 7` pointer | ldr r2; mov r1; mov r3 |
| 13 | Pointer declared last | mov r2; mov r3; ldr r1 |
| 14 | `int` types | ldr r2; mov r1; mov r3 |
| 15 | `u32` types | ldr r2; mov r1; mov r3 |
| 16 | `vu32 *` volatile pointer | ldr r2; mov r1; mov r3 |
| 17 | Pointer as `s32` | ldr r2; mov r1; mov r3 |
| 18 | 7x unrolled inner loop | ldr r0; mov r2; mov r1 — different structure |
| 19 | Inner loop as decrement-to-zero | ldr r2; mov r1; mov r3 |
| 20 | Store before decrement in inner loop | ldr r2; mov r1; mov r3 |

**Notable**: Attempt 18 (7x unrolled) produced `ldr r0` for the pointer — but the rest of the function body was completely different (no inner loop), so the overall function doesn't match.

---

## Conclusion

agbcc (GCC 2.95 for Thumb) has hardwired register allocation policies that no C-level construct can override:

1. **Byte-load loops**: r0 is always reused for `ldrb` results, even when a parameter lives there
2. **Copy elimination**: `x = y` is always optimized away when `x` is only used for one conditional modification
3. **Non-leaf functions**: first literal pool load goes to r3, not r2, when r0 is used for parameter extension
4. **No-parameter functions**: literal pool loads go to r2 regardless of declaration order

These are all deterministic: across 80+ attempts with varying types, loop structures, declaration orders, abstraction levels, and volatile qualifiers, the register assignment never changed for any given pattern.
