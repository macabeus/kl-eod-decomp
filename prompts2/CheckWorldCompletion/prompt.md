You are decompiling an assembly function called `CheckWorldCompletion` in ARMv4T from a Game Boy Advance game.



# Functions that call the target assembly

## `UpdateEntities`

```c
void UpdateEntities(void) {
    u8 i;
    for (i = 0; i <= 6; i++) {
        if ((u8)CheckWorldCompletion(i)) {
            CopyWorldMapTiles(i);
            UpdateWorldMapNodeTile(i);
        }
    }
}
```

```asm
         push {r4, lr}
         mov r4, #0x0
     15mov r0, r4
         bl CheckWorldCompletion-0x4
         lsl r0, #0x18
         cmp r0, #0x0
         beq .L1c11
         mov r0, r4
         bl CopyWorldMapTiles-0x4
         mov r0, r4
         bl UpdateWorldMapNodeTile-0x4
     6add r0, r4, #0x1
         lsl r0, #0x18
         lsr r4, r0, #0x18
         cmp r4, #0x6
         bls .L42
         pop {r4}
         pop {r0}
         bx r0
```







# Context from runtime analysis

## What this function does

Called by `UpdateEntities` for each world (0–6). Returns 1 if the world should display as "completed" on the world map, 0 otherwise. The completion check differs by world index:

- **Worlds 0–3** (`world <= 3`): checks a per-world flag at offset `0x0F + world*8` in the save data struct, then a second flag at offset `0x08 + (world+1)*8`.
- **Worlds 4–6** (`world > 3`): iterates over all 5 worlds × 7 vision slots, counting completed visions (`0x64`) and special visions (`0x1E`), then applies world-specific unlock thresholds.

## Memory map (relevant addresses)

- `0x03004670` — `gCurrentEntityCtx`: pointer to the save/game-progress data struct. Fields accessed at various offsets: `+0x08` (vision completion array base), `+0x0F/+0x17/...` (per-world unlock flags), `+0x30/+0x31/+0x32` (bonus world flags).
- `0x03005220` — `gGameStateArray`: game state array used by the entity system.

## System architecture

The game has 7 worlds (indices 0–6). The world map screen calls `UpdateEntities` which iterates worlds 0–6, calling `CheckWorldCompletion(i)` → `CopyWorldMapTiles(i)` → `UpdateWorldMapNodeTile(i)` for completed worlds. This function is the gate that decides which worlds appear as completed.

# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/code_3/CheckWorldCompletion.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	thumb_func_start CheckWorldCompletion
CheckWorldCompletion: @ 0803AC18
	push {r4, r5, r6, r7, lr}
	mov r7, r10
	mov r6, r9
	mov r5, r8
	push {r5, r6, r7}
	lsls r0, r0, #0x18
	lsrs r0, r0, #0x18
	mov r9, r0
	cmp r0, #0x03
	bhi _0803AC68
	ldr r0, _0803AC64 @ =0x03004670
	ldr r2, [r0, #0x00]
	mov r0, r9
	lsls r1, r0, #0x03
	adds r0, r2, #0x0
	adds r0, #0x0F
	adds r0, r0, r1
	ldrb r1, [r0, #0x00]
	movs r0, #0x80
	ands r0, r1
	cmp r0, #0x00
	bne _0803AC46
	b _0803AD80
_0803AC46:
	mov r1, r9
	adds r1, #0x01
	lsls r1, r1, #0x03
	adds r0, r2, #0x0
	adds r0, #0x08
	adds r0, r0, r1
	ldrb r1, [r0, #0x00]
	movs r0, #0x7F
	ands r0, r1
	cmp r0, #0x7F
	bne _0803AC5E
	b _0803AD80
_0803AC5E:
	movs r0, #0x01
	b _0803AD82
	lsls r0, r0, #0x00
_0803AC64: .4byte 0x03004670
_0803AC68:
	ldr r2, _0803ACB4 @ =0x03004670
	ldr r0, [r2, #0x00]
	adds r0, #0x37
	ldrb r1, [r0, #0x00]
	movs r0, #0x80
	ands r0, r1
	cmp r0, #0x00
	bne _0803AC7A
	b _0803AD80
_0803AC7A:
	movs r1, #0x00
	mov r8, r1
	mov r12, r1
	movs r6, #0x00
	mov r10, r6
	movs r0, #0x00
	adds r4, r2, #0x0
	movs r7, #0x7F
_0803AC8A:
	movs r2, #0x00
	adds r5, r0, #0x1
	lsls r3, r0, #0x03
_0803AC90:
	cmp r2, #0x03
	beq _0803AC98
	cmp r2, #0x05
	bne _0803ACB8
_0803AC98:
	ldr r0, [r4, #0x00]
	adds r0, #0x08
	adds r0, r0, r3
	ldrb r1, [r0, #0x00]
	adds r0, r7, #0x0
	ands r0, r1
	cmp r0, #0x64
	bne _0803ACB8
	mov r0, r8
	adds r0, #0x01
	lsls r0, r0, #0x18
	lsrs r0, r0, #0x18
	mov r8, r0
	b _0803ACD6
_0803ACB4: .4byte 0x03004670
_0803ACB8:
	cmp r2, #0x07
	beq _0803ACD6
	ldr r0, [r4, #0x00]
	adds r0, #0x08
	adds r0, r0, r3
	ldrb r1, [r0, #0x00]
	adds r0, r7, #0x0
	ands r0, r1
	cmp r0, #0x1E
	bne _0803ACD6
	mov r0, r12
	adds r0, #0x01
	lsls r0, r0, #0x18
	lsrs r0, r0, #0x18
	mov r12, r0
_0803ACD6:
	ldr r0, [r4, #0x00]
	adds r0, #0x08
	adds r0, r0, r3
	ldrb r1, [r0, #0x00]
	movs r0, #0x80
	ands r0, r1
	cmp r0, #0x00
	beq _0803ACEC
	adds r0, r6, #0x1
	lsls r0, r0, #0x18
	lsrs r6, r0, #0x18
_0803ACEC:
	adds r3, #0x01
	adds r2, #0x01
	cmp r2, #0x06
	bls _0803AC90
	adds r0, r5, #0x0
	cmp r0, #0x04
	bls _0803AC8A
	ldr r0, _0803AD90 @ =0x03004670
	ldr r1, [r0, #0x00]
	adds r0, r1, #0x0
	adds r0, #0x30
	ldrb r0, [r0, #0x00]
	movs r4, #0x7F
	adds r3, r4, #0x0
	ands r3, r0
	cmp r3, #0x1E
	bne _0803AD18
	mov r0, r10
	adds r0, #0x01
	lsls r0, r0, #0x18
	lsrs r0, r0, #0x18
	mov r10, r0
_0803AD18:
	adds r0, r1, #0x0
	adds r0, #0x31
	ldrb r1, [r0, #0x00]
	adds r0, r4, #0x0
	ands r0, r1
	cmp r0, #0x1E
	bne _0803AD30
	mov r0, r10
	adds r0, #0x01
	lsls r0, r0, #0x18
	lsrs r0, r0, #0x18
	mov r10, r0
_0803AD30:
	mov r1, r9
	cmp r1, #0x04
	bne _0803AD3E
	cmp r3, #0x7F
	beq _0803AD3E
	cmp r6, #0x23
	beq _0803AC5E
_0803AD3E:
	mov r0, r9
	cmp r0, #0x05
	bne _0803AD5E
	ldr r1, _0803AD90 @ =0x03004670
	ldr r0, [r1, #0x00]
	adds r0, #0x31
	ldrb r1, [r0, #0x00]
	movs r0, #0x7F
	ands r0, r1
	cmp r0, #0x7F
	beq _0803AD5E
	mov r0, r12
	add r0, r8
	cmp r0, #0x18
	ble _0803AD5E
	b _0803AC5E
_0803AD5E:
	mov r0, r9
	cmp r0, #0x06
	bne _0803AD80
	ldr r1, _0803AD90 @ =0x03004670
	ldr r0, [r1, #0x00]
	adds r0, #0x32
	ldrb r1, [r0, #0x00]
	movs r0, #0x7F
	ands r0, r1
	cmp r0, #0x7F
	beq _0803AD80
	mov r0, r12
	add r0, r8
	add r0, r10
	cmp r0, #0x25
	bne _0803AD80
	b _0803AC5E
_0803AD80:
	movs r0, #0x00
_0803AD82:
	pop {r3, r4, r5}
	mov r8, r3
	mov r9, r4
	mov r10, r5
	pop {r4, r5, r6, r7}
	pop {r1}
	bx r1
_0803AD90: .4byte 0x03004670
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
