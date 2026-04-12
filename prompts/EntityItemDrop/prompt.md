You are decompiling an assembly function called `EntityItemDrop` in ARMv4T from a Game Boy Advance game.

# Entity System Context

This function is part of the entity system in Klonoa: Empire of Dreams. It handles item drop behavior when an entity is defeated — the collectible that floats up in an arc and lands.

## Entity Array (`gEntityArray` at `0x03002920`)

Each entity occupies a **28-byte** (0x1C) struct. The index computation pattern in the assembly is: `slot * 8 - slot = slot * 7`, then `* 4 = slot * 28`.

Known struct fields:

| Offset | Type | Field |
|--------|------|-------|
| +0x00 | u16 | Screen-space X position |
| +0x02 | u16 | Screen-space Y position |
| +0x08 | s8 | Horizontal velocity (signed — used for X movement per frame in state 0x00) |
| +0x09 | s8 | Vertical velocity / amplitude (signed — used for arc height calculation) |
| +0x0A | u8 | Entity slot index |
| +0x0C | u8 | Collision/state flags (lower 4 bits masked in setup) |
| +0x0D | u8 | Animation frame data |
| +0x0F | u8 | Entity type ID / state variable — in this function acts as a state machine: 0x03 and 0x04 = initialization states, 0x00 = arc animation, 0x1C = done/despawn |
| +0x10 | u8 | Sub-state |
| +0x14 | u16 | Animation timer / arc phase counter (incremented by step value each frame, compared against 0x88 to end the arc) |
| +0x16 | u8 | Timer step value (how fast the arc progresses: 0x04 for state 0x03, 0x02 for state 0x04) |

## Key addresses

- `0x03005400` (+0x0A): a global flag — when set to 0x01, the item is immediately despawned (entity type set to 0x1C)
- `0x080E2ADE` (ROM): lookup table for item visual parameters — 2 bytes per entry, indexed by item type. Provides `entity[0x08]` (horizontal velocity) and `entity[0x09]` (vertical amplitude). States 0x03 and 0x04 read from different offsets in this table (+0 and +0x0A respectively)
- `0x080D8E14` (ROM): sine/cosine lookup table — indexed by `entity[0x14]` (arc phase), returns s16 values used for vertical position calculation

## Function behavior

This is a state machine driven by `entity[0x0F]`:
- **State 0x03 / 0x04** (initialization): Sets up the item drop — positions it relative to the player entity (at entity array offset 0xFC*2 = slot 0), clears collision flags, reads velocity parameters from ROM table. State 0x03 uses faster step (4) and different table offset than 0x04 (step 2)
- **State 0x00** (arc animation): Moves the item in an arc. Y position is computed using the sine table: `y = 0x10C - (amplitude * sine[phase]) >> 8`. X increments by horizontal velocity each frame. When arc phase reaches 0x88, transitions to state 0x1C
- **State 0x1C**: item is inactive/collected

The player entity direction flag at `gEntityArray[0].field_at_0x204` (offset `0x81 << 2 = 0x204` from array base) determines whether the item drops left or right (X ± 0x10 from player position).









# Starting Point — Use This Code As Your First Attempt

The following C code produces the correct prologue (`push {r4, r5, lr}`), correct switch setup (state in r1, slot<<3 in r2, intermediate in r1), matching ROM table patterns, and has ~24 halfword diffs remaining. **You MUST use this as your first `compile_and_view_assembly` submission**, then iterate from there.

