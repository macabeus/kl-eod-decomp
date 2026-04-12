# The MusicPlayer2000 Mystery: Why Klonoa's Sound Engine Can't Be Fully Decompiled

## Context

Klonoa: Empire of Dreams (GBA, 2001) uses Nintendo's **MusicPlayer2000** (commonly called "m4a" or "Sappy") as its sound engine. In matching decompilation, the goal is to write C code that compiles to **byte-identical** machine code as the original ROM. This document explains why roughly 60% of the m4a functions in Klonoa cannot be match-decompiled — and how we identified the three separate compilers used to build the original library.

The game was developed by **Now Production** and published by Namco, with the JP release on July 19, 2001 and the US release on August 30, 2001 — just months after the GBA launched.

## The Starting Point: A Compiler Mismatch

GBA decompilation projects like [pokeemerald](https://github.com/pret/pokeemerald) and [Sonic Advance 3](https://github.com/SAT-R/sa3) compile their m4a code using [`old_agbcc`](https://github.com/pret/agbcc), a rebuild of the GCC 2.95 that shipped with the GBA SDK. For those games, it works — the compiled output matches the ROM byte-for-byte.

For Klonoa, `old_agbcc` matches some functions (simple setters like `ply_keysh`, wrapper functions like `m4aSoundMode`) but fails on the majority. The mismatches are consistent: the compiler assigns registers differently than the ROM target. For example, where the ROM uses `r2` for a zero constant, `old_agbcc` uses `r0`. Where the ROM uses `r3` as a scratch register, `old_agbcc` uses `r1`.

We tested **seven different GCC builds** — none reproduced the ROM's register allocation:

| Compiler | Source | Date |
|----------|--------|------|
| `old_agbcc` | [pret/agbcc](https://github.com/pret/agbcc) (simplified GCC 2.95) | — |
| `agbcc` | pret/agbcc (with `-fprologue-bugfix`) | — |
| Baseline cc1.exe | GBA SDK v2.0 ISO ([Archive.org](https://archive.org/details/gba-sdk-v2.0_agb-cpu-ts2)) | May 2000 |
| ThumbPatch cc1 | Built from [mid-kid/arm-000512](https://github.com/mid-kid/arm-000512) `010110-ThumbPatch` branch | Jan 2001 |
| SDK 3.1 JP cc1 | Built from arm-000512 `011025-sdk3.1jp` branch | Oct 2001 |
| SDK 3.0 US cc1 | Built from arm-000512 `021206-sdk3.0us` branch | Dec 2002 |
| Patched cc1.exe | [Gigaleak ezbuild collection](https://archive.org/details/gigaleak-source-code-ezbuild-collection_202409) | Aug 2002 |

## Finding the Original Library

The m4a engine wasn't compiled by game developers. Nintendo distributed it as a **pre-compiled object file** (`mks4agbLib.o`) inside the GBA SDK's MusicPlayerAGB2000 package. Game developers simply linked this `.o` file into their ROM.

We found this file inside the [GBA SDK v3.0 on Archive.org](https://archive.org/details/game-boy-advance-sdk-v-3.0), at path:

```
MusicPlayerAGB2000/mp2000/mks4agbLib.o
```

Searching for 8-byte function signatures from this `.o` file in the Klonoa ROM confirmed the match: **99 out of 109 SDK functions appear in the ROM** at the byte level (the remaining 10 are version-specific wrappers that Klonoa doesn't use).

## The Three Compilers Inside mks4agbLib.o

Disassembling the SDK's `mks4agbLib.o` reveals a GCC marker symbol (`.gcc2_compiled.`) at offset `0x0C12`. This marker divides the library into two sections — but the first section was itself compiled with a **non-GCC compiler**, giving us three distinct compilers total.

### Compiler 1: ARM Norcroft `tcc` (before `.gcc2_compiled.`)

The first section (offsets `0x000`–`0xC12`, 69 functions) was compiled with **ARM's proprietary Norcroft Thumb C Compiler (`tcc`)** — a completely different compiler from GCC, [developed at Cambridge University](https://handwiki.org/wiki/Software:Norcroft_C_compiler) and distributed in the [ARM SDT](https://3dodev.com/software/sdks).

Evidence:
- **No `.gcc2_compiled.` marker** — tcc is not GCC and does not emit this symbol.
- **Graph-colouring register allocation** — produces different register assignments than GCC's linear-scan allocator (e.g., `r2` for zero instead of `r0`).
- **Byte-identical match confirmed** — `clear_modM` compiled with `tcc -O0 -apcs /interwork` produces the exact same bytes as the ROM (see "Breakthrough" section below).
- **Mixed optimization levels** — some functions match at `-O0`, others at `-O1`.

The tcc binary is available at [trapexit/3do-devkit](https://github.com/trapexit/3do-devkit) on GitHub (`bin/compiler/linux/tcc`) or from [3dodev.com](https://3dodev.com/software/sdks) (ARM SDT 2.50/2.51).

However, not all functions in this section are compilable from C. Several contain:
- **ARM/Thumb interworking** — 32-bit ARM instructions inline within Thumb code (e.g., `SoundMainRAM`)
- **Non-standard ABI** — `ChnVolSetAsm` uses `r4`/`r5` without pushing them (implicit parameters from caller)
- **`mov ip, lr` / `bx ip` pattern** — manual link register save that no C compiler generates
- **Fall-through between functions** — one function flows directly into the next without returning

These patterns suggest the section was assembled from a `.s` file that mixed compiler-generated C output with hand-written assembly routines. The struct offset symbols (`o_MPlayTrk_vol`, `o_SA_reverb`, etc.) are `.equ` constants from shared headers used by both the C and assembly code.

### Compiler 2: GCC 2.95 / `old_agbcc` (after `.gcc2_compiled.`)

The second section (offsets `0xC12`–`0x2104`, 57 functions) was compiled with **GCC 2.95** — the same compiler that `old_agbcc` reproduces. These functions have standard GCC prologues/epilogues, all-global symbols, and no ARM-mode instructions. **29 of these 57 functions match the ROM perfectly** and are already decompiled in the project.

### Compiler 3: GCC 2.95 with `-ftst` flag

A small number of functions use the `tst` instruction (Thumb test-and-branch) instead of the `ands` + `cmp` sequence. These were compiled with GCC using the `-ftst` flag. In the project, these go in `src/m4a_1.c`, compiled with `old_agbcc -mthumb-interwork -O2 -ftst`.

## The Breakthrough: ARM's `tcc` Matches the ROM

### `clear_modM` (SoundEffectParamInit): tcc -O0 — PERFECT MATCH

Testing ARM's `tcc` at optimization level `-O0` with `-apcs /interwork` produces **byte-identical code** for `clear_modM`:

```
ROM (0x0804FE50):                tcc -O0 output:
2200  movs r2, #0                2200  movs r2, #0           ✓
758a  strb r2, [r1, #22]         758a  strb r2, [r1, #22]    ✓
768a  strb r2, [r1, #26]         768a  strb r2, [r1, #26]    ✓
7e0a  ldrb r2, [r1, #24]         7e0a  ldrb r2, [r1, #24]    ✓
2a00  cmp r2, #0                 2a00  cmp r2, #0             ✓
d101  bne +2                     d101  bne +2                 ✓
220c  movs r2, #12               220c  movs r2, #12           ✓
e000  b +2                       e000  b +2                   ✓
2203  movs r2, #3                2203  movs r2, #3            ✓
780b  ldrb r3, [r1, #0]          780b  ldrb r3, [r1, #0]      ✓
4313  orrs r3, r2                4313  orrs r3, r2             ✓
700b  strb r3, [r1, #0]          700b  strb r3, [r1, #0]      ✓
4770  bx lr                      4770  bx lr                   ✓
```

**13/13 code halfwords match perfectly.** The only difference is the padding byte after `bx lr` (assembler alignment, not compiler output).

Why `-O0` works: at `-O0`, `tcc` preserves all parameter registers (r0=chan, r1=track) and allocates local variables starting from r2. At `-O2`, `tcc` notices `chan` is unused and reuses r0, shifting the allocation to r0/r2 instead of r2/r3.

### `ClearChain` (VoiceGetParams): tcc -O1 — PERFECT MATCH

A second function, `ClearChain`, matches with `tcc -O1`:

```
ROM (0x0804F6D4):                tcc -O1 output:
6ac3  ldr r3, [r0, #44]         6ac3  ldr r3, [r0, #44]      ✓
2b00  cmp r3, #0                2b00  cmp r3, #0              ✓
d00b  beq +22                   d00b  beq +22                 ✓
6b41  ldr r1, [r0, #52]         6b41  ldr r1, [r0, #52]      ✓
6b02  ldr r2, [r0, #48]         6b02  ldr r2, [r0, #48]      ✓
2a00  cmp r2, #0                2a00  cmp r2, #0              ✓
d001  beq +2                    d001  beq +2                  ✓
6351  str r1, [r2, #52]         6351  str r1, [r2, #52]       ✓
e000  b +2                      e000  b +2                    ✓
6219  str r1, [r3, #32]         6219  str r1, [r3, #32]       ✓
2900  cmp r1, #0                2900  cmp r1, #0              ✓
d000  beq +0                    d000  beq +0                  ✓
630a  str r2, [r1, #48]         630a  str r2, [r1, #48]       ✓
2100  movs r1, #0               2100  movs r1, #0             ✓
62c1  str r1, [r0, #44]         62c1  str r1, [r0, #44]       ✓
4770  bx lr                     4770  bx lr                    ✓
```

**16/16 halfwords match perfectly.** At `-O1`, `tcc` reuses the dead parameter register `r1` for the zero literal (instead of pushing r7 to get a free register as `-O0` would do). This produces tighter code matching the ROM exactly.

### Mixed optimization levels

The SDK library was compiled with **both `-O0` and `-O1`** for different functions:
- **`-O0`**: For functions with unused parameters where register preservation is needed (e.g., `clear_modM` has an unused `chan` parameter)
- **`-O1`**: For functions where dead register reuse produces correct allocation (e.g., `ClearChain` is a 1-parameter function)

## Build System Integration

The project uses four compilation units for m4a, each with a different compiler:

| File | Compiler | Flags | Purpose |
|------|----------|-------|---------|
| `src/m4a.c` | `old_agbcc` (GCC 2.95) | `-mthumb-interwork -O2` | Main m4a compilation unit |
| `src/m4a_1.c` | `old_agbcc` (GCC 2.95) | `-mthumb-interwork -O2 -ftst` | Functions using `tst` instruction |
| `src/m4a_2.c` | ARM `tcc` (Norcroft) | `-O0 -apcs /interwork` | tcc-compiled functions at -O0 |
| `src/m4a_3.c` | ARM `tcc` (Norcroft) | `-O1 -apcs /interwork` | tcc-compiled functions at -O1 |

Each tcc compilation unit is pre-compiled to assembly, converted from ARM assembler syntax to GAS syntax (via `tools/tcc/tcc2gas.py`), and included into `m4a.c` via `asm(".include ...")` to keep all functions in the same `.text` section.

**Platform requirements:**
- **Linux x86_64 (CI)**: tcc runs natively with `libc6:i386` (32-bit compatibility)
- **macOS**: tcc requires Wine (`brew install wine-stable`)

## The Complete Function Map

### Decompiled (41 functions)

| Function | Compiler | Notes |
|----------|----------|-------|
| `ply_keysh` | old_agbcc | Track parameter setter |
| `ply_voice` | old_agbcc | Track parameter setter |
| `ply_vol` | old_agbcc | Track parameter setter |
| `ply_pan` | old_agbcc | Track parameter setter |
| `ply_bend` | old_agbcc | Track parameter setter |
| `ply_bendr` | old_agbcc | Track parameter setter |
| `ply_lfos` | old_agbcc -ftst | Track parameter setter (uses tst) |
| `ply_lfodl` | old_agbcc | Track parameter setter |
| `ply_mod` | old_agbcc | Track parameter setter |
| `m4aSoundMode` | old_agbcc | Sound mode configuration |
| `SoundClear` | old_agbcc | Channel status reset |
| `m4aSoundVSyncOff` | old_agbcc | DMA shutdown |
| `m4aSoundVSyncOn` | old_agbcc | VSync sound enable |
| `MPlayOpen` | old_agbcc | Music player init |
| `MPlayStop` | old_agbcc | Stop music player |
| `FUN_08050a44` | old_agbcc | CGB oscillator off |
| `MidiCommandEncode1` | old_agbcc | MIDI command encoder |
| `MidiCommandEncode2` | old_agbcc | MIDI command encoder |
| `SoundCmd_Dispatch` | old_agbcc | Sound command dispatch |
| `SoundCommand_6450` | old_agbcc | Sound command handler |
| `m4aSoundInit` | old_agbcc | Sound engine init (game-specific) |
| `m4aSongNumStart` | old_agbcc | Start song by number |
| `m4aSongNumStop` | old_agbcc | Stop song by number |
| `m4aMPlayAllStop` | old_agbcc | Stop all music players |
| `m4aMPlayCommand` | old_agbcc | Music player command |
| `MPlayMain_SetAndProcess` | old_agbcc | MIDI process wrapper |
| `SoundHardwareInit_Tail` | old_agbcc | SWI call wrapper |
| `PlaySoundWithContext_D8` | old_agbcc | Sound effect trigger |
| `PlaySoundWithContext_DC` | old_agbcc | Sound effect trigger |
| `FreeSoundStruct` | old_agbcc | Memory free wrapper |
| `SoundInfoInit` | old_agbcc | Sound info init (game-specific) |
| `StreamCmd_StopSoundAndSync` | old_agbcc | Stream command (game-specific) |
| `StreamCmd_SetSoundFreqs` | old_agbcc | Stream command (game-specific) |
| `StreamCmd_AdvanceStream` | old_agbcc | Stream pointer advance |
| `SoundInit` | old_agbcc | Wrapper for m4aSoundInit |
| `StopSoundChannel` | old_agbcc | Stop individual channel |
| `StopSoundEffects` | old_agbcc | Stop all sound effects |
| `DirectSoundFifoSetup` | old_agbcc | Hardware initialization |
| `VoiceLookupAndApply` | old_agbcc -ftst | Voice parameter application |
| **`SoundEffectParamInit`** | **tcc -O0** | **Clear modulation state** |
| **`VoiceGetParams`** | **tcc -O1** | **Unlink voice from chain** |

### Remaining INCLUDE_ASM (58 functions) — Why Each Can't Be Decompiled

#### Non-word-aligned functions (22)

C compilers always emit `.align 2` before functions. These functions start at odd halfword boundaries because they follow non-aligned predecessors:

`StreamCmd_GetStreamPtr`, `StreamCmd_ValidateStream`, `SoundBufferAlloc`, `StreamCmd_GetSoundReverb`, `SoundContextInit`, `StreamCmd_StopAndAdvance`, `StreamCmd_ReadFreqParam`, `StreamCmd_SetChannelMode`, `MPlayMain`, `TrackStop`, `MidiNoteDispatch`, `MidiNoteSetup`, `MidiNoteWithVelocity`, `MPlayChannelRelease`, `m4aSongNumContinue`, `m4aSongNumLoad`, `SappyStateVerify`, `MPlayTrackVerify`, `SoundEffectTrigger`, `MPlayStateCheck1`, `MidiKeyToCgbFreq`, `FUN_0804f75a`

#### Register allocation mismatches (10)

The ROM was compiled with a GCC version that makes different register choices than `old_agbcc`. These produce functionally equivalent but not byte-identical code:

| Function | Issue |
|----------|-------|
| `SoundReset` | 4 instructions reordered (instruction scheduling) |
| `MPlayStateCheck` | ROM saves r9 unnecessarily; body matches 82% |
| `MidiNoteLookup` | Same r9 issue; body matches 91% |
| `MPlayStateCheck3` | ROM saves r10; register allocation throughout differs |
| `MidiKeyToFreq` | ROM uses `mov r12, r0` (no compiler generates this) |
| `MidiProcessEvent` | Literal pool placement and register assignment differ |
| `SoundDmaInit` | 4-param function, extensive register pressure differences |
| `DmaControllerInit` | Different loop code generation patterns |
| `MPlayStart` | Complex guard conditions cause allocation divergence |
| `CgbModVol` | ROM loads `0xFFFF` from literal pool; GCC uses `lsl/lsr` pair |

#### Structural blockers (13)

| Function | Issue |
|----------|-------|
| `SoundMain` | 248 lines, ARM/Thumb interworking |
| `InitSoundEngine` | 549 lines, ARM/Thumb interworking, inline ARM code |
| `FixedPointMultiply` | ARM-mode multiply routine |
| `MPlayContinue` | 327 lines, non-aligned, tst, embedded `SoundChannelRelease` thunk |
| `MPlayNoteProcess` | 281 lines, embedded `FUN_0804fca0` label, `tst` (pokeemerald also keeps this as asm) |
| `SoundMixerMain` | 393 lines, ARM-mode PCM mixing |
| `CgbChannelMix` | Two halves of one function with SoundMixerMain |
| `MidiCommandHandler` | Not one function — ~13 separate command handlers with jump table |
| `SoundContextRef` | Persistent zero register (`movs r6, #0`), `tst` |
| `MPlayStop_Channel` | Non-standard ABI (r4/r5 implicit params from caller) |
| `SoundEffectChain` | Non-standard ABI, `mov r12, lr` / `bx r12` |
| `StreamCmd_SetSoundReverb` | Shares literal pool with `StreamCmd_GetSoundReverb` |
| `MPlayChannelReset` | Contains embedded `FUN_0804ff2a` sub-function |

#### Other (13)

Remaining functions blocked by: shared literal pools, `.2byte`-encoded branches, embedded sub-functions, `MPlayTrackCallback` (non-standard ABI with implicit r2/r6), `InstrumentGetEntry`/`MidiDecodeByte` (fall-through to next function), `SoundHardwareInit` (166-byte version difference from SDK), `MPlayTrackFadeAndVerify` (non-aligned + encoded branches).

## How to Reproduce

### Verify the SDK library match

```bash
# 1. Download GBA SDK v3.0 from Archive.org
#    https://archive.org/details/game-boy-advance-sdk-v-3.0
#    Extract: MusicPlayerAGB2000/mp2000/mks4agbLib.o

# 2. Check the .gcc2_compiled. marker (tcc/GCC boundary)
arm-none-eabi-nm -n mks4agbLib.o | grep gcc2_compiled
# Output: 00000c12 t .gcc2_compiled.

# 3. Search for SDK function signatures in the ROM
python3 -c "
import struct
sig = bytes([0x79, 0x43, 0x2b, 0x00, 0xd0, 0x2c, 0xa1, 0x01])
with open('baserom.gba', 'rb') as f:
    rom = f.read()
pos = rom.find(sig)
if pos >= 0:
    print(f'SoundMainRAM found at ROM 0x{0x08000000+pos:08X}')
"
```

### Verify the tcc match

```bash
# 1. Get tcc from the 3do-devkit repo
git clone --depth 1 https://github.com/trapexit/3do-devkit.git /tmp/3do-devkit

# 2. Write the test function
cat > /tmp/test.c << 'EOF'
typedef unsigned int u32;
typedef unsigned char u8;
void SoundEffectParamInit(u32 unused, u8 *track) {
    u8 orVal;
    track[0x16] = 0;
    track[0x1A] = 0;
    if (track[0x18] == 0) {
        orVal = 0x0C;
    } else {
        orVal = 0x03;
    }
    track[0x00] |= orVal;
}
EOF

# 3. Compile with tcc -O0 (requires Linux x86 or Docker)
docker run --rm --platform linux/amd64 \
  -v /tmp/3do-devkit:/devkit -v $(pwd):/repo \
  ubuntu:22.04 bash -c '
dpkg --add-architecture i386
apt-get update -qq && apt-get install -y -qq libc6:i386 binutils-arm-none-eabi >/dev/null 2>&1
chmod +x /devkit/bin/compiler/linux/tcc
/devkit/bin/compiler/linux/tcc -O0 -apcs /interwork -S -o /tmp/out.s /tmp/test.c
cat /tmp/out.s
'

# tcc output will show: MOV r2,#0 / STRB r2,[r1,#&16] / ...
# confirming r2 for zero (matching the ROM), not r0 (what GCC produces)
```

## Summary

The m4a sound engine in Klonoa: Empire of Dreams consists of **99 functions**. Of these:

- **41 are decompiled** (~41%) to byte-identical C code
- **58 remain as INCLUDE_ASM** (~59%) due to structural blockers

The library (`mks4agbLib.o`) was built using **three compilers**:

1. **ARM Norcroft `tcc`** at `-O0` and `-O1` — for the first section (before `.gcc2_compiled.`)
2. **GCC 2.95** (`old_agbcc`) at `-O2` — for the second section (after `.gcc2_compiled.`)
3. **GCC 2.95** with `-ftst` — for a small number of functions using the `tst` instruction

The remaining 58 functions cannot be match-decompiled because they contain ARM/Thumb interworking code, non-standard calling conventions, non-word-aligned entry points, shared literal pools, embedded sub-functions, or register allocation patterns that no available compiler reproduces exactly.

### Sources

- [GBA SDK v3.0 (mks4agbLib.o)](https://archive.org/details/game-boy-advance-sdk-v-3.0) — Archive.org
- [ARM SDT 2.51 (tcc compiler)](https://3dodev.com/software/sdks) — 3dodev.com
- [trapexit/3do-devkit (tcc Linux binary)](https://github.com/trapexit/3do-devkit) — GitHub
- [mid-kid/arm-000512 (SDK GCC source)](https://github.com/mid-kid/arm-000512) — GitHub
- [pret/agbcc (old_agbcc/agbcc)](https://github.com/pret/agbcc) — GitHub
- [GBA SDK v2.0 ISO (baseline cc1.exe)](https://archive.org/details/gba-sdk-v2.0_agb-cpu-ts2) — Archive.org
- [Gigaleak ezbuild collection (patched cc1.exe)](https://archive.org/details/gigaleak-source-code-ezbuild-collection_202409) — Archive.org
- [Norcroft C compiler history](https://handwiki.org/wiki/Software:Norcroft_C_compiler) — HandWiki
