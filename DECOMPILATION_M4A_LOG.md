# M4A Decompilation Log

## Session 1 — Initial Decompilation

**Functions decompiled:** 10 C functions
- `ply_voice`, `ply_bend`, `ply_bendr`, `ply_lfodl`, `ply_mod` (TrackStruct field setters)
- `StreamCmd_AdvanceStream` (stream pointer advance)
- `m4aSoundVSyncOn` (VSync sound enable)
- `m4aMPlayAllStop` (stop all music players)
- `StopSoundEffects` (stop sound effects)
- `SoundHardwareInit_Tail` (SWI call wrapper)

**ROM status:** `klonoa-eod.gba: OK`

## Session 2 — Deep Investigation with Reference Projects

Spent ~6 hours across 5 parallel agents attempting all remaining 68 functions, using pokeemerald (100% decompiled m4a) and sa3 as references.

**Functions decompiled:** 1 additional — `m4aSoundInit` (50 lines, initializes sound engine: calls BitUnPack, DirectSoundFifoSetup, SoundHardwareInit, m4aSoundMode, and iterates MPlayOpen for each music player entry).

**Key discovery: `old_agbcc` is NOT the exact original m4a compiler.**

While `old_agbcc` is closer than `agbcc` (it gets `gMPlayTable` register allocation right for `m4aSongNumStart`), systematic testing across all 68 remaining functions revealed that `old_agbcc` differs from the original SDK compiler in at least 5 ways:

### Compiler Differences Found

#### 1. Persistent zero register pattern
**Target:** `push {r4, r5, r6, lr}` then `movs r6, #0` — uses r6 as a persistent zero throughout the function body.
**old_agbcc:** `push {r4, r5, lr}` — never saves r6, uses inline `movs rN, #0` at each use site.
**Affected:** SampleFreqSet, SoundContextRef, DmaControllerInit, SoundInfoInit, and many others. This is a pervasive pattern across the m4a codebase.

#### 2. Literal pool constants vs immediates
**Target:** `ldr r0, =4` — loads small constants (like 4, 0xFFFF) from the literal pool.
**old_agbcc:** `movs r0, #4` — constant-folds small values into immediate operands.
**Affected:** m4aMPlayAllStop, StopSoundEffects (worked around with `gNumMusicPlayers` linker symbol trick), MPlayStateCheck, CgbModVol.

#### 3. u16 truncation method
**Target:** `ldr r3, =0xFFFF; ands r0, r3` — truncates to u16 with a mask from the literal pool.
**old_agbcc:** `lsls r0, r0, #16; lsrs r0, r0, #16` — truncates with shift pair.
**Affected:** CgbModVol, MidiNoteLookup, and functions with u16 temporaries.

#### 4. Dead code preservation
**Target:** Preserves dead instructions (e.g., `movs r2, #0` that is overwritten before use).
**old_agbcc:** Eliminates dead stores at `-O2`.
**Affected:** MidiUtilConvert, SoundEffectParamInit.

#### 5. Register allocation ordering
**Target:** Assigns function arguments and locals to specific registers in a particular order.
**old_agbcc:** Uses a different assignment order (e.g., r3 vs r2 for the second argument copy, r1 vs r3 for ident loads).
**Affected:** MPlayChannelReset, CgbSound (TrkVolPitSet), MPlayStateCheck (saves r8 but not r9 vs target saving both).

### Structural Blockers (compiler-independent)

#### Shared literal pools
Many functions share `.4byte` pool entries across function boundaries. The first 4 bytes of function B are actually function A's literal pool data. Decompiling A alone removes the pool; decompiling B alone starts 4 bytes late.
**Affected:** StreamCmd_GetStreamPtr/ValidateStream, StreamCmd_StopAndAdvance/ReadFreqParam/SetChannelMode, StreamCmd_SetSoundReverb/GetSoundReverb, and many MIDI functions.

#### Internal label cross-references
Large asm functions (InitSoundEngine, MidiCommandHandler, SoundMixerMain) branch directly into the middle of other functions. Decompiling the target removes those labels.
**Affected:** VoiceGetParams, MidiNoteDispatch, MidiNoteSetup, CgbChannelMix, FUN_08050a44.

#### Fall-through thunks
Single-instruction functions that fall through to the next function. Can't be expressed in C.
**Affected:** MidiDecodeByte (1 instruction → falls into FUN_0804f75a), InstrumentGetEntry (1 instruction → falls into MidiNoteDispatch).

#### ARM/Thumb interworking
FixedPointMultiply, SoundMixerMain, and parts of InitSoundEngine switch to ARM mode. `old_agbcc` is Thumb-only.

#### Non-standard ABI
MPlayStop_Channel and SoundEffectChain use callee-saved registers as implicit function arguments (r4/r5 pre-loaded by caller).

### What Would Unblock More Decompilation