```c
void EntityItemDrop(u8 arg0) {
    u32 shifted = (u32)arg0 << 24;
    register u32 slot asm("r4") = shifted >> 24;
    register u8 itemType asm("r5") = (shifted + ((u32)0xE4 << 24)) >> 24;
    asm("" : "=r"(slot) : "0"(slot));
    if (gGameFlagsPtr[0x0A] == 1) {
        u8 *b;
        u8 *e;
        u8 zero;
        b = gEntityArray;
        e = b + ((slot << 3) - slot) * 4;
        zero = 0;
        e[0x0F] = 0x1C;
        e[0x10] = zero;
        return;
    }
    {
        u8 *base;
        register u8 *entityBase asm("r3");
        register u32 shl3 asm("r2");
        u8 state;
        register u8 *new_var asm("r12");
        asm("" : "=r"(base) : "0"((u8 *)0x03002920));
        shl3 = slot << 3;
        {
            register u32 offs asm("r1") = (shl3 - slot) << 2;
            entityBase = (u8 *)(offs + (u32)base);
        }
        state = entityBase[0x0F]; new_var = base;
        switch (state) {
        case 3: {
            u8 one;
            register u8 *ent asm("r1");
            { u8 zero = 0; entityBase[0x0F] = zero; *(u16 *)(entityBase + 0x14) = zero; }
            one = 1; entityBase[0x10] = one;
            entityBase[0x0C] = (one - 5) & entityBase[0x0C];
            { u32 dir = *(u8 *)(new_var + 0x204); dir = (dir << 0x1C) >> 0x1E;
              if (dir == 0) { u16 xp = *(u16 *)(new_var + 0x1F8); xp += 0x10; *(u16 *)(entityBase) = xp; }
              else { u16 xp = *(u16 *)(new_var + 0x1F8); xp -= 0x10; *(u16 *)(entityBase) = xp; } }
            ent = new_var + (shl3 - slot) * 4;
            *(u16 *)(ent + 0x02) = *(u16 *)(new_var + 0x1FA);
            { register u8 *rt2 asm("r2") = (u8 *)0x080E2ADE;
            register u32 idx3 asm("r3") = (u32)itemType << 1;
            ent[0x08] = *((u8 *)(idx3 + (u32)rt2));
            rt2++;
            idx3 = idx3 + (u32)rt2;
            ent[0x09] = *(u8 *)idx3; }
            ent[0x16] = 4; break; }
        case 4: {
            u8 one;
            register u8 *ent asm("r1");
            { u8 zero = 0; entityBase[0x0F] = zero; *(u16 *)(entityBase + 0x14) = zero; }
            one = 1; entityBase[0x10] = one;
            entityBase[0x0C] = (one - 5) & entityBase[0x0C];
            { u32 dir = *(u8 *)(new_var + 0x204); dir = (dir << 0x1C) >> 0x1E;
              if (dir == 0) { u16 xp = *(u16 *)(new_var + 0x1F8); xp += 0x10; *(u16 *)(entityBase) = xp; }
              else { u16 xp = *(u16 *)(new_var + 0x1F8); xp -= 0x10; *(u16 *)(entityBase) = xp; } }
            ent = new_var + (shl3 - slot) * 4;
            *(u16 *)(ent + 0x02) = *(u16 *)(new_var + 0x1FA);
            { register u8 *rt2 asm("r2") = (u8 *)0x080E2ADE;
            register u32 idx3 asm("r3") = (u32)itemType << 1;
            { u8 *b2 = (u8 *)((u32)rt2 + 0x0A); ent[0x08] = *((u8 *)(idx3 + (u32)b2)); }
            rt2 += 0x0B;
            idx3 = idx3 + (u32)rt2;
            ent[0x09] = *(u8 *)idx3; }
            ent[0x16] = 2; break; }
        case 0: {
            s8 *eb_s8 = (s8 *)entityBase;
            s8 vm; s16 *st; s16 sv; s16 yo;
            vm = eb_s8[0x09]; st = (s16 *)0x080D8E14;
            sv = st[*(u16 *)(entityBase + 0x14)]; yo = (vm * sv) >> 8;
            *(u16 *)(entityBase + 0x02) = (u16)(0x10C - yo);
            { s8 xv = eb_s8[0x08]; u16 xp = *(u16 *)(entityBase); *(u16 *)(entityBase) = (u16)(xv + xp); }
            { u8 stp = entityBase[0x16]; u16 ang = *(u16 *)(entityBase + 0x14); ang += stp; *(u16 *)(entityBase + 0x14) = ang;
              if ((u16)ang == 0x88) { entityBase[0x0F] = 0x1C; { u8 z; asm("" : "=r"(z) : "0"(0)); entityBase[0x10] = z; } } } break; }
        default: break;
        }
    }
}
```

## What this code gets right

