# Extern Symbol Pattern for ROM Data Tables

## Problem

This project originally used `#define` integer constants for all ROM data table addresses:

```c
#define ROM_MUSIC_TABLE     0x08118AB4
#define ROM_MUSIC_META_TABLE 0x08118AE4
```

When these constants are used in C code, agbcc treats them as immediate values and may:
- Inline them into arithmetic expressions
- Schedule their loads at any point
- Fold them with other constants

This forced decompiled code to use `asm("")` barriers to control when and how these addresses were loaded into registers.

## Solution

Define the addresses as **linker symbols** and declare them as **extern arrays** in C headers.

### Step 1: Add symbol to ldscript.txt

```
gMPlayTable = 0x08118AB4;
gSongTable = 0x08118AE4;
```

These go at the top of `ldscript.txt`, before the SECTIONS block.

### Step 2: Define struct (if table has fixed-stride entries)

In `globals.h`:

```c
struct MusicPlayer {
    u32 info;
    u32 track;
    u16 numTracks;
    u8 unk_A;
    u8 pad;
};

struct Song {
    u32 header;
    u16 ms;
    u16 pad;
};
```

### Step 3: Declare extern in globals.h

```c
extern const struct MusicPlayer gMPlayTable[];
extern const struct Song gSongTable[];
```

Keep the `#define` for backward compatibility:
```c
#define ROM_MUSIC_TABLE 0x08118AB4
```

### Step 4: Use in C code

```c
// Before (with 3 asm barriers + register pin):
void m4aSongNumStart(u32 idx) {
    u32 shifted = idx << 16;
    u32 musicTableAddr = ROM_MUSIC_TABLE;
    u32 metaTableAddr = ROM_MUSIC_META_TABLE;
    register u8 *musicTable asm("r2");
    u8 *metaTable;
    u8 *songEntry;

    asm("" : "+r"(shifted));
    asm("" : "=r"(musicTable) : "0"(musicTableAddr));
    asm("" : "=r"(metaTable) : "0"(metaTableAddr));
    shifted >>= 13;
    shifted += (u32)metaTable;
    songEntry = (u8 *)shifted;

    u16 playerIdx = *(u16 *)(songEntry + 4);
    u32 playerOff = (u32)playerIdx * 12 + (u32)musicTable;
    MPlayStart(*(u32 *)playerOff, *(u32 *)songEntry);
}

// After (with extern symbols + u16 param — zero asm barriers):
void m4aSongNumStart(u16 idx) {
    register const struct MusicPlayer *mplayTable asm("r2") = gMPlayTable;
    const struct Song *songTable = gSongTable;
    const struct Song *song = &songTable[idx];
    u16 playerIdx = song->ms;
    u32 playerOff = (u32)playerIdx * 12;
    playerOff += (u32)mplayTable;
    MPlayStart(*(u32 *)playerOff, song->header);
}
```

Result: all 3 `asm` barriers removed. The `u16` parameter type eliminates the shift-folding barrier (see `agbcc-asm-barriers.md` — "The Parameter Type Technique"), and the extern symbols eliminate the address-load barriers. Only the `register asm("r2")` pin remains because the target requires `mplayTable` in `r2` specifically.

**Important**: The `u16` parameter type is not a workaround — pokeemerald and sa3 both define `m4aSongNumStart(u16 n)`. The original `u32` parameter was a type inference error during initial decompilation.

## When This Works vs. Doesn't

### Works well for:
- **ROM data tables** (0x08xxxxxx) accessed by index with a fixed stride
- Tables that multiple functions reference (the extern can be reused)
- Cases where the `asm` barrier's only purpose was to force a `ldr` instruction from the literal pool

### Also works for IWRAM addresses (sometimes):
- `gGameFlagsPtr` (`0x03005400`) was successfully defined as an extern linker symbol for `GetEntityLookupData`, allowing both `ldr` instructions to appear between the two shift instructions naturally. The key was that the extern forces a literal pool load instead of an immediate materialization.
- **Declaration order matters**: agbcc assigns registers in declaration order. If the target has `ldr r2, =flags; ldr r1, =table`, declare flags first, then table.