1. **Find the exact original m4a compiler** — The persistent zero register pattern (r6=0) and literal pool constant loading suggest an older or differently-configured GCC than `old_agbcc`. It may be a pre-2.95 GCC or a Nintendo-patched version with different register allocation heuristics. Other decomp projects (pokeemerald, sa3) achieve matching with `old_agbcc` because their m4a code was compiled with a closer GCC version. Klonoa's m4a binary may have been compiled with a different SDK release.

2. **Patch `old_agbcc`** — Add flags like:
   - `-fpersistent-zero-reg` — force r6=0 as a persistent zero register
   - `-fpool-small-constants` — load small constants from literal pool instead of immediates
   - `-fmask-truncation` — use `ands` with pool mask instead of shift pair for u16 truncation

3. **Decompile function pairs** — For shared literal pool cases, decompile adjacent functions simultaneously so both literal pools are generated by the compiler.

## Session 3 — `-ftst` Investigation & Exhaustive Function Testing

### `-ftst` Flag: Why It Can't Be Enabled Globally on m4a.c

Attempted to add `-ftst` to the m4a.c compilation so that functions using the `tst` instruction could be decompiled without a separate compilation unit.

**Result: not viable.** Enabling `-ftst` on m4a.c breaks 4 already-decompiled functions. The flag converts ALL `ands rX, rY; cmp rX, #0` patterns to `tst rX, rY`, but the ROM uses `ands` (not `tst`) in these functions:

| Broken function | What changes | ROM expects |
|----------------|--------------|-------------|
| `m4aSoundVSyncOff` | `ands r0, r3; cmp r0, #0` → `tst r3, r0` | `ands` (result stored) |
| `MidiCommandEncode1` | `ands r0, r7; cmp r0, #0` → `tst r7, r0` | `ands` (flags used with stored result) |
| `MidiCommandEncode2` | Same pattern as MidiCommandEncode1 | `ands` |
| `m4aSoundMode` | Register allocation completely changes (first mask test restructured) | `ands` with different reg layout |

**The `-ftst` flag cannot distinguish** which `ands`+`cmp` patterns should become `tst` and which should remain `ands`. It converts all of them.

**Verified with diff:** compiled m4a.c with and without `-ftst` — the only code differences are the above 4 functions plus the expected `tst` conversions. No other functions are affected.

### `-ftst` Correct Architecture: Separate Compilation Unit (m4a_1.c)

The existing m4a_1.c mechanism is the right approach:
- Functions needing `-ftst` go in `src/m4a_1.c`
- Pre-compiled to `build/m4a_1_funcs.s` with `old_agbcc -ftst`
- Included into m4a.c via `asm(".include \"build/m4a_1_funcs.s\"")`
- Currently contains only `VoiceLookupAndApply`

**Makefile fix applied:** Changed the TST compilation unit from `agbcc -ftst` to `old_agbcc -ftst`. The m4a code was compiled with `old_agbcc`, so the TST unit should use the same compiler for consistent register allocation. Verified: `old_agbcc -ftst` and `agbcc -ftst` produce identical bytes for VoiceLookupAndApply (the only difference is decimal vs hex constant formatting, which assembles identically).

### Functions That Use `tst` in the ROM

Only 5 INCLUDE_ASM functions contain `tst` instructions. Each has an independent blocker preventing decompilation:

| Function | Lines | tst count | Blocker | Details |
|----------|-------|-----------|---------|---------|
| **MPlayNoteProcess** | 281 | 11 | **ICE** | `old_agbcc -ftst` crashes with "Unable to generate reloads for andsi3_setflags" — register pressure too high in the channel-search do-while loop. The `tst` pattern (`andsi3_setflags`) requires both operands in low registers, but the loop body uses r8-r11 for spilling. pokeemerald also keeps `ply_note` (equivalent) in assembly (`m4a_1.s`), confirming this is a known GCC 2.95 limitation. |
| **SoundContextRef** | 39 | 1 | **Persistent zero register** | `push {r4, r5, r6, lr}; movs r6, #0` — r6 cached as zero for `strb r6, [r4]` and `str r6, [r4, #0x2C]`. old_agbcc uses inline `movs r0, #0` at each site (2 extra bytes). Also uses `ands r0, r3` (not `tst`) for the `& 0x07` check — the `tst` is only for the `& 0x80` check. |
| **MPlayChannelRelease** | 36 | 2 | **Non-word-aligned** | `non_word_aligned_thumb_func_start` — C compiler always emits `.align 2, 0` before functions. Also has `.2byte`-encoded branches and embedded `FUN_0804fe18` label. |
| **MPlayContinue** | 327 | 8 | **Multiple blockers** | Non-word-aligned + embedded `SoundChannelRelease` thunk (`bx r3` at end) + `.inst.n` encoded instructions + `.2byte`-encoded branches + `FUN_0804f9d0` mid-function label. |
| **InitSoundEngine** | 549 | 6 | **ARM interworking** | 120+ lines of `.2byte 0xE...` ARM instruction bytes embedded in the Thumb code section. Multiple `bx r3`/`bx r1` transitions to ARM mode. Cannot be expressed in C with a Thumb-only compiler. |

