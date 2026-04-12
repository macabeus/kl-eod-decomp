# old_agbcc Code Generation Patterns for m4a Matching

## Context

The MusicPlayer2000 (m4a) sound engine in Klonoa was compiled with a different GCC build than the game code. The project uses `old_agbcc` (`tools/agbcc/bin/old_agbcc -mthumb-interwork -O2`) for `src/m4a.c`, while the rest of the game uses the custom `agbcc` with `-fhex-asm -fprologue-bugfix`.

Both pokeemerald and sa3 also compile m4a with `old_agbcc`. However, Klonoa's m4a binary exhibits some code generation patterns that neither `agbcc` nor `old_agbcc` reproduces exactly. This document catalogs what `old_agbcc` DOES produce and techniques for working with it.

## Register Allocation Rules

### Parameter copy register

When a function copies its first parameter to free r0:
- **1-param function** `f(u32 *p)`: `old_agbcc` generates `adds r2, r0, #0` — copies to **r2**
- **2-param function** `f(u32 a, u32 *p)`: the second param is already in r1, no copy needed

If the target asm shows `adds r1, r0, #0` (copy to r1), the function likely takes 1 param and the original compiler chose r1. `old_agbcc` always chooses r2 for 1-param copies. In such cases, try a 2-param signature: `void func(u32 unused, u8 *actual)` — this places the real argument in r1 directly.

### Scratch register preference

After copying the parameter, `old_agbcc` allocates scratch temporaries in order: **r0, r1, r2, r3** (whichever are free). The Klonoa m4a target sometimes uses r3 before r1 for scratch. No C-level construct changes this order — it's determined by GCC 2.95's register allocator internals.

### Callee-saved registers

`old_agbcc` pushes callee-saved registers (r4-r7) only when they're assigned by the register allocator. It will NOT push r6 just to use it as a persistent zero register — even though the Klonoa target does this for ~10 functions. No compiler flag or C trick can force `old_agbcc` to push an unused callee-saved register.

## Constant Loading

### Small constants (0-255)

`old_agbcc` always uses `movs rN, #value` for constants that fit in 8 bits. It will NOT load them from the literal pool via `ldr rN, =value`.

**Workaround**: Use an extern linker symbol. Define `gMyConst = value;` in `ldscript.txt` and `extern char gMyConst[];` in C. Then `(u16)gMyConst` forces a literal pool load.

Example:
```c
// old_agbcc generates: movs r0, #4
u16 count = 4;

// old_agbcc generates: ldr r0, =gNumMusicPlayers (literal pool load)
extern char gNumMusicPlayers[];
u16 count = (u16)gNumMusicPlayers;  // linker resolves to 4
```

This trick was used for `m4aMPlayAllStop` and `StopSoundEffects`.

### Large constants

Constants > 255 are always loaded from the literal pool: `ldr rN, =value`. This matches the target.

## Zero Stores

`old_agbcc` generates `movs rN, #0` inline at each zero store. It does NOT cache zero in a dedicated register. For functions that use zero in multiple places:
```c
// old_agbcc generates:
//   mov r0, #0
//   strb r0, [r4]       ← inline zero
//   mov r0, #0
//   str r0, [r4, #44]   ← inline zero again
```

The Klonoa target often generates:
```asm
movs r6, #0
strb r6, [r4]         ; ← reuses r6
str r6, [r4, #0x2C]   ; ← reuses r6
```

There is no C-level workaround for this. The 10 functions with this pattern (SoundContextRef, MidiKeyToFreq, SampleFreqSet, MPlayStart, SoundEffectTrigger, MPlayContinue, MPlayNoteProcess, DmaControllerInit, CgbChannelMix, InitSoundEngine) cannot be matched with `old_agbcc`.

## u16 Truncation

`old_agbcc` truncates u16 values using a shift pair:
```asm
lsls r0, r0, #16
lsrs r0, r0, #16
```

The Klonoa target sometimes uses a mask from the literal pool:
```asm
ldr r3, =0xFFFF
ands r0, r3
```

Both produce the same result but different bytes. `old_agbcc` always uses the shift pair for `u16` casts. There is no C-level workaround.

## Dead Code

`old_agbcc` at `-O2` eliminates dead stores. For example:
```c
u8 zero = 0;  // dead if overwritten before use
```
The compiler removes the `movs r2, #0` if r2 is overwritten before being read.

The Klonoa target preserves some dead stores (e.g., MidiUtilConvert has a dead `movs r2, #0`). This is a compiler difference that cannot be reproduced.

## Volatile for Matching

