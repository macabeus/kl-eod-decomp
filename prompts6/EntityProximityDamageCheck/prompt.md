You are decompiling an assembly function called `EntityProximityDamageCheck` in ARMv4T from a Game Boy Advance game.

# Entity System Context

This function is part of the entity system in Klonoa: Empire of Dreams. It checks whether the player entity is within damage range of nearby enemies and applies a knockback/damage reaction.

## Entity Array (`gEntityArray` at `0x03002920`)

Each entity occupies a **28-byte** (0x1C) struct. The index computation pattern in the assembly is: `slot * 8 - slot = slot * 7`, then `* 4 = slot * 28`.

Known struct fields:

| Offset | Type | Field |
|--------|------|-------|
| +0x00 | u16 | Screen-space X position |
| +0x02 | u16 | Screen-space Y position |
| +0x08 | u8 | Damage state flag (set to 1 when hit detected, cleared after invincibility timer expires) |
| +0x0A | u8 | Entity slot index |
| +0x0C | u8 | Collision/state flags |
| +0x0D | u8 | Animation frame data |
| +0x0F | u8 | Entity type ID — primary dispatch key (e.g. 0x13 = Moo enemy) |
| +0x10 | u8 | Sub-state |
| +0x11 | u8 | Behavior ID — secondary dispatch key. This function checks for specific behavior states: 0x6F, 0x25, or > 0x75 to determine if the entity can deal contact damage |
| +0x12 | u8 | Direction/orientation flag |

## Key addresses

- `0x030034A4`: start index for entity slot iteration
- `0x030052B0`: end index for entity slot iteration (inclusive upper bound)
- `0x03000810`: global "damage sound playing" flag — prevents the Moo damage sound (0x5D) from being triggered multiple times simultaneously
- `0x03004C20`: level/world state struct — fields at +0x0C, +0x0D, +0x0E store world/level/area indices (1-based) used to look up invincibility frame duration from a ROM table
- `0x080E2B64` (ROM): per-level invincibility duration table. Indexed by `(world-1) * 0x8980 + (level-1) * 0x1130 + (area-1) * 0x2C + (slot-1) * 8`, with the duration at offset +0x02 in each entry

## Function behavior

