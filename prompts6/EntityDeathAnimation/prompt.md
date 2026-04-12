You are decompiling an assembly function called `EntityDeathAnimation` in ARMv4T from a Game Boy Advance game.

# Entity System Context

This function is part of the entity system in Klonoa: Empire of Dreams. It plays the death/destruction animation when an entity (e.g. a Moo enemy) is defeated.

## Entity Array (`gEntityArray` at `0x03002920`)

Each entity occupies a **28-byte** (0x1C) struct. The index computation pattern in the assembly is: `slot * 8 - slot = slot * 7`, then `* 4 = slot * 28`.

Known struct fields:

| Offset | Type | Field |
|--------|------|-------|
| +0x00 | u16 | Screen-space X position |
| +0x02 | u16 | Screen-space Y position |
| +0x08 | u8 | Multi-purpose: death phase counter in this function (decremented each cycle, checked against 0x05 and 0x09) |
| +0x09 | u8 | Frame timer / countdown (decremented each tick; resets to 0x19 = 25 when it wraps past 0) |
| +0x0A | u8 | Entity slot index |
| +0x0C | u8 | Collision/state flags |
| +0x0D | u8 | Animation frame data |
| +0x0F | u8 | Entity type ID — primary dispatch key (e.g. 0x13 = Moo enemy, 0x1C = inactive/despawn marker) |
| +0x10 | u8 | Sub-state / secondary state |
| +0x11 | u8 | Behavior ID — secondary dispatch key used by `EntityUpdateDispatch` |
| +0x12 | u8 | Direction/orientation flag |

## Key addresses

- `0x0300528C`: global that stores an entity slot offset — used to compute a "parent" or "linked" entity slot relative to the current slot (the pattern `slot - offset` and `slot + offset` accesses linked entity pairs)

## Function behavior

The death animation runs as a countdown: `entity[0x09]` decrements each frame. When it wraps to 0xFF (underflow), it resets to 0x19 (25 frames) and decrements `entity[0x08]` (phase counter). Different phases trigger different effects:
- Phase > 5: spawns a visual effect entity via `SpawnEntityAtPosition` with type derived from current phase
- Phase == 0: spawns a final entity (type 0x02) at the linked entity's position, then the function finishes
- Phase > 9: manages a paired entity at `slot + offset` — clears its type, updates its sprite via `LoadSpriteFrame`
- Phase == 9: reassigns the paired entity to type 0x1C (inactive) with sub-state 0

Throughout the animation, the dying entity's screen position is copied from the linked entity (`slot - offset`), with a Y offset of -0x20 (32 pixels upward).





# Declarations for the functions called from the target assembly

- `void m4aSongNumStart(u16);`



# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/code_1/EntityDeathAnimation.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	thumb_func_start EntityDeathAnimation
EntityDeathAnimation: @ 0801DE44
	push {r4, r5, r6, r7, lr}
	mov r7, r8
	push {r7}
	lsls r0, r0, #0x18
	lsrs r5, r0, #0x18
	ldr r7, _0801DEC0 @ =0x03002920
	lsls r0, r5, #0x03
	subs r0, r0, r5
	lsls r0, r0, #0x02
	adds r6, r0, r7
	ldrb r0, [r6, #0x09]
	subs r0, #0x01
	movs r1, #0x00
	mov r8, r1
	strb r0, [r6, #0x09]
	movs r4, #0xFF
	lsls r0, r0, #0x18
	lsrs r0, r0, #0x18
	cmp r0, #0xFF
	bne _0801DF3E
	movs r0, #0x19
	strb r0, [r6, #0x09]
	ldrb r2, [r6, #0x08]
	cmp r2, #0x05
	bhi _0801DE94
	ldr r0, _0801DEC4 @ =0x0300528C
	ldrb r0, [r0, #0x00]
	subs r0, r5, r0
	lsls r1, r0, #0x03
	subs r1, r1, r0
	lsls r1, r1, #0x02
	adds r1, r1, r7
	ldrh r0, [r1, #0x00]
	ldrh r1, [r1, #0x02]
	adds r2, #0x0C
	lsls r2, r2, #0x18
	lsrs r2, r2, #0x18
	movs r3, #0x00
	bl SpawnEntityAtPosition
_0801DE94:
	ldrb r0, [r6, #0x08]
	subs r0, #0x01
	strb r0, [r6, #0x08]
	ands r0, r4
	cmp r0, #0x00
	bne _0801DEC8
	ldr r0, _0801DEC4 @ =0x0300528C
	ldrb r3, [r0, #0x00]
	subs r3, r5, r3
	lsls r1, r3, #0x03
	subs r1, r1, r3
	lsls r1, r1, #0x02
	adds r1, r1, r7
	ldrh r0, [r1, #0x00]
	ldrh r1, [r1, #0x02]
	lsls r3, r3, #0x18
	lsrs r3, r3, #0x18
	movs r2, #0x02
	bl SpawnEntityAtPosition
	b _0801DF3E
	lsls r0, r0, #0x00
_0801DEC0: .4byte 0x03002920
_0801DEC4: .4byte 0x0300528C
_0801DEC8:
	movs r0, #0x56
	bl m4aSongNumStart
	ldrb r0, [r6, #0x08]
	cmp r0, #0x09
	bls _0801DF02
	ldr r2, _0801DFBC @ =0x0300528C
	ldrb r1, [r2, #0x00]
	adds r1, r5, r1
	lsls r0, r1, #0x03
	subs r0, r0, r1
	lsls r0, r0, #0x02
	adds r0, r0, r7
	mov r1, r8
	strb r1, [r0, #0x0F]
	ldrb r4, [r2, #0x00]
	adds r4, r5, r4
	lsls r4, r4, #0x18
	lsrs r4, r4, #0x18
	ldrb r0, [r6, #0x08]
	movs r1, #0x0A
	bl FUN_08051a0c
	adds r1, r0, #0x0
	lsls r1, r1, #0x18
	lsrs r1, r1, #0x18
	adds r0, r4, #0x0
	bl LoadSpriteFrame
_0801DF02:
	ldrb r0, [r6, #0x08]
	movs r1, #0x0A
	bl FUN_08051a84
	adds r1, r0, #0x0
	lsls r1, r1, #0x18
	lsrs r1, r1, #0x18
	adds r0, r5, #0x0
	bl LoadSpriteFrame
	ldrb r0, [r6, #0x08]
	cmp r0, #0x09
	bne _0801DF3E
	ldr r2, _0801DFBC @ =0x0300528C
	ldrb r1, [r2, #0x00]
	adds r1, r5, r1
	lsls r0, r1, #0x03
	subs r0, r0, r1
	lsls r0, r0, #0x02
	adds r0, r0, r7
	movs r1, #0x1C
	strb r1, [r0, #0x0F]
	ldrb r1, [r2, #0x00]
	adds r1, r5, r1
	lsls r0, r1, #0x03
	subs r0, r0, r1
	lsls r0, r0, #0x02
	adds r0, r0, r7
	mov r1, r8
	strb r1, [r0, #0x10]
_0801DF3E:
	ldr r3, _0801DFC0 @ =0x03002920
	lsls r0, r5, #0x03
	subs r0, r0, r5
	lsls r0, r0, #0x02
	adds r4, r0, r3
	ldr r6, _0801DFBC @ =0x0300528C
	ldrb r1, [r6, #0x00]
	subs r1, r5, r1
	lsls r0, r1, #0x03
	subs r0, r0, r1
	lsls r0, r0, #0x02
	adds r0, r0, r3
	ldrh r0, [r0, #0x00]
	strh r0, [r4, #0x00]
	ldrb r1, [r6, #0x00]
	subs r1, r5, r1
	lsls r0, r1, #0x03
	subs r0, r0, r1
	lsls r0, r0, #0x02
	adds r0, r0, r3
	ldrh r0, [r0, #0x02]
	subs r0, #0x20
	strh r0, [r4, #0x02]
	ldrb r0, [r4, #0x08]
	cmp r0, #0x09
	bls _0801DFB0
	ldrb r2, [r6, #0x00]
	adds r0, r5, r2
	lsls r1, r0, #0x03
	subs r1, r1, r0
	lsls r1, r1, #0x02
	adds r1, r1, r3
	subs r2, r5, r2
	lsls r0, r2, #0x03
	subs r0, r0, r2
	lsls r0, r0, #0x02
	adds r0, r0, r3
	ldrh r0, [r0, #0x00]
	subs r0, #0x03
	strh r0, [r1, #0x00]
	ldrb r2, [r6, #0x00]
	adds r0, r5, r2
	lsls r1, r0, #0x03
	subs r1, r1, r0
	lsls r1, r1, #0x02
	adds r1, r1, r3
	subs r2, r5, r2
	lsls r0, r2, #0x03
	subs r0, r0, r2
	lsls r0, r0, #0x02
	adds r0, r0, r3
	ldrh r0, [r0, #0x02]
	subs r0, #0x20
	strh r0, [r1, #0x02]
	ldrh r0, [r4, #0x00]
	adds r0, #0x03
	strh r0, [r4, #0x00]
_0801DFB0:
	pop {r3}
	mov r8, r3
	pop {r4, r5, r6, r7}
	pop {r0}
	bx r0
	lsls r0, r0, #0x00
_0801DFBC: .4byte 0x0300528C
_0801DFC0: .4byte 0x03002920
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