When the target has a "dead" load that `old_agbcc` would optimize away, use `volatile`:
```c
// Forces the compiler to emit the ldrb even though the result is overwritten
u8 scratch = ((vu8 *)info)[0x04];
scratch = 0;
((u8 *)info)[0x04] = scratch;
```

This was used in `m4aSoundVSyncOn`.

## Function Signature Tricks

### 2-param signature to keep param in r1

When the target asm shows the main pointer in r1 (not r0):
```asm
adds r5, r1, #0x0    ; r1 is the real param, r0 is unused
```

Use a 2-param signature:
```c
void SoundContextRef(u32 unused, u32 *info) { ... }
```

### Return type to keep param in r0

When the target keeps the first param in r0 throughout:
```c
u32 *func(u32 *info) { ...; return info; }
```
The `return` prevents the compiler from moving `info` out of r0.

## Standalone Testing Workflow

Always test C code in isolation BEFORE putting it in m4a.c:

```bash
# 1. Write test file
cat > /tmp/test.c << 'EOF'
typedef unsigned char u8;
typedef unsigned int u32;
void MyFunc(u32 *info) { ... }
EOF

# 2. Compile with old_agbcc
tools/agbcc/bin/old_agbcc -mthumb-interwork -O2 -o /tmp/test.s /tmp/test.c

# 3. Assemble
arm-none-eabi-as -mcpu=arm7tdmi -mthumb-interwork -o /tmp/test.o /tmp/test.s

# 4. Compare bytes
arm-none-eabi-objdump -d /tmp/test.o

# 5. Compare with target ROM bytes
python3 -c "
import struct
with open('baserom.gba', 'rb') as f:
    f.seek(TARGET_ADDR - 0x08000000)
    for i in range(NUM_HALFWORDS):
        hw = struct.unpack('<H', f.read(2))[0]
        print(f'  {TARGET_ADDR + i*2:08x}: {hw:04x}')
"
```

Only after ALL bytes match should the function be placed in m4a.c and verified with `make`.

## Non-word-aligned Functions

26 m4a functions are `non_word_aligned_thumb_func_start` — they start at addresses not divisible by 4. The C compiler always word-aligns functions (`.align 2, 0`), so these cannot be decompiled individually. They can only be decompiled if their preceding function is ALSO decompiled as C, removing the alignment gap.

## The r0-copy Problem (Most Common Blocker)

The single biggest obstacle: `old_agbcc` always copies the first parameter (r0) to r2 when it needs r0 for other operations (like `movs r0, #0`). The Klonoa m4a target frequently keeps the parameter in r0 and uses a different register (r1) for the zero constant.

```
Target:                          old_agbcc:
  ldr r3, [r0, #0x2C]             adds r2, r0, #0      ← copies to r2!
  ...                              ldr r3, [r2, #0x2C]  ← uses r2
  movs r1, #0                     ...
  str r1, [r0, #0x2C]             movs r0, #0           ← reuses r0!
                                   str r0, [r2, #0x2C]
```

This affects **nearly every non-trivial m4a function**. No C-level construct, variable declaration order, or optimization flag can change this allocation in `old_agbcc`. Functions that match are those where the target ALSO copies r0 to r2 — which happens when the original SDK compiler made the same choice.

### Functions that match despite this

Functions where old_agbcc's r0→r2 copy matches the target:
- `MPlayChannelReset`: target also copies r0→r2 (but uses r3 for scratch vs old_agbcc's r1)
- Simple getters/setters (`ply_*`): no copy needed (leaf functions with no zero stores)
- 2-param functions where the real param is in r1: no r0 copy needed

### Non-standard ABI (r4 as implicit parameter)

Some functions receive values in callee-saved registers (r4, r5) from the caller, bypassing the standard ABI. Example: `MPlayCmd_ReadU32Param` uses r4 as an accumulator value set by the caller. These CAN'T be expressed in C without register pins.

## Structural Blockers Reference

| Blocker | Count | Description |
|---------|-------|-------------|
| `non_word_aligned` | 26 | C compiler adds alignment padding |
| `r6_zero` (persistent zero) | 10 | old_agbcc doesn't cache zero in r6 |
| `TRUE_fallthrough` | 8 | Function falls through to next (no return) |
| `r0-copy mismatch` | ~30 | old_agbcc copies r0→r2 but target keeps r0 |
| `r4/r5 implicit args` | ~3 | Non-standard ABI using callee-saved registers |
| `embedded` sub-functions | 2 | Multiple functions in one asm file |
| `shared_pool_start` | 1 | Literal pool from previous function |
| ARM code | 3 | Thumb-only compiler can't emit ARM |
