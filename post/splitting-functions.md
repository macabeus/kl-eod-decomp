# Splitting the ROM: Recovering Function Boundaries from a GBA Disassembly

When you point a disassembler at a GBA ROM, you don't get neat, self-contained functions. You get a soup. This post documents the work of building a pipeline in `generate_asm.py` to turn the raw Luvdis disassembly of *Klonoa: Empire of Dreams* into individually decompilable function files — and the surprising things we learned about how the original compiler laid out code.

## The starting point

Luvdis (a GBA-specialized disassembler) combined with Ghidra's function detection gave us 688 labeled function boundaries. Naively splitting at each boundary produced 688 `.s` files. Sounds great — until you look at them.

The vast majority were **fragments**: functions whose last instruction wasn't a return. They just... fell through to the next function. Of 688 files, only 48 ended with a proper return (`bx`, `pop {pc}`, or an unconditional branch). The other 640 were broken pieces of larger functions.

These fragments are useless for decompilation. You can't write C code for half a function.

## Why does this happen?

GBA games compiled with agbcc (a GCC 2.95 derivative targeting ARMv4T Thumb) produce some patterns that confuse modern disassemblers:

### 1. Shared epilogues (tail-merged returns)

The compiler notices multiple functions end with the same `pop {r1}; bx r1` sequence and emits it once:

```asm
FUN_08000960:           @ a real function
    push {lr}
    ...
    @ falls through to shared epilogue below

FUN_08000972:           @ NOT a real function — compiler artifact
    pop {r1}
    bx r1
```

Ghidra sees `FUN_08000972` as a function (it has a label, other code branches to it). But from the programmer's perspective, this is just the return instruction of `FUN_08000960`. The C code was simply:

```c
s16 FUN_08000960(s16 a, s16 b) {
    return divide(a << 8, b);
}
```

The compiler created the shared epilogue as a size optimization under `-O2`.

### 2. Literal pools decoded as instructions

Thumb `ldr Rd, [PC, #imm]` loads constants from a "literal pool" — a table of 32-bit values placed after the function body. Luvdis sometimes places a function boundary right at a literal pool address, splitting the pool from the function that uses it:

```asm
FUN_0804b464:
    ...
    ldr r1, FUN_0804b4ac    @ loads from address 0x0804B4AC
    ...
    bx r0
_0804B4A4: .4byte 0x08189BCC
_0804B4A8: .4byte 0x08057ACC
                            @ pool entry at 0x0804B4AC is MISSING

FUN_0804b4ac:               @ Luvdis thinks a new function starts here
    ...                     @ but this is actually a .4byte 0x03003430
```

The bytes at `0x0804B4AC` are `0x03003430` — a RAM address that the `ldr` instruction needs. But Luvdis decoded those bytes as the first instruction of a new function.

### 3. Inter-module padding decoded as instructions

Between compilation units, the linker inserts padding bytes (`0x0000`) for alignment. The Thumb encoding of `0x0000` is `lsls r0, r0, #0x00` — a valid (if useless) instruction. Luvdis faithfully decodes these as code, making the last function in each module appear to not return properly.

### 4. Missing function boundaries

The flip side: Luvdis *misses* real function boundaries. A single Luvdis "function" sometimes contains multiple real functions separated by:

```
bx r1                   @ return from function A
lsls r0, r0, #0x00     @ 0x0000 padding
push {r4, r5, r6, lr}  @ start of function B — Luvdis missed this!
```

The `push {... lr}` is the universal Thumb function prologue. The compiler always emits it for any non-leaf function. Luvdis didn't recognize these as function boundaries because they weren't in its function database.

### 5. Hidden leaf functions

Some functions never call other functions (leaf functions). They don't need to save the link register, so they have no `push {lr}` prologue — they just end with `bx lr`:

```asm
FUN_08000978:
    push {lr}
    ...                     @ fixed-point division wrapper
    pop {r1}
    bx r1                   @ ← return from first function

    lsls r0, r0, #0x10     @ ← start of leaf function (no push!)
    ...                     @ fixed-point multiply with rounding
    bx lr                   @ ← return via bx lr (leaf — lr was never clobbered)
```

The second function is invisible to prologue-based detection because it has no `push`. On ARMv4T (GBA), `bx lr` is the standard leaf return — the function never calls `bl`, so the link register still holds the caller's return address.

## The pipeline

The solution required three phases, applied in order:

### Phase 1: Split — detect hidden sub-functions

Before doing anything else, scan each Luvdis function body for two patterns:

**Non-leaf functions** (the common case):
```
terminating instruction (bx / pop {pc} / b)
  → optional padding / data-as-code
  → push {... lr}  ← new function starts here
```

