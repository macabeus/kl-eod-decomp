# EntityDeathAnimation Decompilation: Findings and Learnings

## Overview

`EntityDeathAnimation` (ROM address `0x0801DE44`) is a 384-byte Thumb function from *Klonoa: Empire of Dreams* (GBA) that manages the multi-frame death/destruction animation for entities such as the Moo enemy. It was successfully decompiled to produce a binary-identical match using agbcc with `-mthumb-interwork -Wimplicit -Wparentheses -O2 -fhex-asm -fprologue-bugfix -fno-fold-addr`.

The function was initially attempted by Mizuchi's Claude Runner (12 attempts) and two decomp-permuter background tasks. Both permuter runs converged to a score of 650 (from base scores of 1690 and 18100). The final matching C code was derived manually from the permuter output by fixing register allocation issues with `asm` barriers and `register ... asm("rN")` constraints.

## Entity Struct Layout

The entity system uses a flat array at `0x03002920` (IWRAM) with 28-byte entries. The index computation pattern in Thumb is `slot * 8 - slot = slot * 7`, then `* 4 = slot * 28`, i.e. `(slot << 3 - slot) << 2`.

```c
typedef struct EntityDeathStruct {
    u16 x;           /* +0x00: screen-space X position */
    u16 y;           /* +0x02: screen-space Y position */
    u8 unk04;        /* +0x04 */
    u8 unk05;        /* +0x05 */
    u8 unk06;        /* +0x06 */
    u8 unk07;        /* +0x07 */
    u8 phase;        /* +0x08: death animation phase counter (decrements each cycle) */
    u8 timer;        /* +0x09: frame timer (counts down per frame; resets to 0x19) */
    u8 slotIdx;      /* +0x0A: entity slot index */
    u8 unk0B;        /* +0x0B */
    u8 unk0C;        /* +0x0C: collision/state flags */
    u8 unk0D;        /* +0x0D: animation frame data */
    u8 unk0E;        /* +0x0E */
    u8 typeId;       /* +0x0F: entity type ID (e.g. 0x13=Moo, 0x1C=inactive) */
    u8 subState;     /* +0x10: sub-state */
    u8 behaviorId;   /* +0x11: behavior ID */
    u8 direction;    /* +0x12: direction/orientation flag */
    u8 unk13[9];     /* +0x13..+0x1B: padding to 28 bytes */
} EntityDeathStruct;  /* sizeof = 28 = 0x1C */
```

The struct size of 28 is critical: agbcc generates the `slot * 7 * 4` pattern only when `sizeof` is exactly 28. Changing any field sizes or adding padding would break the match.

## Key Addresses

| Address | Name | Description |
|---------|------|-------------|
| `0x03002920` | `gEntityArray` | Base of entity struct array |
| `0x0300528C` | `gEntityDeathState` | Byte holding the slot offset for linked entity pairs |

`gEntityDeathState[0]` stores an offset used to find a "linked" entity relative to the current slot. The death animation accesses `slot - offset` (parent entity, used for position source) and `slot + offset` (paired/shadow entity, used for sprite effects).

## Function Behavior

The function has two distinct phases:

### Phase 1: Timer and death state machine (inside the `if` block)

- `entity->timer` decrements each frame. When it underflows to `0xFF`, it resets to `0x19` (25 frames) and advances the death phase.
- `entity->phase` is the death phase counter that decrements each cycle:
  - **phase > 9**: manages a paired entity at `slot + offset` (clears its type, updates sprite via `LoadSpriteFrame`). At phase == 9, marks the paired entity as inactive (typeId = 0x1C, subState = 0).
  - **phase <= 5**: spawns a visual effect entity via `SpawnEntityAtPosition` (type = phase + 0x0C).
  - **phase == 0**: spawns a final entity (type 0x02) at the linked entity's position.
  - Each non-zero cycle plays sound effect 0x56 via `m4aSongNumStart`.

### Phase 2: Position synchronization (after the `if` block, runs every frame)

- Copies X/Y position from the parent entity (`slot - offset`), with Y offset -0x20 (32 pixels upward).
- If `phase > 9`, also positions the paired entity (`slot + offset`) at X-3 from parent, same Y-0x20.
- Adjusts the current entity's X by +3 (slight rightward offset from the paired entity).

