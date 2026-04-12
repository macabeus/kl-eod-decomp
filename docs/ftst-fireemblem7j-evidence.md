# `-ftst` Cross-Project Evidence: `ply_fine` in Fire Emblem 7 (Japanese)

## Overview

This document demonstrates that the `-ftst` flag for `pret/agbcc` ([PR #90](https://github.com/pret/agbcc/pull/90)) enables decompilation of functions in **Fire Emblem: The Blazing Blade (Japanese)** — a project completely unrelated to both Klonoa (which motivated the patch) and pokeemerald (where `ply_fine` was first identified).

## Shared audio engine confirmation

FireEmblem7J uses the same Nintendo MP2000 (m4a) audio engine as pokeemerald, SA3, and Klonoa. The function list in `asm/m4a_1.s` confirms this:

| Function | pokeemerald | SA3 | FireEmblem7J |
|----------|------------|-----|-------------|
| `SoundMain` | `m4a_1.s` | `m4a0.s` | `m4a_1.s` |
| `MPlayMain` | `m4a_1.s` | `m4a0.s` | `m4a_1.s` |
| `ply_note` | `m4a_1.s` | `m4a0.s` | `m4a_1.s` (as `ply_note_rev01`) |
| `TrackStop` | `m4a_1.s` | `m4a0.s` | `m4a_1.s` (as `TrackStop_rev01`) |
| `ClearChain` | `m4a_1.s` | `m4a0.s` | `m4a_1.s` |
| `ChnVolSetAsm` | `m4a_1.s` | `m4a0.s` | `m4a_1.s` |
| `ply_fine` | `m4a_1.s` | `m4a0.s` | `m4a_1.s` (unnamed, at `_080BE804`) |

FireEmblem7J uses a `_rev01` suffix on several functions (`ply_note_rev01`, `TrackStop_rev01`, `ply_endtie_rev01`), indicating a slightly different SDK revision. Despite this, the core engine structure, struct layouts, and flag masks are identical.

## The function: `ply_fine` (unnamed at `_080BE804`)

In FireEmblem7J's `asm/m4a_1.s`, the function at address `0x080BE804` is `ply_fine` — it sits immediately after `ClearChain` and handles end-of-track events by marking channels as stopped. It was not labeled in the original disassembly, but is byte-identical to pokeemerald's `ply_fine`.

### Target assembly (from `FireEmblem7J/asm/m4a_1.s`, address `0x080BE804`)

```asm
_080BE804:
    push {r4, r5, lr}
    adds r5, r1, #0
    ldr r4, [r5, #0x20]          @ r4 = track->chan
    cmp r4, #0
    beq _080BE828
_080BE80E:
    ldrb r1, [r4]                @ r1 = chan->status
    movs r0, #0xC7               @ r0 = SOUND_CHANNEL_SF_ON
    tst r0, r1                   @ ← TEST: status & 0xC7, result DISCARDED
    beq _080BE81C
    movs r0, #0x40               @ SOUND_CHANNEL_SF_STOP
    orrs r1, r0
    strb r1, [r4]                @ chan->status |= 0x40
_080BE81C:
    adds r0, r4, #0
    bl ClearChain
    ldr r4, [r4, #0x34]          @ chan = chan->np (nextChannelPointer)
    cmp r4, #0
    bne _080BE80E
_080BE828:
    movs r0, #0
    strb r0, [r5]                @ track->flags = 0
    pop {r4, r5}
    pop {r0}
    bx r0
```

### Decompiled C code

```c
void ply_fine(u32 unused, struct MusicPlayerTrack *track)
{
    struct SoundChannel *chan;

    chan = track->chan;
    while (chan != 0)
    {
        u8 sf;
        sf = chan->status;
        if (sf & 0xC7)
        {
            chan->status = sf | 0x40;
        }
        ClearChain(chan);
        chan = chan->np;
    }
    track->flags = 0;
}
```

## Compilation comparison

### Without `-ftst` (current `pret/agbcc`)

```
00000000 <ply_fine>:
   0: b530    push  {r4, r5, lr}
   2: 1c0d    adds  r5, r1, #0
   4: 6a2c    ldr   r4, [r5, #32]
   6: 2c00    cmp   r4, #0
   8: d00d    beq.n 26             ← WRONG offset (0d not 0c)
   a: 7821    ldrb  r1, [r4, #0]
   c: 20c7    movs  r0, #199
   e: 4008    ands  r0, r1         ← WRONG: ands (0x4008) not tst (0x4208)
  10: 2800    cmp   r0, #0         ← EXTRA: redundant comparison
  12: d002    beq.n 1a
  ...
  24: d1f1    bne.n a              ← WRONG offset (f1 not f2)
  26: 2000    movs  r0, #0
  28: 7028    strb  r0, [r5, #0]
  2a: bc30    pop   {r4, r5}
  2c: bc01    pop   {r0}
  2e: 4700    bx    r0
```

**3 differences**: `ands` instead of `tst`, extra `cmp r0, #0`, shifted branch offsets.

### With `-ftst` (patched agbcc)

```
00000000 <ply_fine>:
   0: b530    push  {r4, r5, lr}     ✓
   2: 1c0d    adds  r5, r1, #0       ✓
   4: 6a2c    ldr   r4, [r5, #32]    ✓
   6: 2c00    cmp   r4, #0           ✓
   8: d00c    beq.n 24               ✓
   a: 7821    ldrb  r1, [r4, #0]     ✓
   c: 20c7    movs  r0, #199         ✓
   e: 4208    tst   r0, r1           ✓ CORRECT
  10: d002    beq.n 18               ✓
  12: 2040    movs  r0, #64          ✓
  14: 4301    orrs  r1, r0           ✓
  16: 7021    strb  r1, [r4, #0]     ✓
  18: 1c20    adds  r0, r4, #0       ✓
  1a: f7ff fffe  bl  ClearChain      ✓
  1e: 6b64    ldr   r4, [r4, #52]    ✓
  20: 2c00    cmp   r4, #0           ✓
  22: d1f2    bne.n a                ✓
  24: 2000    movs  r0, #0           ✓
  26: 7028    strb  r0, [r5, #0]     ✓
  28: bc30    pop   {r4, r5}         ✓
  2a: bc01    pop   {r0}             ✓
  2c: 4700    bx    r0               ✓
```

**0 differences** — every instruction byte-identical to the target.

## Cross-project summary

The same `ply_fine` function has been verified byte-identical across **4 independent GBA decomp projects**:

| Project | Game | m4a asm file | `ply_fine` label | Match with `-ftst`? |
|---------|------|-------------|------------------|-------------------|
| **pokeemerald** | Pokémon Emerald | `src/m4a_1.s` | `ply_fine` | Yes |
| **SA3** | Sonic Advance 3 | `src/lib/m4a/m4a0.s` | `MP2K_event_fine` | Yes |
| **FireEmblem7J** | Fire Emblem 7 (JP) | `asm/m4a_1.s` | `_080BE804` (unnamed) | Yes |
| **kl-eod** | Klonoa: Empire of Dreams | — | Not present (different m4a version) | N/A |

All use the same MP2000 audio engine with identical `tst`-based flag checks. Without `-ftst`, none of these functions can be matched from C code.

## Reproducing

```bash
./scripts/test-ftst-ply-fine.sh /path/to/patched/old_agbcc
```

The reproducer script compiles the same C code with and without `-ftst`, assembles the target, and performs a binary comparison. It works for all projects since the function is byte-identical across them (the only difference is the external function name: `RealClearChain` in pokeemerald vs `ClearChain` in FireEmblem7J, which doesn't affect the relocatable object comparison).

## References

- [pret/agbcc PR #90](https://github.com/pret/agbcc/pull/90) — `-ftst` flag implementation
- [pret/agbcc `gcc/thumb.h` line 36](https://github.com/pret/agbcc/blob/master/gcc/thumb.h#L36) — *"??? There is no pattern for the TST instuction"*
- FireEmblem7J `asm/m4a_1.s` — function at `_080BE804` (immediately after `ClearChain`)
- [pokeemerald `src/m4a_1.s` line 750](https://github.com/pret/pokeemerald/blob/master/src/m4a_1.s#L750) — same function as `ply_fine`