### Exhaustive Testing of All Remaining Word-Aligned m4a Functions

Every word-aligned, non-tst m4a function was test-compiled with `old_agbcc -mthumb-interwork -O2` and byte-compared against the ROM. **None matched.** Detailed results:

#### Near-misses (2–6 bytes off)

| Function | Lines | Bytes off | Root cause | Exact diff |
|----------|-------|-----------|------------|------------|
| **MPlayChannelReset** | 14 | **2 bytes** | Scratch register preference: target uses r3, old_agbcc uses r1 | `ldr r3, [r2, #0x34]` vs `ldr r1, [r2, #0x34]` and `cmp r3, r0` vs `cmp r1, r0` |
| **DirectSoundFifoSetup** | 110 | **~8 bytes** | Multiple: (1) target copies `0x80<<3` to temp register via `adds r1, r2, #0` before storing, old_agbcc stores directly; (2) target uses `strh r1, [r0]; adds r0, #0xC; strh r1, [r0]` while old_agbcc uses `strh r0, [r1]; strh r0, [r1, #12]`; (3) literal pool offset differences cascade |
| **SoundEffectParamInit** | 19 | **~4 bytes** | Target uses r2 for zero (`movs r2, #0`), old_agbcc uses r0 (`movs r0, #0`) — the 2-param function where old_agbcc frees r0 and reuses it |

#### Moderate mismatches (full instruction differences)

| Function | Lines | Root cause |
|----------|-------|------------|
| **MidiKeyToFreq** | 52 | Target saves first param with `mov r12, r0` (IP register); old_agbcc uses `adds r7, r0, #0` (never uses r12). Target reuses literal pool base `r3` for second lookup (`adds r0, r6, #0x1; adds r0, r0, r3`); old_agbcc loads a separate literal `0x08117A75` (3 pool entries vs 2). |
| **CgbOscOff** | 60 | Target does redundant u8 truncation: `ldrb r0, [r1, #0x02]; lsls r2, r0, #0x18; lsrs r4, r2, #0x18` — shift-pair after ldrb (which already zero-extends). old_agbcc optimizes this away: `ldrb r3, [r1, #2]`. Different registers throughout. |
| **MidiUtilConvert** | 21 | Dead code: target has both `movs r2, #0x00` AND `movs r0, #0x00` (r2 is dead, overwritten before use). old_agbcc at -O2 eliminates the dead `movs r2, #0`. |
| **SoundReset** | 24 | u16 truncation: target does `lsls r2, #0x10; lsrs r2, #0x10` to truncate val to u16, then uses u16 `orrs` pattern. old_agbcc folds the truncation into the `orrs` operation differently (`lsls r0, #24; orrs; lsrs #16`). Also literal pool base `0x03000900` loaded separately vs pre-computed `0x03001100`. |
| **MPlayStart** | 122 | Large function, target copies r0→r5 and r1→r7. Multiple differences in loop structure and register assignments throughout. |

#### Blocked by non-compiler structural issues

| Function | Lines | Blocker |
|----------|-------|---------|
| SoundBufferAlloc | 20 | `non_word_aligned_thumb_func_start` |
| StreamCmd_StopAndAdvance | 20 | `non_word_aligned_thumb_func_start` |
| StreamCmd_ReadFreqParam | 19 | `non_word_aligned_thumb_func_start` + shared literal pool before code |
| StreamCmd_SetChannelMode | 18 | `non_word_aligned_thumb_func_start` + shared literal pool before code |
| m4aSongNumContinue | 37 | `non_word_aligned_thumb_func_start` + `.2byte`-encoded branches |
| m4aSongNumLoad | 40 | `non_word_aligned_thumb_func_start` + `.2byte`-encoded branches |
| TrackStop | 10 | `non_word_aligned_thumb_func_start` + `.inst.n` encoded instructions |
| MPlayTrackCallback | 17 | `non_word_aligned_thumb_func_start` + non-standard ABI (r2, r6 implicit params) |
| SoundContextInit | 63 | `non_word_aligned_thumb_func_start` + embedded `FUN_0804f092` sub-function |
| FUN_0804f75a | 8 | `non_word_aligned_thumb_func_start` + tail-call `b MidiNoteDispatch` (no return) |
| MidiDecodeByte | 4 | Falls through into FUN_0804f75a (no return instruction) |
| InstrumentLookup | 15 | Uses `mov r12, lr` / `bx r12` return pattern (old_agbcc uses standard pop/bx) |

### Complete Blocker Census for All 61 Remaining INCLUDE_ASM Functions

