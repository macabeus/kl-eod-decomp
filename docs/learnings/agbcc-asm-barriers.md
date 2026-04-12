# Removing `asm("")` Barriers in agbcc Decompilation

## Context

agbcc (GCC 2.95 for ARM7TDMI) has limited optimization capabilities and very specific register allocation behavior. Decompiled C code often uses `asm("")` inline assembly barriers to force the compiler to produce byte-identical output. This document catalogs when these barriers can be removed and when they cannot.

## Three Types of `asm("")` Barriers

### 1. Address Load Barrier: `asm("" : "=r"(ptr) : "0"(addr))`

Forces the compiler to load a constant address into a register at a specific point.

```c
// Before (with barrier):
u32 infoAddr = 0x0300081C;
u32 *info;
asm("" : "=r"(info) : "0"(infoAddr));
info[3] = 0;

// Equivalent to:
u32 *info = (u32 *)0x0300081C;
info[3] = 0;
```

**Why it exists**: Without the barrier, agbcc may:
- Load the address at a different point (earlier or later)
- Fold the address into a different register
- Combine it with other constants

**When removable**: When the address refers to an IWRAM global already defined as a `#define` cast (e.g., `#define gSoundInfo (*(u32 *)0x0300081C)`), and the resulting code happens to match.

### 2. Shift-Folding Barrier: `asm("" : "+r"(shifted))`

Prevents agbcc from optimizing `(x << N) >> M` into `x << (N-M)`.

```c
// Without barrier: agbcc compiles (idx << 24) >> 21 as idx << 3
// With barrier:
u32 shifted = idx << 24;
asm("" : "+r"(shifted));  // compiler must materialize shifted before continuing
shifted >>= 21;           // separate lsrs instruction
```

**Target assembly**:
```arm
lsls r0, r0, #24    ; shifted = idx << 24
ldr r2, =ADDR       ; <-- other instructions interleaved here
ldr r1, =ADDR2      ;
lsrs r0, r0, #21    ; shifted >>= 21
```

**Why it exists**: The original compiler emitted separate `lsls`/`lsrs` with `ldr` instructions between them. Without the barrier, agbcc folds the two shifts into one, changing the instruction count and literal pool layout.

**When removable**: Never, when `ldr` instructions must appear between the two shifts. No C construct can force instruction interleaving.

### 3. Register Pin: `register type var asm("rN")`

Forces a variable into a specific ARM register.

```c
register u32 *ptr asm("r0");
register s32 count asm("r1");
register s32 zero asm("r2");
```

**Why it exists**: The original code used specific registers for loop variables. Without pinning, agbcc freely chooses registers, producing different (but functionally equivalent) machine code.

**When removable**: When extern symbols produce the correct register assignment naturally. agbcc assigns registers to extern symbol loads based on declaration/usage order, so the right extern declarations often eliminate the need for register pins entirely (see `SetSpriteTableFromIndex` example below). Always try without the pin first and check `make compare`. Only keep the pin if the register choice doesn't match naturally.

## The Extern Symbol Technique

The most powerful technique for removing barriers: replace `#define` integer constants with `extern` linker symbols. This solves not only address-load barriers but also **instruction scheduling** and often **register assignment** — sometimes eliminating all three barrier types at once.

### Problem

```c
#define ROM_MUSIC_TABLE 0x08118AB4

void m4aSongNumStart(u32 idx) {
    u32 addr = ROM_MUSIC_TABLE;
    u8 *table;
    asm("" : "=r"(table) : "0"(addr));  // forces ldr at this point
    // ...
}
```

agbcc treats `0x08118AB4` as an integer constant. It may inline it, fold it with arithmetic, or schedule the load anywhere. The `asm` barrier forces the load to happen at a specific point.

### Solution

Define the symbol in the linker script and declare it as `extern` in C:

**ldscript.txt**:
```
gMPlayTable = 0x08118AB4;
gSongTable = 0x08118AE4;
```

**globals.h**:
```c
struct MusicPlayer { u32 info; u32 track; u16 numTracks; u8 unk_A; u8 pad; };
struct Song { u32 header; u16 ms; u16 pad; };

extern const struct MusicPlayer gMPlayTable[];
extern const struct Song gSongTable[];
```