- **Prologue**: `push {r4, r5, lr}` — matches target exactly (no r6 spill)
- **Function size**: 372 bytes — matches target exactly
- **Switch setup**: `lsls r2, r4, #3; subs r1, r2, r4; lsls r1, r1, #2; adds r3, r1, r0` — matches target register assignments (slot<<3→r2, intermediate→r1, entity→r3)
- **State register**: `ldrb r1, [r3, #0xf]` — state in r1, matches target
- **No strength reduction**: `lsl r2, r4, #3` (clean shift, not `(slot<<24)>>21`)
- **`register asm` pins**: `slot` (u32) → r4, `itemType` → r5, `entityBase` → r3, `shl3` → r2, `new_var` → r12, `offs` → r1, `ent` → r1
- **`asm("" : "=r"(slot) : "0"(slot))` after extraction**: Breaks the shifted→slot CSE chain, prevents `(slot<<24)>>21` strength reduction. MUST use `u32` type for slot (not u8) to avoid masking.
- **`asm("" : "=r"(z) : "0"(0))` in case 0**: Prevents the compiler from reusing the switch state register (known to be 0 in case 0) for the `entityBase[0x10] = 0` store. Without this, state's lifetime extends to end of case 0, conflicting with r0/r1, forcing state to r6.
- **Entity recomp via `shl3 - slot`**: Using `ent = new_var + (shl3 - slot) * 4` keeps slot (r4) and shl3 (r2) alive through the switch cases
- **Both cases use `u8 zero = 0` + `u32 dir`**: Works with the new register allocation structure
- **Case structure**: switch on `state` with cases 3, 4, 0 and default
- **ROM table patterns**: `register u8 *rt2 asm("r2")` + `register u32 idx3 asm("r3") = (u32)itemType << 1` — shifts itemType to r3 (preserving r5), table ptr in r2. Expression `idx3 + (u32)rt2` controls operand order. Second byte via `idx3 = idx3 + (u32)rt2` makes r3 the accumulator.
- **Case 4 ROM table**: `{ u8 *b2 = (u8 *)((u32)rt2 + 0x0A); ... }` matches target's `adds r0, r2, #0; adds r0, #0xA` pattern

## What still needs fixing (~24 halfword diffs)

### 1. Case 0 register allocation (~20 diffs)
The compiler uses r4 for temporaries (sine table pointer, zero constant, 0x10C) because slot is dead in case 0. Target uses r1/r2 for these. Keeping slot alive via `asm("" : : "r"(slot))` causes r6/r7 push (too much register pressure). The target's case 0 loads entity[9] FIRST into r2, then sine table into r1 — completely different operation order and register assignment from compiled output.

### 2. Mask instruction order (cases 3 & 4, ~4 diffs)
Compiled generates `subs r0, #5; ldrb r1, [r3, #0xC]` (subtract first). Target has `ldrb r1; subs r0` (load first). This is instruction scheduling — hard to control from C. Separating into a load variable causes massive regressions (100+ extra diffs).

## Important agbcc-specific knowledge

