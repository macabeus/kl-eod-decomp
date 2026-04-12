# Catalog of Remaining asm Barriers and Orphan Blocks

## Remaining `asm("")` Barriers: 0

All `asm("")` barriers have been eliminated. The original 42 barriers were removed across 9 rounds using these techniques:

| Technique | Barriers removed | Key insight |
|-----------|-----------------|-------------|
| Extern linker symbols | 17 | Forces `ldr rN, =sym` via literal pool; prevents constant folding |
| Direct `register asm("rN") = expr` init | 4 | Eliminates the `"=r" : "0"` barrier when combined with extern symbol for the address |
| Parameter type narrowing (`u32` → `u16`/`u8`) | 4 | Natural `lsls`/`lsrs` pair from ABI zero-extension replaces manual shift barriers |
| Volatile store (`*(vu8 *)addr = val`) | 4 | Compiler cannot reorder past volatile side effect |
| Volatile read (`*(vu32 *)ptr`) | 1 | Forces load to happen before subsequent address computation |
| Direct pointer init (`type *p = (type *)ADDR`) | 5 | Works when the compiler naturally produces the right scheduling |
| Using existing `#define` globals | 3 | Works for simple single-dereference patterns |
| Inline extern in function argument | 1 | `func(gExternSym[idx])` places load at exactly the right point |
| Hoisting declarations to function top | 3 | Some barriers were only needed because of C89 block scope |

## Remaining `register asm("rN")` Pins: 15 declarations across 6 functions

These are verified irreducible — without them, agbcc uses different registers which produce different Thumb opcodes.

### EntityDeathAnimation (1 function × 2 registers)

```c
register int mask asm("r4") = 0xFF;        /* block-scoped after timer store */
register int ents asm("r3") = 0x03002920;  /* block-scoped for second half */
```

Both use block-scoped direct initialization (no `asm` barriers). Without `r4` pin, agbcc puts the zero-extended timer value in r4 instead of 0xFF. Without `r3` pin, agbcc reuses r7 from the first half instead of reloading into r3. Direct init at function scope fails for mask (emits `movs r4, #0xFF` too early); block scope controls the scheduling point.

### Engine zero-fill loops (3 functions × 3 registers)

```c
register u32 *oamEntry asm("r0") = (u32 *)gOamBuffer0;
register s32 entryCount asm("r1") = 0x63;
register s32 zeroFill asm("r2") = 0;
```

Without pinning, agbcc uses r2/r1/r3 instead of r0/r1/r2, and restructures the loop.

### m4a song table lookup (3 functions × 1 register)

```c
register const struct MusicPlayer *mplayTable asm("r2") = gMPlayTable;
```

Without pinning, agbcc puts mplayTable in r3 instead of r2.

### FixedMul8 (1 function × 1 register)

```c
register s32 shifted asm("r1") = result;
```

Without pinning, agbcc keeps everything in r0 and never copies to r1.

## Remaining Orphan `{ }` Blocks: 0

All orphan blocks have been eliminated.

The last one — `DecompressAndDmaCopy`'s volatile DMA scope — was removed by declaring `vu32 *dma3` at function top, between `buf` and `DecompressData()`:

```c
// Before (orphan block):
void DecompressAndDmaCopy(u32 src, u32 dest, u32 size) {
    u32 buf = AllocAndDecompress((u32 *)src);
    DecompressData(buf, src);
    {
        vu32 *dma3 = (vu32 *)0x040000D4;
        ...
    }
    thunk_FUN_0800020c(buf);
}

// After (no block):
void DecompressAndDmaCopy(u32 src, u32 dest, u32 size) {
    u32 buf = AllocAndDecompress((u32 *)src);
    vu32 *dma3;
    DecompressData(buf, src);
    dma3 = (vu32 *)0x040000D4;
    ...
    thunk_FUN_0800020c(buf);
}
```

**Key insight**: Declaration position within the function top matters for register allocation in agbcc. Declaring `vu32 *dma3` after `buf` (but before the `DecompressData` call) puts it in the right position in the compiler's variable list. Previous attempts declared it before `buf` or at the very top, which changed register assignment across function calls.

## What Worked (complete catalog of techniques)

| Technique | Barriers eliminated | Orphan blocks removed | Functions |
|-----------|--------------------|-----------------------|-----------|
| Extern linker symbols | 17 `"=r" : "0"` | 1 | 11 functions |
| Direct register init with extern | 4 `"=r" : "0"` | 0 | ClearVideoState, ClearOamBuffer*, ClearOamEntries6Plus, FixedMul8 |
| Parameter type narrowing | 4 `"+r"` | 0 | m4aSongNumStart/Command/Stop, GetEntityLookupData |
| Volatile store | 4 `"=r" : "0"` | 0 | TransitionToWorldMap, TransitionSoftReset |
| Volatile read | 1 `"+r"` | 0 | FadeOutController |
| Direct pointer init | 5 `"=r" : "0"` | 0 | FadeOutController, StreamCmd_*, SoundClear |
| `#define` globals | 3 `"=r" : "0"` | 0 | MPlayMain_SetAndProcess, FreeAllDecompBuffers, IsSelectButtonPressed |
| Inline extern in arg | 1 `"=r" : "0"` | 0 | SoundCmd_Dispatch |
| Hoisting declarations | 0 | 15 | Multiple functions |
| Hoisting volatile DMA | 0 | 2 | m4aSoundVSyncOff |
| Declaration position tuning | 0 | 1 | DecompressAndDmaCopy (vu32 *dma3 declared after buf) |
| Block-scoped direct register init | 2 `"=r" : "0"` | 0 | EntityDeathAnimation (mask + base reload via `register asm = val` in `{ }` blocks) |