**m4a.c**:
```c
void m4aSongNumStart(u32 idx) {
    const struct MusicPlayer *mplayTable = gMPlayTable;  // ldr via literal pool
    const struct Song *songTable = gSongTable;            // ldr via literal pool
    // ...
}
```

### Why It Works

When agbcc sees `extern const struct Song gSongTable[]`, it cannot inline the address — it must emit `ldr rN, =gSongTable` through the literal pool, which the linker resolves to `0x08118AE4`. This naturally creates the same instruction the `asm` barrier was forcing.

The linker symbol approach also enables proper struct access (`song->ms`, `mplay->info`) instead of raw pointer arithmetic, making the code much more readable.

### Extern Symbols Also Fix Instruction Scheduling

When a function loads two addresses and does arithmetic on a parameter, agbcc may interleave the parameter computation between the loads. Extern symbols fix this because the compiler hoists extern loads to function entry.

**Example — `SetSpriteTableFromIndex`**:

Target assembly has both `ldr`s before the `lsl`:
```arm
ldr r2, =0x030051DC    ; destination
ldr r1, =0x08189E84    ; table base
lsls r0, r0, #2        ; index * 4
adds r0, r0, r1
ldr r0, [r0]
str r0, [r2]
bx lr
```

With hardcoded addresses, agbcc interleaves the `lsl` between the loads:
```c
// WRONG scheduling: ldr r2; lsl r0; ldr r1 (shift between loads)
void SetSpriteTableFromIndex(u32 arg0) {
    *(u32 *)0x030051DC = ((u32 *)0x08189E84)[arg0];
}
```

With extern symbols, the compiler naturally loads both first — **zero barriers, zero register pins**:
```c
// CORRECT scheduling: ldr r2; ldr r1; lsl r0 (both loads before shift)
void SetSpriteTableFromIndex(u32 arg0) {
    gSpriteRenderPtr = gSpriteDataTable[arg0];
}
```

The extern approach also assigned the correct registers (r2, r1) naturally without `register asm()` pins. This replaced a version that needed both `asm("" : "+r"(...))` and two register pins.

### When It Doesn't Work

The extern approach fails when:
1. **The load must happen at a very specific instruction position** (e.g., `SoundCmd_Dispatch` where the table load must occur after a store, not before). The compiler hoists extern loads earlier than the `asm` barrier would place them.
2. **Register assignment matters**. `extern` loads go into whichever register the compiler chooses. `asm("" : "=r"(x) : "0"(addr))` forces `x` into the same register that held `addr`, giving indirect control over register assignment.
3. **Constant offset folding**. `((u32 *)gExternSym)[1]` compiles to `ldr r1, =(gExternSym+4); str r0, [r1, #0]` — the compiler folds the `+4` into the literal pool address. The target may need `ldr r1, =gExternSym; str r0, [r1, #4]` (base + offset). Neither `extern` nor `#define` can prevent this folding; only `asm("" : "=r"(ptr) : "0"(addr))` loads the raw base address without offset.

## The Orphan Block Pattern

C89 (which agbcc implements) requires variable declarations at the start of a block. Orphan `{ }` blocks exist for two reasons:

### 1. Mid-function variable declarations

```c
void func(void) {
    // ... code ...
    {
        u32 newVar = something;  // C89: must be at block start
        // use newVar
    }
}
```

**Removable when**: The variable can be hoisted to the function top without changing register allocation. Test by moving the declaration and running `make compare`.

### 2. Register lifetime control

```c
void func(void) {
    u32 *a = (u32 *)ADDR_A;
    a[0] = val1;
    a[1] = val2;

    {
        u32 *b = (u32 *)ADDR_B;  // b allocated AFTER a is no longer needed
        b[0] = val3;
    }
}
```

The block scope tells the compiler that `b` doesn't overlap with `a`'s lifetime, allowing register reuse. Without the block, the compiler may allocate `b` to a different register.

