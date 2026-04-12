# `-ftst` Cross-Project Evidence: `ply_fine` in pokeemerald

## Overview

This document demonstrates that the `-ftst` flag for `pret/agbcc` ([PR #90](https://github.com/pret/agbcc/pull/90)) enables decompilation of functions that were previously stuck as handwritten assembly. The evidence comes from **pokeemerald** — a project unrelated to the one that motivated the patch.

## The function: `ply_fine`

`ply_fine` is part of the m4a sound engine (the GBA's standard MP2000 audio library). It handles "fine" (end-of-track) events by marking all channels as stopped and clearing the track. Both pokeemerald and SA3 keep this function as handwritten assembly in their `m4a_1.s` / `m4a0.s` files because agbcc cannot reproduce the `tst` instruction it contains.

### Target assembly (from `pokeemerald/src/m4a_1.s`)

```asm
	thumb_func_start ply_fine
ply_fine:
	push {r4,r5,lr}
	adds r5, r1, 0
	ldr r4, [r5, 0x20]           @ r4 = track->chan
	cmp r4, 0
	beq ply_fine_done
ply_fine_loop:
	ldrb r1, [r4, 0x00]          @ r1 = chan->statusFlags
	movs r0, 0xC7                @ r0 = SOUND_CHANNEL_SF_ON
	tst r0, r1                   @ ← TEST: flags & 0xC7, result DISCARDED
	beq ply_fine_ok
	movs r0, 0x40                @ SOUND_CHANNEL_SF_STOP
	orrs r1, r0
	strb r1, [r4, 0x00]          @ chan->statusFlags |= 0x40
ply_fine_ok:
	adds r0, r4, 0
	bl RealClearChain
	ldr r4, [r4, 0x34]           @ chan = chan->nextChannelPointer
	cmp r4, 0
	bne ply_fine_loop
ply_fine_done:
	movs r0, 0
	strb r0, [r5, 0x00]          @ track->flags = 0
	pop {r4,r5}
	pop {r0}
	bx r0
```

The key instruction is `tst r0, r1` at offset `0x0E` — it tests `statusFlags & 0xC7` without clobbering `r0`. The standard agbcc emits `ands r0, r1; cmp r0, #0` instead (2 instructions, clobbers `r0`).

### Decompiled C code

```c
void ply_fine(u32 unused, struct MusicPlayerTrack *track)
{
    struct SoundChannel *chan;

    chan = track->chan;
    while (chan != 0)
    {
        u8 sf;
        sf = chan->statusFlags;
        if (sf & 0xC7)
        {
            chan->statusFlags = sf | 0x40;
        }
        RealClearChain(chan);
        chan = chan->nextChannelPointer;
    }
    track->flags = 0;
}
```

This is clean, straightforward C. The `if (sf & 0xC7)` test is a standard bitwise flag check — exactly the pattern where `tst` should be emitted.

## Compilation comparison

### Without `-ftst` (current `pret/agbcc`)

```
00000000 <ply_fine>:
   0: b530    push  {r4, r5, lr}
   2: 1c0d    adds  r5, r1, #0
   4: 6a2c    ldr   r4, [r5, #32]
   6: 2c00    cmp   r4, #0
   8: d00d    beq.n 26             ← WRONG offset (0d instead of 0c)
   a: 7821    ldrb  r1, [r4, #0]
   c: 20c7    movs  r0, #199
   e: 4008    ands  r0, r1         ← WRONG: ands (0x4008) instead of tst (0x4208)
  10: 2800    cmp   r0, #0         ← EXTRA instruction: redundant comparison
  12: d002    beq.n 1a
  14: 2040    movs  r0, #64
  16: 4301    orrs  r1, r0
  18: 7021    strb  r1, [r4, #0]
  1a: 1c20    adds  r0, r4, #0
  1c: f7ff fffe  bl  RealClearChain
  20: 6b64    ldr   r4, [r4, #52]
  22: 2c00    cmp   r4, #0
  24: d1f1    bne.n a              ← WRONG offset
  26: 2000    movs  r0, #0
  28: 7028    strb  r0, [r5, #0]
  2a: bc30    pop   {r4, r5}
  2c: bc01    pop   {r0}
  2e: 4700    bx    r0
```

**3 differences** from the target:
1. `ands r0, r1` (0x4008) instead of `tst r0, r1` (0x4208) — wrong instruction
2. Extra `cmp r0, #0` (0x2800) — redundant, shifts all subsequent offsets
3. Branch offsets shifted due to the extra instruction

### With `-ftst` (patched agbcc)

```
00000000 <ply_fine>:
   0: b530    push  {r4, r5, lr}
   2: 1c0d    adds  r5, r1, #0
   4: 6a2c    ldr   r4, [r5, #32]
   6: 2c00    cmp   r4, #0
   8: d00c    beq.n 24              ✓
   a: 7821    ldrb  r1, [r4, #0]
   c: 20c7    movs  r0, #199
   e: 4208    tst   r0, r1          ✓ CORRECT: tst (0x4208)
  10: d002    beq.n 18              ✓ (no extra cmp, offset correct)
  12: 2040    movs  r0, #64
  14: 4301    orrs  r1, r0
  16: 7021    strb  r1, [r4, #0]
  18: 1c20    adds  r0, r4, #0
  1a: f7ff fffe  bl  RealClearChain
  1e: 6b64    ldr   r4, [r4, #52]
  20: 2c00    cmp   r4, #0
  22: d1f2    bne.n a               ✓
  24: 2000    movs  r0, #0
  26: 7028    strb  r0, [r5, #0]
  28: bc30    pop   {r4, r5}
  2a: bc01    pop   {r0}
  2c: 4700    bx    r0
```

**0 differences** — byte-identical to the target assembly.

## Why this matters for pokeemerald and SA3

Both projects currently keep 6 functions (totaling ~36 `tst` instructions) as handwritten assembly because agbcc cannot emit `tst`:

| Function | `tst` count | Lines | pokeemerald file | SA3 file |
|----------|-------------|-------|-----------------|----------|
| `SoundMainRAM` | 15 | ~378 | `m4a_1.s` | `m4a0.s` |
| `MPlayMain` | 10 | ~340 | `m4a_1.s` | `m4a0.s` |
| `ply_note` | 10+ | ~350 | `m4a_1.s` | `m4a0.s` |
| `SoundMainRAM_Unk1` | 4 | ~199 | `m4a_1.s` | `m4a0.s` |
| **`ply_fine`** | **1** | **28** | **`m4a_1.s`** | **`m4a0.s`** |
| `TrackStop` | 1 | 39 | `m4a_1.s` | `m4a0.s` |

`ply_fine` is the simplest — with `-ftst`, it matches from straightforward C with zero workarounds. `TrackStop` and the larger functions may have additional blockers (non-standard prologues, `call_r3` trampolines), but `-ftst` removes the `tst`-related blockers for all of them.

## Reproducing

Run the shell script [`scripts/test-ftst-ply-fine.sh`](../scripts/test-ftst-ply-fine.sh) from any directory. It requires `arm-none-eabi-*` tools and a patched agbcc binary with `-ftst` support.

```bash
./scripts/test-ftst-ply-fine.sh /path/to/patched/old_agbcc
```

## References

- [pret/agbcc PR #90](https://github.com/pret/agbcc/pull/90) — `-ftst` flag implementation
- [pokeemerald `src/m4a_1.s` line 750](https://github.com/pret/pokeemerald/blob/master/src/m4a_1.s#L750) — `ply_fine` as handwritten assembly
- [SA3 `src/lib/m4a/m4a0.s`](https://github.com/SAT-R/sa3/blob/main/src/lib/m4a/m4a0.s) — same function, same `tst` pattern
- [pret/agbcc `gcc/thumb.h` line 36](https://github.com/pret/agbcc/blob/master/gcc/thumb.h#L36) — *"??? There is no pattern for the TST instuction"*
