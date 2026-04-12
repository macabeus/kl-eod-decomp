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