## agbcc Register Allocation Challenges

### Challenge 1: `movs r4, #0xFF` mask scheduling

**Problem**: The target emits `movs r4, #0xFF` as a separate instruction before the timer zero-extend shift, keeping the mask in r4 for later use in `ands r0, r4`. agbcc instead combines the shift and mask, putting the zero-extended timer value directly into r4 and eliminating the separate mask load.

**Target assembly:**
```asm
strb r0, [r6, #0x09]    @ store timer
movs r4, #0xFF           @ mask = 0xFF (for later ands)
lsls r0, r0, #0x18      @ zero-extend timer
lsrs r0, r0, #0x18      @ (keeps result in r0)
cmp r0, #0xFF
```

**agbcc default output:**
```asm
strb r0, [r6, #0x09]    @ store timer
lsls r0, r0, #0x18      @ zero-extend timer
lsrs r4, r0, #0x18      @ (puts result in r4, combining with mask)
cmp r4, #0xFF
```

**Solution**: Use `register int mask asm("r4")` with an asm barrier to force the compiler to emit the mask load as a separate instruction:
```c
register int mask asm("r4");
/* ... */
entity->timer = entity->timer - 1;
asm("" : "=r"(mask) : "0"(0xFF));
if ((u8)entity->timer == 0xFF)
```

The `asm("" : "=r"(mask) : "0"(0xFF))` construct tells the compiler: "take the value 0xFF and place it in whatever register `mask` lives in (r4), treating this as a black-box operation." This forces an explicit `movs r4, #0xFF` at this exact point.

### Challenge 2: Extra zero-extend after `ands`

**Problem**: The expression `(entity->phase & mask) == 0` with `u8 mask` generates an extra `lsls r0, r0, #0x18` after the `ands r0, r4`, because agbcc zero-extends the `u8 & u8` result. The target has no such extra instruction since `ands` with 0xFF already guarantees an 8-bit result.

**Solutions attempted:**
| Approach | Result |
|----------|--------|
| `u8 mask`, `(entity->phase & mask) == 0` | Extra `lsl` after `ands` |
| `int mask`, `(entity->phase & mask) == 0` | Extra `ldrb` re-read from memory |
| `u8 val = entity->phase - 1; entity->phase = val; (val & mask) == 0` | Extra `lsl/lsr` before `strb` (u8 assignment forces zero-extend) |
| **`int val; val = entity->phase - 1; entity->phase = val; (val & mask) == 0`** | **No extra instructions** |

**Key insight**: Using `int` for both `val` and `mask` avoids all implicit zero-extends. The `int` type means:
- `val = entity->phase - 1` stores the raw subtraction result (no zero-extend for u8 truncation)
- `entity->phase = val` stores via `strb` which naturally truncates
- `(val & mask)` is `int & int`, so no implicit u8 promotion/zero-extend

### Challenge 3: Base address reload in the second half

**Problem**: The target reloads `0x03002920` into r3 after the if/else block, but agbcc keeps the value in r7 (where it was loaded at the start of the function). The `int base` variable keeps the value alive in r7 across the entire function.

**Target behavior:**
```asm
@ Second half:
ldr r3, =0x03002920      @ RELOAD into r3
lsls r0, r5, #0x03
subs r0, r0, r5
lsls r0, r0, #0x02
adds r4, r0, r3           @ entity_ptr in r4
ldr r6, =0x0300528C       @ death offset in r6
```

**agbcc default (using `base` variable):**
```asm
@ Second half:
lsls r0, r5, #0x03       @ (reuses r7, no reload)
subs r0, r0, r5
lsls r0, r0, #0x02
adds r3, r0, r7           @ entity_ptr in r3
ldr r4, =0x0300528C       @ death offset in r4
```

**Solution**: Use a new block-scoped variable with `register ... asm("r3")` and an asm barrier:
```c
{
    register int ents asm("r3");
    asm("" : "=r"(ents) : "0"(0x03002920));
    /* ... use ((EntityDeathStruct *)ents)[slot] ... */
}
```