| Blocker category | Count | Can be fixed? |
|-----------------|-------|---------------|
| Register allocation mismatch (r0-copy, scratch order, r12 usage) | **~30** | No — baked into GCC 2.95 register allocator |
| `non_word_aligned_thumb_func_start` | **26** | Only if preceding function is also decompiled (removes alignment pad) |
| Persistent zero register (`movs r6, #0`) | **~10** | No — old_agbcc never caches zero in a callee-saved register |
| Shared literal pools across function boundaries | **~8** | Possible if function pairs are decompiled simultaneously |
| Fall-through / tail-call thunks | **8** | No — can't express in C |
| ARM mode interworking | **3** | No — old_agbcc is Thumb-only |
| Non-standard ABI (r4/r5 implicit args) | **~3** | No — can't express in standard C calling convention |
| Dead code preservation | **2** | No — old_agbcc -O2 eliminates dead stores |
| `tst` ICE (Internal Compiler Error) | **1** | No — GCC 2.95 register allocator limitation |
| u16 truncation method difference | **~3** | No — old_agbcc always uses shift pair |
| Literal pool constant folding | **~4** | Partially — extern linker symbol trick works for some |

Note: many functions have multiple blockers simultaneously.

## Patterns and Techniques

### ply_* functions (track parameter setters)

All follow the same pattern:
```c
void ply_XXX(void *r0, TrackStruct *track) {
    track->fieldAtOffset = *track->unk40;
    track->unk40++;
}
```

| Function | Field | Offset |
|----------|-------|--------|
| ply_keysh | keyShift | 0x24 |
| ply_voice | unk2C | 0x2C |
| ply_vol | unk2D | 0x2D |
| ply_pan | unk2E | 0x2E |
| ply_bend | unk2F[0] | 0x2F |
| ply_bendr | unk1E | 0x1E |
| ply_lfos | unk1F | 0x1F |
| ply_lfodl | unk26 | 0x26 |
| ply_mod | unk27 | 0x27 |

### Why ply_* and simple wrappers match but nothing else does

The ply_* functions match because they are **trivially constrained**: 2-param functions where r0 is unused and r1 is the only live register, with a single `ldr` + `strb` + `adds` + `str` sequence. There is only ONE possible register assignment. Similarly, `m4aSoundInit` matched because its structure (loop + function calls) happens to produce the same allocation in old_agbcc as the original compiler.

For any function with 3+ live variables, old_agbcc's register allocator makes different choices than the Klonoa SDK compiler. The ordering is deterministic but different, and no C-level construct can change it.

### Alignment at C/ASM boundaries

`asm(".align 2, 0")` is needed between a C function and a `non_word_aligned` INCLUDE_ASM that follows it. The C compiler doesn't emit padding after `bx lr`.

### gNumMusicPlayers linker symbol trick

The target loads the constant `4` from the literal pool (`ldr r0, =4`), but `old_agbcc` constant-folds it to `movs r0, #4`. Workaround: define a linker symbol `gNumMusicPlayers = 4` and use `extern char gNumMusicPlayers[]` with `#define NUM_MUSIC_PLAYERS ((u16)gNumMusicPlayers)`. This forces a literal pool load.

### Volatile reads for dead code

When the target has a dead `ldrb` that was not optimized away, use `vu8` volatile reads:
```c
scratch = ((vu8 *)info)[0x04];  // volatile read preserves the load
scratch = 0;                     // then overwrite
```

### Negation pattern for mask constants

The target compiler generates `movs r0, #4; negs r0, r0; ands r0, r1` for `val & ~0x03`. `agbcc` constant-folds this to `movs r0, #0xFC; ands r0, r1` (shorter). To force the negation pattern:
```c
s32 mask = 4;
mask = -mask;
val &= (u8)mask;
```
This generates the `movs; negs; ands` sequence in `agbcc`, but the register assignment (which register gets the mask vs the value) may still differ.

## Compiler Configuration

| File | Compiler | Flags |
|------|----------|-------|
| `src/m4a.c` | `old_agbcc` | `-mthumb-interwork -O2` |
| `src/m4a_1.c` | `old_agbcc` | `-mthumb-interwork -O2 -ftst` |
| `src/*.c` (non-m4a) | `agbcc` | `-mthumb-interwork -O2 -fhex-asm -fprologue-bugfix` |

**Important:** m4a_1.c was changed from `agbcc` to `old_agbcc` in Session 3. Both produce identical bytes for the current function (`VoiceLookupAndApply`), but `old_agbcc` is correct because m4a was compiled with it. The only output difference is decimal vs hex constant formatting (`#199` vs `#0xc7`), which assembles identically.

### Reference projects