### Doesn't work for:
- **Cases requiring specific load timing** — extern loads happen at compiler's discretion (e.g., `SoundCmd_Dispatch` where the table must load after a `str`, not before)
- **Multiple stores to the same base address** — `((u32 *)0x030047C0)[0]` and `[1]` generate separate `ldr` per access instead of reusing a register; the asm barrier forces the base into a persistent register

## Symbols Defined So Far

| Symbol | Address | Type | Used by |
|--------|---------|------|---------|
| `gMPlayTable` | `0x08118AB4` | `struct MusicPlayer[]` | `m4aSongNumStart`, `m4aMPlayCommand`, `m4aSongNumStop` |
| `gSongTable` | `0x08118AE4` | `struct Song[]` | Same |
| `gEntityDataTable` | `0x081168E8` | `u8[]` | `GetEntityLookupData` |
| `gLevelPaletteTable` | `0x08189B4C` | `u32[]` | `FinalizeLevelLayerSetup` |
| `gStreamDataTable` | `0x08189AFC` | `u32[]` | `LoadAndDecompressStream` |
| `gSoundCmdTable` | `0x08117C8C` | `u32[]` | (defined but unused — SoundCmd_Dispatch still needs asm for timing) |
| `gGameFlagsPtr` | `0x03005400` | `u8[]` | `GetEntityLookupData` |
| `gVBlankCallbackArray` | `0x030047C0` | `u32[]` | `SetupGfxCallbacks` |
| `gSpriteRenderPtr` | `0x030051DC` | `u32` | `SetSpriteTableFromIndex` |
| `gSpriteDataTable` | `0x08189E84` | `const u32[]` | `SetSpriteTableFromIndex` |

## Combining with Parameter Type Changes

The extern symbol pattern is most powerful when combined with correct parameter types. The song functions went from 3 asm barriers + register pin to just a register pin:

1. `u32 idx` → `u16 idx` eliminated the shift-folding barrier (`asm("" : "+r"(shifted))`)
2. `extern gSongTable[]` + `extern gMPlayTable[]` eliminated both address-load barriers (`asm("" : "=r" : "0")`)
3. Only `register asm("r2")` remains (verified irreducible — target needs `r2` specifically)

Similarly, `GetEntityLookupData` went from 3 asm barriers to zero:

1. `u32 idx` → `u8 idx` eliminated the shift-folding barrier
2. `extern gEntityDataTable[]` + `extern gGameFlagsPtr[]` eliminated both address-load barriers
3. **Declaration order** (`flags` before `table`) was critical to match register assignment (`r2` for flags, `r1` for table)

### Extern symbols can also fix instruction scheduling and register allocation

`SetSpriteTableFromIndex` demonstrates the strongest case: extern symbols eliminated **all** barriers and **all** register pins at once.

With hardcoded addresses, agbcc interleaved a `lsl` between two `ldr` instructions and assigned registers differently from the target. An initial fix used `asm("" : "+r"(...))` + two `register asm("rN")` pins. But replacing both addresses with extern symbols produced the correct instruction order and register assignment naturally:

```c
// Before (3 workarounds):
void SetSpriteTableFromIndex(u32 arg0) {
    register u32 *dst asm("r2") = (u32 *)0x030051DC;
    register u32 *table asm("r1") = (u32 *)0x08189E84;
    asm("" : "+r"(dst), "+r"(table));
    *dst = table[arg0];
}

// After (zero workarounds):
void SetSpriteTableFromIndex(u32 arg0) {
    gSpriteRenderPtr = gSpriteDataTable[arg0];
}
```

**Why it works**: Extern symbols force literal pool loads (`ldr rN, =sym`). agbcc hoists these loads to function entry, placing both before any parameter computation. The compiler also happened to assign the correct registers (r2 for destination, r1 for table) — matching the target without any `register asm()` pins.

**Key takeaway**: Before reaching for `asm("")` barriers or `register asm()` pins, try extern symbols first. They are the cleanest solution and often handle scheduling, register allocation, and load ordering all at once.

## Inspiration

This pattern comes directly from pokeemerald and sa3 (Sonic Advance 3) decomp projects, where m4a.c uses `extern const struct MusicPlayer gMPlayTable[]` and `extern const struct Song gSongTable[]` with zero `asm` barriers in the song lookup functions. Both projects use `u16` parameter types for the song functions.