- `gGameFlagsPtr` is `extern u8[]` at `0x03005400` — prevents offset folding
- `gEntityArray` is `extern u8[]` at `0x03002920` — using it for early return changes register assignment
- `register ... asm("r12")` pins to ip (NOT callee-saved — no push needed)
- `register u32 slot asm("r4")` — MUST be u32 (not u8) to avoid masking after asm barrier
- `asm("" : "=r"(base) : "0"(val))` forces a pool load at a specific point
- `u8 dir` causes arithmetic shift right (`asrs`); `u32 dir` gives logical shift (`lsrs`)
- `register u32 offs asm("r1")` for the switch setup intermediate makes it go through r1 (matching target)
- Entity ptr operand order: `(u8 *)(offs + (u32)base)` generates `adds r3, r1, r0` (matching target's operand order)
- `register u32 idx3 asm("r3") = (u32)itemType << 1` — ONLY way to get `lsls r3, r5, #1` (shift to different register). Plain variables, `add`, and inline asm all produce in-place `lsl r5, r5, #1`
- Expression order in ADD controls encoding: `idx3 + (u32)rt2` → `add r0, r3, r2` (r3 first); `rt2 + idx3` → `add r0, r2, r3` (r2 first)
- **agbcc constant propagation through switch**: In case 0, the compiler knows `state == 0` and reuses the state register for zero stores. This extends state's lifetime, causing conflicts. The `asm("" : "=r"(z) : "0"(0))` barrier creates a fresh zero pseudo that the compiler can't trace back to state.

## What has been tried and DOES NOT WORK

1. **`register u8 state asm("r1")`** — compiler copies state to r6 anyway (`add r6, r1, #0`), adding extra instruction
2. **`asm("" : "+r"(slot))` or `asm("" : "=r"(slot) : "0"(slot))` with u8 type** — causes masking (`lsl r4, r4, #0x18; lsr r4, r4, #0x18`)
3. **`slot = 0x0A` overwrite** — breaks prologue and switch setup
4. **`-O1` compiler flag** — breaks 479 other functions
5. **`-fcaller-saved-preference` flag as Makefile global** — breaks existing matched functions (too broad). Only safe for per-function use via Mizuchi's compilerScript
6. **Separating mask load into variable** — `{ u8 mc = entityBase[0x0C]; ... }` causes 100+ extra diffs
7. **Keeping slot alive in case 0** — `asm("" : : "r"(slot))` causes r6/r7 push (register pressure too high). Even just slot alone (without shl3) causes push {r4, r5, r6, r7, lr}
8. **`entityBase[0x0C] &= one - 5`** — generates different code than `= (one - 5) & entityBase[0x0C]`
9. **`entRecomp = entityBase` approach** — makes slot dead → state reuses r4 → wrong state register
10. **Plain `u32 idx` for ROM table shift** — compiler still generates in-place `lsl r5, r5, #1`. Only `register ... asm("r3")` forces shift to different register
11. **`itemType + itemType` instead of shift** — compiler optimizes add back to in-place shift
12. **`asm("lsl %0, %1, #1" : "=l"(idx) : "l"(itemType))`** — compiler allocates r5 for output too, still in-place
13. **Removing `s8 *eb_s8` and using direct casts** — `((s8 *)entityBase)[0x09]` eliminates the `add r2, r3, #0` entity copy, making the function 1 halfword shorter → misaligns everything after case 0 (45 diffs)
14. **Scoping `s16 *st` in a sub-block** — `{ s16 *st = ...; sv = st[...]; }` — compiler sees through the scope, still uses r4
15. **`register s16 *st asm("r1")` in case 0** — causes r6/r7 push (too much register pressure)
16. **`-fcaller-saved-preference` for case 0** — has no effect because slot (r4) isn't pinned in case 0 (it's dead), so `user_pinned_regs` doesn't apply
17. **`register s32 vm asm("r2")` + inline asm for multiply** — changes too many things, increases diffs

# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/code_1/EntityItemDrop.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	non_word_aligned_thumb_func_start EntityItemDrop
EntityItemDrop: @ 0801F4D0
	push {r4, r5, lr}
	lsls r0, r0, #0x18
	lsrs r4, r0, #0x18
	movs r1, #0xE4
	lsls r1, r1, #0x18
	adds r0, r0, r1
	lsrs r5, r0, #0x18
	ldr r0, [pc, #0x01C] @ =0x03005400
	ldrb r0, [r0, #0x0A]
	cmp r0, #0x01
	bne _0801F504
	ldr r1, [pc, #0x018] @ =0x03002920
	lsls r0, r4, #0x03
	subs r0, r0, r4
	lsls r0, r0, #0x02
	adds r0, r0, r1
	movs r2, #0x00
	movs r1, #0x1C
	strb r1, [r0, #0x0F]
	strb r2, [r0, #0x10]
	.2byte 0xE0A1 @ b _0801F63E
	lsls r0, r0, #0x00
	.4byte 0x03005400
	.4byte 0x03002920
_0801F504:
	ldr r0, [pc, #0x018] @ =0x03002920
	lsls r2, r4, #0x03
	subs r1, r2, r4
	lsls r1, r1, #0x02
	adds r3, r1, r0
	ldrb r1, [r3, #0x0F]
	mov r12, r0
	cmp r1, #0x03
	beq _0801F52A
	cmp r1, #0x03
	bgt _0801F524
	cmp r1, #0x00
	beq _0801F5FC
	.2byte 0xE08E @ b _0801F63E
	.4byte 0x03002920
_0801F524:
	cmp r1, #0x04
	beq _0801F590
	.2byte 0xE089 @ b _0801F63E
_0801F52A:
	movs r0, #0x00
	strb r0, [r3, #0x0F]
	strh r0, [r3, #0x14]
	movs r0, #0x01
	strb r0, [r3, #0x10]
	ldrb r1, [r3, #0x0C]
	subs r0, #0x05
	ands r0, r1
	strb r0, [r3, #0x0C]
	movs r0, #0x81
	lsls r0, r0, #0x02
	add r0, r12
	ldrb r0, [r0, #0x00]
	lsls r0, r0, #0x1C
	lsrs r0, r0, #0x1E
	cmp r0, #0x00
	bne _0801F558
	movs r0, #0xFC
	lsls r0, r0, #0x01
	add r0, r12
	ldrh r0, [r0, #0x00]
	adds r0, #0x10
	.2byte 0xE004 @ b _0801F562
_0801F558:
	movs r0, #0xFC
	lsls r0, r0, #0x01
	add r0, r12
	ldrh r0, [r0, #0x00]
	subs r0, #0x10
	strh r0, [r3, #0x00]
	subs r1, r2, r4
	lsls r1, r1, #0x02
	add r1, r12
	movs r0, #0xFD
	lsls r0, r0, #0x01
	add r0, r12
	ldrh r0, [r0, #0x00]
	strh r0, [r1, #0x02]
	ldr r2, [pc, #0x014] @ =0x080E2ADE
	lsls r3, r5, #0x01
	adds r0, r3, r2
	ldrb r0, [r0, #0x00]
	strb r0, [r1, #0x08]
	adds r2, #0x01
	adds r3, r3, r2
	ldrb r0, [r3, #0x00]
	strb r0, [r1, #0x09]
	movs r0, #0x04
	strb r0, [r1, #0x16]
	.2byte 0xE058 @ b _0801F63E
	.4byte 0x080E2ADE
_0801F590:
	movs r0, #0x00
	strb r0, [r3, #0x0F]
	strh r0, [r3, #0x14]
	movs r0, #0x01
	strb r0, [r3, #0x10]
	ldrb r1, [r3, #0x0C]
	subs r0, #0x05
	ands r0, r1
	strb r0, [r3, #0x0C]
	movs r0, #0x81
	lsls r0, r0, #0x02
	add r0, r12
	ldrb r0, [r0, #0x00]
	lsls r0, r0, #0x1C
	lsrs r0, r0, #0x1E
	cmp r0, #0x00
	bne _0801F5BE
	movs r0, #0xFC
	lsls r0, r0, #0x01
	add r0, r12
	ldrh r0, [r0, #0x00]
	adds r0, #0x10
	.2byte 0xE004 @ b _0801F5C8
_0801F5BE:
	movs r0, #0xFC
	lsls r0, r0, #0x01
	add r0, r12
	ldrh r0, [r0, #0x00]
	subs r0, #0x10
	strh r0, [r3, #0x00]
	subs r1, r2, r4
	lsls r1, r1, #0x02
	add r1, r12
	movs r0, #0xFD
	lsls r0, r0, #0x01
	add r0, r12
	ldrh r0, [r0, #0x00]
	strh r0, [r1, #0x02]
	ldr r2, [pc, #0x01C] @ =0x080E2ADE
	lsls r3, r5, #0x01
	adds r0, r2, #0x0
	adds r0, #0x0A
	adds r0, r3, r0
	ldrb r0, [r0, #0x00]
	strb r0, [r1, #0x08]
	adds r2, #0x0B
	adds r3, r3, r2
	ldrb r0, [r3, #0x00]
	strb r0, [r1, #0x09]
	movs r0, #0x02
	strb r0, [r1, #0x16]
	.2byte 0xE023 @ b _0801F63E
	lsls r0, r0, #0x00
	.4byte 0x080E2ADE
_0801F5FC:
	movs r2, #0x09
	ldsb r2, [r3, r2]
	ldr r1, [pc, #0x040] @ =0x080D8E14
	ldrh r0, [r3, #0x14]
	lsls r0, r0, #0x01
	adds r0, r0, r1
	movs r1, #0x00
	ldsh r0, [r0, r1]
	adds r1, r2, #0x0
	muls r1, r0
	asrs r1, r1, #0x08
	movs r2, #0x86
	lsls r2, r2, #0x01
	adds r0, r2, #0x0
	subs r0, r0, r1
	strh r0, [r3, #0x02]
	movs r0, #0x08
	ldsb r0, [r3, r0]
	ldrh r1, [r3, #0x00]
	adds r0, r0, r1
	strh r0, [r3, #0x00]
	ldrb r1, [r3, #0x16]
	ldrh r0, [r3, #0x14]
	adds r0, r0, r1
	strh r0, [r3, #0x14]
	lsls r0, r0, #0x10
	lsrs r0, r0, #0x10
	cmp r0, #0x88
	bne _0801F63E
	movs r0, #0x1C
	strb r0, [r3, #0x0F]
	movs r0, #0x00
	strb r0, [r3, #0x10]
_0801F63E:
	pop {r4, r5}
	pop {r0}
	bx r0
	.4byte 0x080D8E14
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