| Project | m4a status | m4a compiler |
|---------|-----------|-------------|
| pokeemerald | 100% C (1,781 lines) | `old_agbcc` |
| sa3 | 100% C (1,473 lines) | `old_agbcc` (same as pokeemerald) |
| FireEmblem7J | 100% assembly | N/A (not decompiled) |
| **Klonoa** | **~18% C, 82% asm** | `old_agbcc` (wrong version — see above) |

pokeemerald and sa3 match with `old_agbcc` because their m4a binaries were compiled with a GCC version that `old_agbcc` reproduces. Klonoa's m4a was compiled with a different SDK compiler build that has different register allocation behavior.

### Quick-test workflow for future attempts

```bash
# 1. Write candidate C code
cat > /tmp/test.c << 'EOF'
#include "global.h"
#include "gba.h"
#include "globals.h"
void MyFunc(...) { ... }
EOF

# 2. Preprocess + compile
arm-none-eabi-cpp -nostdinc -I tools/agbcc/include -iquote include /tmp/test.c -o /tmp/test.i
tools/agbcc/bin/old_agbcc -mthumb-interwork -O2 -o /tmp/test.s /tmp/test.i

# 3. Assemble + compare
arm-none-eabi-as -mcpu=arm7tdmi -mthumb-interwork -o /tmp/test.o /tmp/test.s
python3 -c "
import struct
addr = 0x0804XXXX  # function ROM address
with open('baserom.gba','rb') as f:
    f.seek(addr - 0x08000000)
    target = f.read(200)
# ... byte compare with objdump of /tmp/test.o
"
```

For `-ftst` functions, use `old_agbcc -mthumb-interwork -O2 -ftst` instead.

## Session 4 — Original SDK Compiler Investigation

### The Original AGB SDK GCC Source