**Leaf functions** (no prologue):
```
bx / pop {pc}              (hard return only — not unconditional b)
  → optional padding / data-as-code
  → short instruction sequence (≤ 20 instructions, no .4byte data)
  → bx lr                  ← confirms this is a real leaf function
```

The leaf detection uses a stricter "hard return" trigger (`bx`/`pop {pc}` only, not `b label`) because unconditional branches within a function are common — a `b` followed by more code is usually just a jump to another part of the same function, not a function boundary.

Each detected sub-function gets a name derived from its ROM address (computed by counting instruction bytes from the parent's known address: 2 bytes per Thumb instruction, 4 for `bl`/`blx`, and the appropriate sizes for data directives).

One critical validation: we reject any split that would leave the first segment with unresolved literal pool references. If a function's `ldr` instruction references a label that would end up in the second segment, the split point is inside a literal pool — not a real function boundary.

We also use `non_word_aligned_thumb_func_start` (without `.align`) for the injected function headers. The regular `thumb_func_start` macro includes `.align 2, 0` which can insert padding bytes, shifting literal pool entries to non-word-aligned addresses and breaking `ldr` offsets.

### Phase 2: Merge — rejoin fragments into complete functions

After splitting, many Luvdis entries are still fragments. We merge consecutive fragments within each module until we reach a function that ends with a return.

Two merge triggers:
1. **Fragment fall-through**: the current function's last real instruction isn't a terminating instruction
2. **Pool overlap**: the current function's literal pool references labels defined in the next function (the literal pool spans the boundary)

Three stop conditions prevent incorrect merging:

1. **Never absorb a function that starts with `push {... lr}`**. That's a real function prologue — even if the previous function appears to be a fragment, the next function is independent. The "fragment" before it is likely a shared epilogue or jump table that the compiler placed between two real functions.

2. **Never absorb a function that is called or branched to from other functions** — unless the two blocks share cross-label references that require co-location. A function targeted by `b`/`bl` from elsewhere is an independently callable entry point. Absorbing it creates a multi-entry-point `.s` file that cannot be decompiled to C. However, if the candidate function has backward branches into the merged block (or vice versa), the assembler needs both in the same file to resolve short Thumb branch offsets, so we allow the merge.

3. **Last-in-module heuristic**: for the last function in a module, if there's a return instruction anywhere in the body (even if followed by padding), the function is complete. The trailing bytes are just inter-module alignment padding.

#### Why the external-reference check matters

Without condition 2, the merge phase would produce monster files like:

```
FUN_0804f758 (MidiDecodeByte):     ← entry point 1
    ldr r2, [r1, #0x40]
FUN_0804f75a:                      ← entry point 2 (called from other code!)
    adds r3, r2, #0x1
    str r3, [r1, #0x40]
    ldrb r3, [r2, #0x00]
    b MidiNoteDispatch
```

`MidiDecodeByte` is a one-instruction thunk that loads a field and falls through into `FUN_0804f75a`. Other code calls `FUN_0804f75a` directly. If merged, you get a single INCLUDE_ASM with two entry points — impossible to express in C. If kept separate, `FUN_0804f75a` becomes independently decompilable, and `MidiDecodeByte` stays as a small assembly stub (fall-through between adjacent INCLUDE_ASMs works naturally since they're in the same compilation unit).

The cross-label exception handles cases like this 13-function chain in code_0:

```
FUN_0800d188 <- [FUN_0800d81a, FUN_0800db18, FUN_0800de24, ...]
```

Here `FUN_0800de24` is called from outside, but its code has `b _0800B788` — a backward branch into `FUN_0800d188`'s body. Separating them would put the branch target in a different inline-asm block, and the GNU assembler can't resolve short Thumb branches (±2KB) across separate `asm()` statements. So the merge is preserved, but the chain is split at `FUN_0800de24`: `FUN_0800d188` absorbs only up to `FUN_0800db18`, and `FUN_0800de24` starts a new chain absorbing the rest.

#### Alignment after un-merging

When a previously-absorbed function becomes standalone, it needs its own `thumb_func_start` header. But `thumb_func_start` includes `.align 2, 0`, which pads to 4-byte boundaries. Functions that sit at 2-byte-aligned (but not 4-byte-aligned) addresses — common after a fall-through from a short thunk — would get unwanted padding. The pipeline detects these and uses `non_word_aligned_thumb_func_start` instead, which omits the `.align` directive.

### Phase 3: Downgrade — hide compiler artifacts

After merging, each `.s` file may contain multiple `thumb_func_start` macros — the first is the real function entry; the rest are absorbed fragments (shared epilogues, jump table entries, etc.).

The `thumb_func_start` macro includes `.global`, which makes the symbol visible to objdiff and the decompilation pipeline as an independent function. We replace all non-first macros with plain `.thumb_func` labels (keeping `.global` only if other files reference the symbol). This way:

- The bytes are preserved (ROM matches)
- The labels exist for internal branches
- But objdiff and Mizuchi don't see them as separate functions to decompile

## Tricky label detection

One subtle bug took several iterations to fix: Luvdis emits labels like `FUN_08000978: @ 08000978` — a label followed by an address comment. The naive check `line.endswith(":")` fails because the colon isn't the last character. This caused the merge phase to not recognize function labels as labels, absorbing real functions that should have been kept separate.

The fix was a proper label parser that finds the first `:` and checks that everything before it is a valid identifier — rather than relying on the colon being at the end of the line.

## Results

| Metric | Before | After |
|--------|--------|-------|
| Luvdis function boundaries | 688 | 688 |
| Usable function files | 48 (7%) | **488** (71%) |
| Fragment files (unusable) | 640 | 0 |
| Compiler artifacts hidden | 0 | 329 |
| Merge groups | 0 | 135 |
| ROM SHA1 match | - | Yes |

The 688 raw Luvdis boundaries became 488 properly-bounded, individually-decompilable functions with zero fragments — all while producing a byte-identical ROM.

An earlier version of the pipeline produced 475 entries by aggressively merging all fall-through fragments. The external-reference check (condition 2 in Phase 2) recovered 13 additional entries — functions like `MidiNoteDispatch`, `FixedPointMultiply`, and `StreamCmd_DmaSpriteData` that are real callable entry points, not compiler artifacts. Each of these is now independently decompilable.

The largest file (`FUN_080158ac`) is 10,341 lines — a 24-function chain in code_1 where every sub-function has backward branches into the parent's body, forcing co-location. Before the external-reference check, this was part of a 28-function, 13,237-line monster (`FUN_08014760`). The split at `FUN_080158ac` (an externally-called function) broke the chain into a 4-function head and a 24-function tail. These are genuinely massive state machines — likely game update routines with huge switch statements. The compiler generated them as single continuous code blocks with no internal prologues to split on. They'll need to be decompiled as-is.

## What can't be decompiled: fall-through thunks

The split revealed a pattern that is fundamentally impossible to express in C:

```asm
MidiDecodeByte: @ 0804F758          ← entry point 1: loads a field, falls through
    ldr r2, [r1, #0x40]

FUN_0804f75a:   @ 0804F75A          ← entry point 2: uses r2, called independently
    adds r3, r2, #0x1
    str r3, [r1, #0x40]
    ldrb r3, [r2, #0x00]
    b MidiNoteDispatch
```

`MidiDecodeByte` is a **thunk** — a one-instruction wrapper that sets up a register and falls through to `FUN_0804f75a`, which is also called directly from other code (with r2 already prepared). The combined logic is "read one byte from a stream pointer, advance the pointer, dispatch the byte."

In C, you can't express fall-through between functions. A function must either return or call. If `MidiDecodeByte` called `FUN_0804f75a`, the compiler would emit `bl` + `bx lr` — 6 extra bytes of call/return overhead that don't exist in the original code. Inlining would duplicate `FUN_0804f75a`'s body (which must also exist independently). There is no C construct for "execute this instruction, then continue into the next function."

The pipeline handles this correctly: `MidiDecodeByte` stays as a 3-line INCLUDE_ASM, and `FUN_0804f75a` gets its own INCLUDE_ASM immediately after. The assembler places them contiguously (they're in the same compilation unit), so the fall-through works. `FUN_0804f75a` is independently decompilable — only the thunk must remain as assembly.

This pattern appears in the m4a (MusicPlayer2000) sound engine, likely hand-optimized or produced by an older compiler pass. `MidiDecodeByte` is the "read from stream" entry point; `FUN_0804f75a` is the "process already-loaded byte" entry point. Two callers, one code path, zero overhead.

## Misplaced literal pools: splitting inside a 32-bit word

A subtle bug remained after the pipeline was working: the leaf function detector could split *inside* a literal pool word, attributing half the pool to the wrong function.

### The pattern

When a function ends with `bx r0` followed by padding and a 4-byte literal pool, Luvdis decodes the pool as two Thumb instructions:

```
bx r0                   @ function return
lsls r0, r0, #0x00     @ 0x0000 padding
ldr r5, [pc, #0x210]   @ 0x4D84 — lower halfword of .4byte 0x03004D84
lsls r0, r0, #0x0C     @ 0x0300 — upper halfword of .4byte 0x03004D84
ldr r1, [pc, #0x008]   @ real instruction — start of next function
```

The leaf function detector sees `bx r0` (hard return), skips the NOP, then finds `ldr r5, [pc, #0x210]` — which looks like the start of a valid leaf function (it can count ~7 instructions ending with `bx lr`). It places the split boundary at `0x0804E7E4` or `0x0804E7E6`, which is inside the literal pool word. The preceding function (`StreamCmd_StopSound`) loses its pool data, and the new sub-function (`StreamCmd_Nop3`) starts with a `.4byte` that isn't its own.

This made `StreamCmd_StopSound` impossible to decompile — the C compiler generates its own literal pool, but the INCLUDE_ASM didn't include the original pool data, so the bytes didn't match.

### The fix

A post-processing fixup (`_fix_misplaced_literal_pools`) runs after all asm files are written. It scans consecutive function pairs and detects the pattern: a function whose first content line is `.4byte`, where the preceding function has a `ldr rN, [pc, #imm]` referencing the same value. When found, it moves the `.4byte` from the sub-function to the end of the preceding function.

This fixed 5 instances across the codebase:

| Preceding function | Sub-function | Pool value |
|---|---|---|
| StreamCmd_StopSound | StreamCmd_Nop3 | `0x03004D84` |
| StreamCmd_GetStreamPtr | StreamCmd_ValidateStream | `0x0300081C` |
| StreamCmd_GetSoundReverb | SoundContextInit | `0x0300081C` |
| StreamCmd_StopAndAdvance | StreamCmd_ReadFreqParam | `0x0300081C` |
| StreamCmd_ReadFreqParam | StreamCmd_SetChannelMode | `0x0300081C` |

After the fix, `StreamCmd_StopSound` became a self-contained function with its own literal pool — and matched as simple C code:

```c
void StreamCmd_StopSound(void) {
    StopSoundEffects();
    gStreamPtr += 2;
}
```

### Root cause

Luvdis uses `ldr rN, [pc, #imm]` (pc-relative) syntax for literal pool references within sub-functions it doesn't know about, rather than `ldr rN, LABEL` (label-referenced) syntax. The `_has_unresolved_pool_refs` validation only checks for label-referenced `ldr` instructions, so it doesn't detect that the split point falls inside a literal pool. Additionally, the address computation (`_compute_addresses`) has a small drift from counting function labels with comments as 2-byte instructions, which shifts the computed pool target addresses — but even without the drift, the label-based validation would miss pc-relative pool references.

Applying the data-as-code conversion *before* sub-function detection (which would convert the literal pool bytes to `.4byte` directives and prevent the bad split) was considered but rejected: it changes instruction patterns globally, causing different split points across the entire codebase. The post-processing approach is targeted — it only fixes the specific misplaced pools without affecting any other function boundaries.

## Key takeaways

**`push {lr}` is the most reliable signal.** In Thumb code compiled by agbcc, every non-leaf function begins with `push {... lr}`. After a return instruction, a `push {lr}` in the unreachable zone is always a new function. This simple pattern recovered the majority of boundaries.

**Leaf functions need a different approach.** Functions that never call other functions don't push `lr`. We detect them by looking for short code sequences ending with `bx lr` after a hard return (`bx`/`pop {pc}`). The key distinction: we only trigger leaf detection on hard returns, not on `b label` which is usually an intra-function branch.

**Literal pools are the main constraint on splitting.** You can't split where a function's `ldr` instruction would lose access to its pool data. Validating pool references before accepting a split point prevented all assembler errors.

**Don't trust disassembler boundaries — but don't ignore them either.** Luvdis and Ghidra got the boundaries *mostly* right, but confused shared epilogues and literal pool addresses for function starts, and missed boundaries where there was no label in their database. The right approach is to use their boundaries as a starting point, then apply domain knowledge (Thumb calling conventions, agbcc codegen patterns) to fix the errors.

**Compiler artifacts are not functions.** A shared epilogue like `pop {r1}; bx r1` is a compiler optimization, not something a programmer wrote. Treating it as a function to decompile wastes effort and confuses the matching pipeline. Downgrading these to local labels is a net win for the decompilation workflow.

**External references distinguish real entry points from fragments.** A function that is `bl`'d or `b`'d from elsewhere is independently callable — it has its own calling convention and can be decompiled in isolation. Absorbing it into a merged group hides it from the decompilation pipeline. The cross-label-reference exception handles the edge case where two functions share code despite being separately callable: if one has backward branches into the other's body, the assembler needs them co-located to resolve short branch offsets.

**Literal pool data decoded as instructions can fool the leaf detector.** When a 4-byte pool word like `0x03004D84` is decoded as two Thumb instructions (`0x4D84` + `0x0300`), the leaf function detector may see the first decoded instruction as the start of a new function. The fix is a post-processing pass that detects `.4byte` data at the start of a sub-function and moves it to the preceding function's literal pool. This pattern appeared 5 times in the Klonoa codebase and blocked decompilation of all 5 preceding functions.