This forces the compiler to load the constant into r3 specifically. With r3 taken for the base, the compiler naturally picks r4 for the entity pointer and r6 for the death offset pointer (via `gEntityDeathState`), matching the target exactly.

### Challenge 4: Commutative `adds` operand order

**Problem**: `adds r4, r0, r3` (target) vs `adds r4, r3, r0` (agbcc). In Thumb encoding T1 (`ADD Rd, Rn, Rm`), the operand order affects the binary: Rn and Rm are in different bit fields. Both compute `r0 + r3`, but the bytes differ.

**Target**: `0x18C4` = `adds r4, r0, r3` (Rn=r0, Rm=r3)
**agbcc default**: `0x181C` = `adds r4, r3, r0` (Rn=r3, Rm=r0)

**Solution**: Write the pointer computation with explicit integer arithmetic, placing the slot offset before the base:
```c
int tmp;
tmp = slot * sizeof(EntityDeathStruct);
ent = (EntityDeathStruct *)(tmp + ents);
```

Instead of:
```c
ent = &ents[slot];   /* generates adds r4, r3, r0 (base first) */
```

The expression `tmp + ents` puts the slot offset (r0) as the left operand, which agbcc maps to Rn. The `&ents[slot]` form puts `ents` (r3) as Rn because the pointer is the "base" of the array indexing.

### Challenge 5: Death offset pointer caching

**Problem**: Using `register u8 *deathOff asm("r6")` with an asm barrier for `0x0300528C` caused the compiler to re-read from `[r6]` for every use, adding 2 extra `ldrb` instructions (4 bytes). The target reads the offset once and reuses the register value.

**Solution**: Use the `gEntityDeathState` macro (defined as `((u8 *)0x0300528C)` in `globals.h`) without a register constraint. The compiler naturally loads the address into r6 via a pool constant and caches the byte value across uses.

## General Learnings for Klonoa: Empire of Dreams Decompilation

### 1. `-fno-fold-addr` does not prevent variable reuse

The flag prevents the compiler from folding address expressions across statements, but a single `int base = 0x03002920` variable still keeps the value in one register for the entire function. To force a reload, you need a new variable or an asm barrier.

### 2. `asm("" : "=r"(var) : "0"(val))` is the primary register control tool

This project uses this pattern extensively (see `src/gfx.c`, `src/code_0.c`, `src/engine.c`). It acts as a compiler barrier that:
- Forces a value into a specific register (via `register ... asm("rN")`)
- Prevents the optimizer from eliminating the assignment
- Does NOT add any bytes to the output (the `.code 16` directive it emits is a no-op)

### 3. Type choice affects zero-extend behavior

| Variable type | Expression | Extra instructions |
|--------------|------------|-------------------|
| `u8 x = expr` | assignment | `lsls/lsrs` to zero-extend |
| `int x = expr` | assignment | none (keeps full 32-bit value) |
| `u8 & u8` | binary and | `lsls` after `ands` |
| `int & int` | binary and | none |
| `u8 & int` | binary and | `ldrb` re-read from memory |

When the target uses `ands Rd, Rm` without a subsequent zero-extend, use `int` types for both the value and the mask.

### 4. Operand order in commutative Thumb instructions

In Thumb T1 encoding, `ADD Rd, Rn, Rm` has Rn and Rm in different bit positions. For `adds r4, r0, r3`:
- The left operand of `+` in C tends to map to Rn
- The right operand maps to Rm

To control ordering:
- `ptr + offset` puts ptr in Rn, offset in Rm
- `offset + ptr` (via explicit int arithmetic) puts offset in Rn, ptr in Rm

### 5. Pool layout and padding

agbcc emits literal pools after branches, with `.align 2, 0` for word alignment. The target has `lsls r0, r0, #0x00` (= `0x0000` = NOP) as explicit padding before pools. Both produce the same bytes (`0x0000`), so this is not a source of mismatch.

Each pool area contains the constants referenced by nearby `ldr Rd, [pc, #imm]` instructions. If the function uses the same constant in two distant code sections, agbcc may emit two separate pool entries (one per section), matching the target's behavior.

### 6. Block scoping partially controls register lifetime