**Removable when**: The compiler naturally makes the same register choice with declarations hoisted. In practice, agbcc often makes different choices, so these blocks tend to be load-bearing.

### 3. Volatile DMA scope

```c
{
    vu32 *dma = (vu32 *)0x040000D4;
    dma[0] = src;
    dma[1] = dest;
    dma[2] = control;
    (void)dma[2];  // volatile read: wait for DMA
}
```

Volatile DMA scope blocks are **sometimes removable**. It depends on what happens before the block:

- **Removable** when the volatile pointer is only used after all function calls are complete (e.g., `m4aSoundVSyncOff` — DMA access happens after all setup). Declare the `vu32 *` at function top and reassign as needed.
- **Not removable** when the volatile pointer sits between function calls (e.g., `DecompressAndDmaCopy` — DMA happens between `DecompressData()` and `thunk_FUN_0800020c()`). Hoisting `vu32 *` to function top changes register allocation across the calls.

The key is whether the volatile pointer's lifetime overlaps with a function call that clobbers registers.

## The Volatile Store Technique

For address ordering barriers where code must store to address A before loading address B:

```c
// Before (with asm barrier):
u32 pauseFlagAddr = 0x030034E4;
u32 controlBlockAddr = 0x03004C20;
u8 *pauseFlag;
u32 *sceneCtrl;
asm("" : "=r"(pauseFlag) : "0"(pauseFlagAddr));
*pauseFlag = 1;
asm("" : "=r"(sceneCtrl) : "0"(controlBlockAddr));

// After (with volatile store):
*(vu8 *)0x030034E4 = 1;
sceneCtrl = (u32 *)0x03004C20;
```

**Why it works**: A volatile write is a side effect that the compiler cannot reorder past. agbcc must complete the `strb` before evaluating subsequent expressions. This prevents hoisting the `sceneCtrl` load before the store.

**When it works**: When the barrier's only purpose is to order a store before a load. The volatile qualifier on the store pointer is sufficient.

**Also works as volatile read**: `asm("" : "+r"(var))` barriers that prevent reordering a load past a subsequent address computation can be replaced by `*(volatile type *)ptr` reads:

```c
// Before:
fadeTimer = sceneCtrl[0];
asm("" : "+r"(fadeTimer));   // prevent reordering past next ldr
fadeCounter = (u8 *)0x03005498;

// After:
fadeTimer = *(vu32 *)sceneCtrl;  // volatile read = compiler barrier
fadeCounter = (u8 *)0x03005498;
```

**When it doesn't work**: When the barrier also controls which register the address goes into (the `"=r" : "0"` constraint ties output to input register).

## The Parameter Type Technique

For shift-folding barriers caused by `u32` parameter + `<<N >>M`:

```c
// Before (u32 param, needs shift barrier):
void m4aSongNumStart(u32 idx) {
    u32 shifted = idx << 16;
    asm("" : "+r"(shifted));           // prevent folding
    song = ... + (shifted >> 13);      // separate lsrs
}

// After (u16 param, no barrier needed):
void m4aSongNumStart(u16 idx) {
    song = &songTable[idx];            // compiler generates lsls+lsrs naturally
}
```

**Why it works**: When agbcc receives a `u16` parameter, it generates `lsls r0, r0, #16` (zero-extend) at function entry. The subsequent array indexing generates a separate shift. The compiler naturally places `ldr` instructions between these shifts because the loads are needed for the array base.

With `u32`, the compiler can fold `(x << 16) >> 13` into `x << 3` because no truncation is needed. With `u16`, the `<<16` is part of the ABI zero-extension and cannot be folded.

**Key insight**: pokeemerald and sa3 define `m4aSongNumStart(u16 n)` — the `u16` type is what the original source used. The `u32` parameter in Klonoa's initial decompilation was a type inference error that required shift barriers to compensate.

## Reference: How Other Decomp Projects Handle This

### pokeemerald (Pokemon Emerald)
- **Zero** `asm("")` barriers in all of `src/`
- Uses `extern` symbols extensively (`gMPlayTable`, `gSongTable`, etc.)
- Proper structs for all data tables
- Uses `u16` parameter types for song functions, eliminating shift patterns
- DMA access via macros (`DmaSet`, `DmaFill`) that use orphan blocks with volatile locals — this is considered acceptable