**Part 1 — Collision detection loop**: Iterates through entity slots from `[0x030034A4]` to `[0x030052B0]`. For each entity:
1. Skips if entity type > 0x1A (outside valid entity range)
2. Skips unless behavior ID (`entity[0x11]`) is 0x6F, 0x25, or > 0x75 (only certain behavior states can deal damage)
3. Performs AABB overlap test between the iterated entity and the target entity (the function's argument, typically the player):
   - X overlap: `target_x - 8` vs `iterated_x + 7` (16px wide hitbox)
   - Y overlap: `target_y - 0x40` vs `iterated_y`, with 0x18 height offset
4. On hit: applies Y knockback (`target_y -= 23`, i.e. `+= 0xFFE9` as unsigned), sets `target[0x08] = 1`
5. Special case for entity type 0x13 (Moo): plays damage sound effect 0x5D via `m4aSongNumStart` and sets the sound flag at `0x03000810`

**Part 2 — Invincibility timer**: After the loop, if `target[0x08] != 0` (damage was dealt), increments `target[0x02]` (Y position, used as invincibility frame counter). Looks up the expected duration from the ROM table based on current world/level/area. When the counter matches the duration, clears the damage flag and stops the sound.





# Declarations for the functions called from the target assembly

- `void m4aSongNumStart(u16);`



# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/code_1/EntityProximityDamageCheck.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	thumb_func_start EntityProximityDamageCheck
EntityProximityDamageCheck: @ 0801C6EC
	push {r4, r5, r6, r7, lr}
	mov r7, r9
	mov r6, r8
	push {r6, r7}
	lsls r0, r0, #0x18
	lsrs r0, r0, #0x18
	mov r8, r0
	ldr r0, _0801C788 @ =0x030034A4
	ldr r7, [r0, #0x00]
	ldr r0, _0801C78C @ =0x030052B0
	ldr r1, [r0, #0x00]
	mov r9, r0
	ldr r0, _0801C790 @ =0x03002920
	cmp r7, r1
	bhi _0801C7AA
	mov r12, r0
	lsls r0, r7, #0x03
	subs r0, r0, r7
	lsls r0, r0, #0x02
	mov r1, r12
	adds r5, r0, r1
	adds r0, #0x0F
	adds r6, r0, r1
_0801C71A:
	ldrb r0, [r6, #0x00]
	cmp r0, #0x1A
	bhi _0801C79C
	ldrb r0, [r6, #0x02]
	cmp r0, #0x6F
	beq _0801C72E
	cmp r0, #0x25
	beq _0801C72E
	cmp r0, #0x75
	bls _0801C79C
_0801C72E:
	mov r2, r8
	lsls r0, r2, #0x03
	subs r0, r0, r2
	lsls r0, r0, #0x02
	mov r1, r12
	adds r4, r0, r1
	ldrh r3, [r4, #0x00]
	adds r1, r3, #0x0
	subs r1, #0x08
	ldrh r2, [r5, #0x00]
	adds r0, r2, #0x7
	cmp r1, r0
	bge _0801C79C
	adds r1, #0x10
	subs r0, #0x16
	cmp r1, r0
	ble _0801C79C
	ldrh r2, [r4, #0x02]
	adds r0, r2, #0x0
	subs r0, #0x40
	ldrh r1, [r5, #0x02]
	cmp r0, r1
	bge _0801C79C
	adds r0, r1, #0x0
	subs r0, #0x18
	cmp r2, r0
	ble _0801C79C
	ldr r2, _0801C794 @ =0x0000FFE9
	adds r0, r1, r2
	strh r0, [r4, #0x02]
	movs r6, #0x01
	strb r6, [r4, #0x08]
	ldrb r0, [r5, #0x0F]
	cmp r0, #0x13
	bne _0801C7AA
	ldr r4, _0801C798 @ =0x03000810
	ldrb r0, [r4, #0x00]
	cmp r0, #0x00
	bne _0801C7AA
	movs r0, #0x5D
	bl m4aSongNumStart
	strb r6, [r4, #0x00]
	b _0801C7AA
	lsls r0, r0, #0x00
_0801C788: .4byte 0x030034A4
_0801C78C: .4byte 0x030052B0
_0801C790: .4byte 0x03002920
_0801C794: .4byte 0x0000FFE9
_0801C798: .4byte 0x03000810
_0801C79C:
	adds r5, #0x1C
	adds r6, #0x1C
	adds r7, #0x01
	mov r1, r9
	ldr r0, [r1, #0x00]
	cmp r7, r0
	bls _0801C71A
_0801C7AA:
	ldr r0, _0801C814 @ =0x03002920
	mov r2, r8
	lsls r1, r2, #0x03
	subs r1, r1, r2
	lsls r1, r1, #0x02
	adds r6, r1, r0
	ldrb r0, [r6, #0x08]
	cmp r0, #0x00
	beq _0801C808
	ldrh r3, [r6, #0x02]
	adds r3, #0x01
	movs r7, #0x00
	strh r3, [r6, #0x02]
	ldr r5, _0801C818 @ =0x080E2B64
	ldr r4, _0801C81C @ =0x03004C20
	ldrb r1, [r4, #0x0E]
	subs r1, #0x01
	lsls r1, r1, #0x03
	subs r2, #0x0D
	movs r0, #0x2C
	muls r0, r2
	adds r1, r1, r0
	ldrb r0, [r4, #0x0C]
	subs r0, #0x01
	ldr r2, _0801C820 @ =0x00001130
	muls r0, r2
	adds r1, r1, r0
	ldrb r0, [r4, #0x0D]
	subs r0, #0x01
	ldr r2, _0801C824 @ =0x00008980
	muls r0, r2
	adds r1, r1, r0
	adds r1, r1, r5
	lsls r3, r3, #0x10
	lsrs r3, r3, #0x10
	ldrh r1, [r1, #0x02]
	cmp r3, r1
	bne _0801C808
	strb r7, [r6, #0x08]
	ldr r1, _0801C828 @ =0x03000810
	ldrb r0, [r1, #0x00]
	cmp r0, #0x01
	bne _0801C808
	strb r7, [r1, #0x00]
	movs r0, #0x5D
	bl m4aMPlayCommand
_0801C808:
	pop {r3, r4}
	mov r8, r3
	mov r9, r4
	pop {r4, r5, r6, r7}
	pop {r0}
	bx r0
_0801C814: .4byte 0x03002920
_0801C818: .4byte 0x080E2B64
_0801C81C: .4byte 0x03004C20
_0801C820: .4byte 0x00001130
_0801C824: .4byte 0x00008980
_0801C828: .4byte 0x03000810
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