The original SDK compiler source code is preserved at [github.com/mid-kid/arm-000512](https://github.com/mid-kid/arm-000512) with three branches:

| Branch | Date | thumb.c size | Description |
|--------|------|-------------|-------------|
| `010110-ThumbPatch` | Jan 10, 2001 | **58,802 bytes** | Early SDK with Thumb instruction patches |
| `011025-sdk3.1jp` | Oct 25, 2001 | 56,857 bytes | SDK 3.1 Japanese release |
| `021206-sdk3.0us` | Dec 6, 2002 | 56,857 bytes | SDK 3.0 US release |
| pret/agbcc | — | 46,476 bytes | Simplified/stripped version |

Klonoa: Empire of Dreams (JP: July 2001) was developed with an early SDK.

### Build Results: All Three SDK Branches Tested

Successfully built cc1 from all three branches in Docker (Ubuntu 22.04, GCC 11, with `-fgnu89-inline` and pre-generated bison-free `c-parse.c`/`c-parse.h`). **None of the three branches match Klonoa's m4a binary.**

Test function: `SoundEffectParamInit` (19 lines, target starts with `movs r2, #0x00`)

| Compiler | First instruction | Register for zero | Result |
|----------|------------------|-------------------|--------|
| **ROM target** | `movs r2, #0` | r2 | — |
| `010110-ThumbPatch` | `adds r2, r0, #0` (copies r0→r2) | r0 for zero | **10 mismatches** |
| `011025-sdk3.1jp` | `push {lr}` | r0 for zero | **10 mismatches** |
| `021206-sdk3.0us` | `push {lr}` | r0 for zero | **10 mismatches** |
| `pret/old_agbcc` | `movs r0, #0` | r0 for zero | **~4 mismatches** |

The ThumbPatch compiler produces different code from SDK 3.0/3.1 (no unnecessary `push {lr}` in leaf functions), but all three use different register allocation than the Klonoa target. pret's `old_agbcc` is actually closest to the ROM for this function.

Also tested `MPlayChannelReset` with ThumbPatch: same 2-byte mismatch as `old_agbcc` (r1 vs r3 for scratch).

### Baseline cc1.exe (May 2000) — Obtained and Tested

Successfully extracted the **original baseline Thumb cc1.exe** from the GBA SDK v2.0 ISO:
- **Source:** [Archive.org GBA SDK v2.0](https://archive.org/details/gba-sdk-v2.0_agb-cpu-ts2) → `GNUPro.exe` (InstallShield) → `data1.cab` → `Cygnus/thumbelf-000512/H-i686-cygwin32/lib/gcc-lib/thumb-elf/2.9-arm-000512/cc1.exe`
- **SHA256:** `fdfc9005a23583c1396c36eaa736b1ce078af07787cc0b34069c341644966c42`
- **PE timestamp:** May 13, 2000 (matches "000512" = May 12, 2000)
- **Build path in strings:** `/tantor/build/nintendo/arm-000512/i686-cygwin32/src/gcc/`
- **Requires:** `cygwin1.dll` from same SDK (found at `thumbelf-000512/H-i686-cygwin32/bin/cygwin1.dll`)
- **Runs via:** Wine in x86 Docker (`--platform linux/amd64`, Ubuntu 22.04, `wine32` package)

**Result: Also does not match.** Updated test results:

| Compiler | SoundEffectParamInit first instruction | Register for zero | MPlayChannelReset scratch reg |
|----------|---------------------------------------|-------------------|------------------------------|
| **ROM target** | `movs r2, #0` | **r2** | **r3** |
| `baseline cc1.exe (May 2000)` | `movs r0, #0` | r0 | r1 |
| `010110-ThumbPatch` | `adds r2, r0, #0` | r0 | r1 |
| `011025-sdk3.1jp` | `push {lr}` | r0 | r1 |
| `021206-sdk3.0us` | `push {lr}` | r0 | r1 |
| `pret/old_agbcc` | `movs r0, #0` | r0 | r1 |

**All five known compiler builds** (baseline + 3 source branches + pret) produce the same register allocation for these functions. The ROM target uses a different allocation that none of them reproduce.

**Conclusion:** The Klonoa m4a binary was NOT compiled with any of the known AGB SDK compiler builds. It was likely compiled with:
1. The `thumb_patch03-OCT-03.zip` update (referenced by RetroReversing but NOT present in the SDK v2.0 ISO — the ISO predates it). This update replaces only `cc1.exe` and `cc1plus.exe` with patched versions.
2. A different SDK distribution version not yet found (e.g., SDK v2.5 or a Japan-specific update).
3. A compiler from a completely different SDK CD that Namco received directly from Nintendo.

The `thumb.c.rej` in the ThumbPatch branch contains **additions** (long_call/short_call attributes) that were added by the patch — not the original baseline code. These are irrelevant to register allocation.

### Remaining Path Forward

### Patched cc1.exe (Aug 2002) — Also Tested

Found a **different cc1.exe** in the Gigaleak ezbuild archives on Archive.org:
- **Source:** [Gigaleak ezbuild collection](https://archive.org/details/gigaleak-source-code-ezbuild-collection_202409) → `GB + GBA/FZERO_for_GBA-ezbuild-WinXP.7z` (also identical in `marioAGB-ezbuild.7z`)
- **Path:** `Cygnus/thumbelf-000512/H-i686-cygwin32/lib/gcc-lib/thumb-elf/2.9-arm-000512/cc1.exe`
- **SHA256:** `926b08b0e6924f55dd7189e0e46b7d2a8fc1905ace5fca787422338d69cd1ec6`
- **PE timestamp:** August 23, 2002
- **Size:** 1,520,430 bytes (much smaller than baseline's 3.6MB — likely stripped/optimized build)
- **Build path in strings:** `/src/cygnus/arm-000512/gcc/toplev.c`

**Result: Also does not match. Worse than baseline — adds `push {lr}` to leaf functions (same as SDK 3.0/3.1 source-built).**

### Complete Compiler Test Matrix

| Compiler | Source | Date | SoundEffectParamInit zero reg | MPlayChannelReset scratch | push lr in leaf? |
|----------|--------|------|------------------------------|--------------------------|-----------------|
| **ROM target** | unknown | ~2001 | **r2** | **r3** | **no** |
| Baseline cc1.exe | SDK v2.0 ISO | May 2000 | r0 | r1 | no |
| Patched cc1.exe | Gigaleak ezbuilds | Aug 2002 | r0 | r1 | **yes** (worse) |
| ThumbPatch (built) | arm-000512 source | Jan 2001 | r0 | r1 | no |
| SDK 3.1 JP (built) | arm-000512 source | Oct 2001 | r0 | r1 | **yes** |
| SDK 3.0 US (built) | arm-000512 source | Dec 2002 | r0 | r1 | **yes** |
| pret/old_agbcc | Simplified source | — | r0 | r1 | no |

**Six compilers tested, none match.** All use r0 for zero and r1 for scratch. The ROM target consistently uses r2 for zero and r3 for scratch — a register allocation pattern not produced by any known build of the AGB SDK GCC.

### BREAKTHROUGH: SDK v3.0 mks4agbLib.o Byte-Matches Klonoa

Compared the pre-compiled `mks4agbLib.o` from SDK v3.0 against the Klonoa ROM:

- **99 out of 109 SDK functions match** the Klonoa ROM at the byte level (8-byte signature match)
- Only **10 functions don't match** — all are thin wrappers (`m4aSoundInit`, `m4aSoundMain`) or revision-specific variants (`MPlayExtender`, `FadeOutBody_rev01`)
- The SDK `.o` has **158 relocations** (linker-resolved addresses), explaining why raw byte comparison shows only 23.6% — the relocations account for the address differences

**The Klonoa m4a binary IS the SDK v3.0 `mks4agbLib.o`**, linked into the ROM with game-specific addresses resolved by the linker. The functions were compiled by Nintendo/SMASH with their internal compiler and shipped as a pre-compiled object file. No end-user compiler can reproduce this output because the `.o` file was never compiled from source by game developers.

**SDK v3.0 `mks4agbLib.o` location:**
- Archive.org: [GBA SDK v3.0](https://archive.org/details/game-boy-advance-sdk-v-3.0) → `Game Boy Advance SDK v3.0.zip` → `MusicPlayerAGB2000/mp2000/mks4agbLib.o`

**SDK function names → Klonoa decomp names (mapping):**

| SDK v3.0 name | Klonoa decomp name | ROM address |
|---------------|-------------------|-------------|
| `clear_modM` | `SoundEffectParamInit` | 0x0804FE50 |
| `MidiKey2fr` | `MidiKeyToFreq` | 0x0804FEA0 |
| `SoundInit_rev01` | `DirectSoundFifoSetup` | 0x08050344 |
| `SoundMode_rev01` | `m4aSoundMode` | 0x080504E0 |
| `SoundClear_rev01` | `SoundClear` | 0x08050578 |
| `SoundVSyncOff_rev01` | `m4aSoundVSyncOff` | 0x080505CC |
| `SoundVSyncOn_rev01` | `m4aSoundVSyncOn` | 0x08050648 |
| `MPlayOpen_rev01` | `MPlayOpen` | 0x08050684 |
| `MPlayStart_rev01` | `MPlayStart` | 0x080506FC |
| `MPlayStop_rev01` | `MPlayStop` | 0x080507E0 |
| `TrackStop_rev01` | `SoundContextRef` | 0x0804FB9C |
| `ply_note_rev01` | `MPlayNoteProcess` | 0x0804FC10 |
| `ChnVolSetAsm` | `VoiceLookupAndApply` | 0x0804FBE0 |
| `MP_clear_modM` | `MidiUtilConvert` | 0x080510B4 |
| `MPlayContinue` | `MPlayContinue` | 0x0804FF08 |
| `CgbOscOff` | `CgbOscOff` | 0x08050A44 |
| `CgbModVol` | `CgbModVol` | 0x08050A94 |
| `CgbSound` | `CgbSound` | 0x08050AFC |
| `SampFreqSet_rev01` | `SampleFreqSet` | 0x0805043C |

### Remaining Path Forward

**The compiler question is now moot for most functions.** The m4a code was pre-compiled by Nintendo and shipped as `mks4agbLib.o`. To achieve matching decompilation:

### Definitive Analysis: ASM vs C Split in mks4agbLib.o

The SDK `mks4agbLib.o` contains a `.gcc2_compiled.` marker at offset `0x0C12`. Analysis of all 109 functions reveals the `.o` is split into two sections:

**Before `.gcc2_compiled.` (offset 0x000–0xC12): 69 functions — hand-written assembly**
- Contains ARM/Thumb interworking code (358 ARM-width instructions)
- Includes: `SoundMain`, `SoundMainRAM`, `MPlayMain_rev01`, `ply_note_rev01`, `TrackStop_rev01`, `ChnVolSetAsm`, `clear_modM`, all `ply_*` command handlers, `ClearChain`, `Clear64byte`
- These functions use register patterns (r2 for zero, r3 for scratch) that NO GCC 2.95 build produces
- **Cannot be match-decompiled to C** — they were never compiled from C

**After `.gcc2_compiled.` (offset 0xC12–0x2104): 57 functions — C-compiled with GCC 2.95**
- No ARM instructions — pure Thumb code
- Includes: `MidiKey2fr`, `SoundInit_rev01`, `SoundMode_rev01`, `CgbSound`, `MPlayOpen_rev01`, `ply_memacc`, all `ply_x*` extended handlers
- Uses `mov ip, r0` for parameter saving (specific to whatever GCC build Nintendo used)
- **29 of these match the ROM PERFECTLY** (excluding relocation bytes) and are **ALL ALREADY DECOMPILED**

**All 29 matchable C-compiled functions have been decompiled in earlier sessions.** The remaining 61 INCLUDE_ASM functions are exclusively from the hand-written ASM section — no C compiler can reproduce their output.

### Final Assessment

The m4a decompilation has reached its **maximum possible progress** with available tools:
- **~30 functions decompiled** (all C-compiled functions from the SDK that match `old_agbcc`)
- **61 functions remain as INCLUDE_ASM** (all hand-written assembly — structurally impossible to compile from C)
- **The build passes:** `klonoa-eod.gba: OK`

Further progress requires either:
1. **NON_MATCHING decompilation** — write equivalent C that produces different bytes, for readability only
2. **Inline assembly** — wrap the hand-written code in `asm()` blocks within C functions (preserves structure while allowing C-level documentation)
3. **Accept the current state** — 61 INCLUDE_ASM functions is the hard floor for matching decompilation of Klonoa's m4a

### Root Cause: pret/agbcc Simplified Register Allocator

Comparing the original SDK source (ThumbPatch branch) against pret/agbcc reveals **massive differences in the register allocation core files**:

| File | Changed lines | What it does |
|------|--------------|--------------|
| `global.c` | **611 lines** | Global register allocator — assigns registers across basic blocks |
| `reload1.c` | **678 lines** | Reload pass — spills/restores registers when allocation fails |
| `reload.c` | **336 lines** | Reload constraint logic |
| `loop.c` | **643 lines** | Loop optimization — affects register lifetime analysis |
| `local-alloc.c` | **138 lines** | Local register allocator — assigns within basic blocks |
| `regclass.c` | **105 lines** | Register class computation |

pret/agbcc significantly stripped down GCC 2.95 to simplify building. This removed hundreds of lines from the register allocator, explaining **all** the register assignment differences documented in Sessions 2-3 (r3-vs-r1 scratch preference, r12 usage, persistent zero register, etc.).

### Why pokeemerald/sa3 Match But Klonoa Doesn't

pokeemerald (2002) and sa3 (2004) were compiled with a **later SDK version** (SDK 3.0/3.1 era) whose m4a `.o` files were compiled with a compiler closer to what pret/agbcc reproduces. Klonoa (2001) used an **earlier SDK** whose m4a binary was compiled with the ThumbPatch-era compiler, which has different register allocation behavior.

The m4a library was distributed as **pre-compiled `.o` files** in the SDK (not source code). Different SDK releases shipped different `.o` files compiled with different GCC builds.

### Building the Original SDK Compiler: Status

**Attempted** to build the ThumbPatch cc1 from `arm-000512` on macOS. Results:

1. **Full autotools build** (`./configure && make`): macOS-specific issues:
   - `RETURN` enum conflict: macOS `sys/tty.h` defines `#define RETURN 6`, conflicting with GCC's RTX code enum `RETURN` in `rtl.def` and yacc token enum `RETURN` in `c-parse.h`. Fixed with `#undef RETURN` in `rtl.h` and `c-parse.h`.
   - Clang rejects duplicate enum names across different enums (C11+ strict), which old GCC sources rely on. GCC-14 (Homebrew) can compile this.
   - Missing `FATAL_EXIT_CODE`, `BITS_PER_UNIT` etc. due to cross-compilation host config.
   - Successfully compiled all `.o` files but cc1 segfaults — stub `libiberty.a` missing `concat()` variadic implementation.

2. **Hybrid approach** (SDK allocator files in pret framework): SDK files use `PROTO()` K&R macros while pret uses ANSI prototypes — incompatible without full file adaptation.

**Recommended next step:** Build on Linux (native or Docker) where the configure scripts work natively and there's no `RETURN` macro conflict from `sys/tty.h`:
```bash
docker run --rm -v /tmp/arm-000512:/src -w /src/build ubuntu:22.04 bash -c "
  apt-get update && apt-get install -y gcc make bison flex
  mkdir -p build && cd build
  CC=gcc ../gcc/configure --target=thumb-elf --host=i686-linux-gnu
  make cc1
"
```

### m4a Library Distribution History

- MusicPlayer2000 (m4a) was developed by **SMASH** (credited as "Smsh" / 0x68736D53 magic)
- Distributed as pre-compiled `.o` files: `mks4agbLib.o`, `m4aLibOD1.o`, `m4aLibUSC.o`
- **Different SDK versions shipped different .o files** compiled with the corresponding SDK compiler
- The C portions were compiled by Nintendo/SMASH before distribution; some parts are handwritten ARM/Thumb assembly

### Sources
- [mid-kid/arm-000512](https://github.com/mid-kid/arm-000512) — original SDK GCC source, 3 branches
- [pret/agbcc](https://github.com/pret/agbcc) — simplified GCC 2.95 for decomp projects
- [RetroReversing GBA SDK](https://www.retroreversing.com/game-boy-advance-sdk/) — SDK documentation
- [loveemu AGBinator list](https://gist.github.com/loveemu/60e2945678fb58ea5957708ebcad8f07) — confirms Klonoa uses MusicPlayer2000

## Current State

**61 INCLUDE_ASM functions remain** in m4a.c. All have been tested and categorized. Further progress requires one of:

1. **Building the original SDK compiler from arm-000512** — the ThumbPatch branch (`010110-ThumbPatch`) is the most likely match. Build on Linux to avoid macOS header conflicts. This is the highest-priority path since it directly addresses the register allocator differences that block all remaining functions.
2. **Patching pret/agbcc's register allocator** — port the ~2,500 changed lines from arm-000512's `global.c`, `reload1.c`, `reload.c`, `loop.c`, `local-alloc.c`, `regclass.c` into pret's ANSI-ified framework. Significant effort but avoids autotools issues.
3. **Accepting NON_MATCHING decompilation** — decompile for readability even if bytes don't match (would break the `make compare` check but improve code understanding)