### sa3 (Sonic Advance 3)
- Only **10** `asm("")` barriers across entire codebase
- Wraps them in `#define MATCH_BREAK asm("")` behind `#ifndef NON_MATCHING`
- Uses `extern` symbols and structs like pokeemerald
- Uses `register ... asm("rN")` in a few places, also behind `NON_MATCHING` guards
- Their m4a.c has **zero** `asm` barriers — identical struct-based approach to pokeemerald

## Decision Tree

When you encounter an `asm("")` barrier:

1. **Is the address a ROM data table constant (0x08xxxxxx)?**
   - Try the extern symbol approach: add to ldscript.txt + extern declaration
   - Define a struct if the table has a fixed stride (8, 12, etc.)

2. **Is it an `asm("" : "+r"(var))` between two shifts?**
   - This prevents shift folding. Cannot be removed.
   - Check if extern symbols for surrounding loads eliminate the *need* for interleaved loads

3. **Is it `register ... asm("rN")`?**
   - First try extern symbols — they often produce the correct register assignment naturally (e.g., `SetSpriteTableFromIndex` needed no pins with externs).
   - If externs don't help, verify by removing the pin and checking `make compare` — if the same registers are chosen naturally, it can go.
   - Only keep the pin as a last resort when the register choice differs and affects encoding.

4. **Is it an IWRAM address (0x03xxxxxx) load barrier?**
   - Check if a `#define` global already exists in globals.h
   - Try direct assignment: `u32 *ptr = (u32 *)0x03004C20;`
   - If that fails, the barrier controls load ordering — keep it.

5. **Is it inside an orphan `{ }` with `vu32 *`?**
   - Volatile DMA scopes are always load-bearing. Keep the block.

## Register Pin Removal Audit (2026-03-22)

Attempted to remove all `register TYPE name asm("rN")` pins from decompiled functions.
All 15 pins across 4 files were investigated; none could be removed.

### Patterns that resist removal

**1. Byte-copy loop temp (StrCpy — r2)**
agbcc prefers to free r0 (first parameter) by copying it to r2, then reusing r0 for the `ldrb` result. The pin forces the opposite: keep dst in r0, use r2 for the loaded byte. No C construct (plain local, `register` hint, comma-operator loop) changed this allocation.

**2. Conditional rounding copy (FixedMul4, FixedMul8 — r1)**
After `muls r0, r1`, the pin forces a copy to r1 (`adds r1, r0, #0`) for conditional `+= 0xF` rounding. Without the pin, agbcc optimizes away the copy entirely, working directly on r0. The `register` keyword without `asm` and alternative loop structures all resulted in the same single-register optimization.

**3. Music player table lookup (m4aSongNumStart etc. — r2)**
The pin forces `gMPlayTable` into r2. Without it, agbcc assigns the extern load to r3 regardless of declaration order. pokeemerald achieves this without pins using struct member access (`mplay->info`) instead of raw byte arithmetic (`(u32)playerIdx * 12`), which may influence the allocator differently. Restructuring to match pokeemerald's approach would require changing the function's logic significantly.

**4. OAM buffer clearing loop (ClearVideoState etc. — r0, r1, r2)**
Three-pin pattern: pointer in r0, count in r1, zero in r2. agbcc assigns the literal pool load to r2 (not r0) and uses r0 for the inner loop counter. Hardcoded addresses, extern symbols, and declaration order all produced the same allocation. The allocator's preference for r2 as the `ldr` destination in no-parameter functions appears to be a fixed policy.

### Key takeaway

pokeemerald achieves zero register pins across its entire codebase, but its code uses different patterns (struct access vs raw pointer math, library calls vs hand-rolled loops). The Klonoa decompilation's use of low-level constructs (raw byte offsets, explicit loop counters) exposes register allocation decisions that higher-level C constructs would avoid. Converting to pokeemerald-style code would require significant refactoring beyond just removing the pins.