In agbcc, `register ... asm("rN")` declared inside a block limits the constraint to that block's scope. This is useful for the second half of a function that needs different register assignments than the first half:

```c
/* First half: r4 = mask */
{
    register int mask asm("r4");
    asm("" : "=r"(mask) : "0"(0xFF));
    /* ... uses mask ... */
}
/* After block: r4 is free for the compiler to reuse */
{
    register int ents asm("r3");
    /* r4 naturally gets entity_ptr */
}
```

### 7. Direct `register asm = val` init: block scope controls timing

All `asm` barriers in this function were replaced with direct `register ... asm("rN") = val` initialization. The key factor is **block scoping**:

- **Function-scoped** `register int mask asm("r4") = 0xFF`: Fails — the compiler schedules `movs r4, #0xFF` at function entry, before the entity pointer setup.
- **Block-scoped** `register int mask asm("r4") = 0xFF` inside a `{ }` that opens right after `entity->timer -= 1`: Works — the initialization emits at the block entry point, exactly after the timer store.

The same pattern worked for `register int ents asm("r3") = 0x03002920` in the second-half block.

**Rule of thumb**: Direct init works when the register variable is declared at the point where the load should happen. Use a `{ }` block to control that point precisely.

### 8. Decomp-permuter reaches diminishing returns quickly

Both permuter runs converged to the same score (650) with similar code structure but couldn't resolve register allocation differences. The permuter is effective at finding the right C logic and expression patterns but cannot emit `asm` barriers or `register ... asm("rN")` constraints. The last mile of matching requires manual intervention for register-level control.

## Final State

**0 `asm` barriers** — all eliminated via block-scoped direct register initialization.

**2 `register asm` pins** (both irreducible — agbcc picks wrong registers without them):
```c
register int mask asm("r4") = 0xFF;        // block-scoped; without pin: r4 gets shift result
register int ents asm("r3") = 0x03002920;  // block-scoped; without pin: base stays in r7
```

## Progression

| Stage | Diffs | Size | Key change |
|-------|-------|------|------------|
| Permuter best (BG task 1) | ~80 | 376B | Base code with `int new_var` |
| + `register int mask asm("r4")` | 65 | 384B | Fixed `movs r4, #0xFF` |
| + `int val` for phase decrement | 18 | 384B | Eliminated extra zero-extend |
| + `register int ents asm("r3")` + `gEntityDeathState` | 1 | 384B | Fixed second-half register allocation |
| + `tmp + ents` operand order | **0** | **384B** | Fixed `adds r4, r0, r3` ordering |
| + direct init for `ents` (barrier reduction) | **0** | **384B** | Replaced `asm` barrier with `= 0x03002920` |
| + block-scoped direct init for `mask` | **0** | **384B** | Eliminated last `asm` barrier with `{ register int mask asm("r4") = 0xFF; ... }` |
| + code quality pass | **0** | **384B** | Docstring, `*gEntityDeathState`, variable names, comments, `make format` |

## Code Quality Improvements

After achieving the match, the following safe improvements were applied:

### What worked (match preserved)

1. **Docstring**: Added `/** ... */` explaining function behavior, phases, and linked entity system.
2. **`*gEntityDeathState` macro**: Replaced all raw `*(u8 *)0x0300528C` with the project's existing `globals.h` define. Safe because both expand to the same expression.
3. **Variable renames**: `pslot` to `pairedSlot`. Names don't affect output.
4. **Decimal for frame count**: `0x19` to `25`. Integer literals produce the same `movs` instruction regardless of base.
5. **Inline comments**: `/* gEntityArray */` next to `0x03002920`, `/* inactive */` next to `0x1C`.
6. **Indentation fix**: The if/else block had broken indentation from the matching process.
7. **`make format`**: Applied `clang-format` project-wide for consistent style.

### What broke the match

- **`gEntityArray` macro for base address**: `gEntityArray` expands to `((u8 *)0x03002920)` (pointer cast). Writing `base = (int)gEntityArray` instead of `base = 0x03002920` changes instruction scheduling — the compiler processes a pointer-to-int cast differently than a plain integer literal, reordering the `ldr` relative to the `slot * 28` computation. The raw literal must stay.
