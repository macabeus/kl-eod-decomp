You are decompiling an assembly function called `UpdateWorldMapNodeTile` in ARMv4T from a Game Boy Advance game.



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

Called by `UpdateEntities` for each completed world. Places 4 specific tile values into the world map tilemap buffer to display the node icon for that world. Branches on `world <= 3` vs `world > 3`, using slightly different source tile offsets for each case.

For each of the 4 tiles, it: reads a (column, row) coordinate pair from the ROM table, computes a destination address in the tilemap buffer as `base + (row * 32 + column) * 2`, reads a source tile from a different row in the buffer (offset by the world's base tile row from the lookup table), and writes it to the destination.

## Memory map (relevant addresses)

- `0x03000900` — `gScreenBufferA`: EWRAM tilemap buffer base. With offset `+0x800` (`movs r0, #0x80; lsls r0, r0, #0x04`) = `0x03001100`, this is the active world map tile area.
- `0x08116748` — ROM data table: 8-byte entries per world. Each entry has 4 pairs of (column, row) tile coordinates. Entry for world `i` at `0x08116748 + i * 8`.
- `0x08116880` — ROM lookup table: 1 byte per world. Maps world index to a base tile row offset (added to `0x1C` = 28).

## System architecture

7 worlds (indices 0–6). `UpdateEntities` calls `CheckWorldCompletion(i)` → `CopyWorldMapTiles(i)` → `UpdateWorldMapNodeTile(i)`. `CopyWorldMapTiles` copies the base tile block; this function overlays the 4 node icon tiles on top.

# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/code_3/UpdateWorldMapNodeTile.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	thumb_func_start UpdateWorldMapNodeTile
UpdateWorldMapNodeTile: @ 0803AF38
	push {r4, r5, r6, lr}
	add sp, #-0x004
	lsls r0, r0, #0x18
	lsrs r3, r0, #0x18
	cmp r3, #0x03
	bhi _0803AFD4
	ldr r6, _0803AFC8 @ =0x03000900
	ldr r4, _0803AFCC @ =0x08116748
	lsls r5, r3, #0x03
	adds r2, r5, r4
	adds r0, r4, #0x1
	adds r0, r5, r0
	ldrb r1, [r0, #0x00]
	lsls r1, r1, #0x05
	ldrb r2, [r2, #0x00]
	adds r1, r1, r2
	lsls r1, r1, #0x01
	movs r0, #0x80
	lsls r0, r0, #0x04
	adds r6, r6, r0
	adds r1, r1, r6
	ldr r0, _0803AFD0 @ =0x08116880
	adds r0, r3, r0
	ldrb r3, [r0, #0x00]
	adds r3, #0x1C
	lsls r0, r3, #0x06
	adds r0, r0, r6
	ldrh r0, [r0, #0x00]
	strh r0, [r1, #0x00]
	adds r2, r4, #0x2
	adds r2, r5, r2
	adds r0, r4, #0x3
	adds r0, r5, r0
	ldrb r1, [r0, #0x00]
	lsls r1, r1, #0x05
	ldrb r2, [r2, #0x00]
	adds r1, r1, r2
	lsls r1, r1, #0x01
	adds r1, r1, r6
	lsls r3, r3, #0x05
	adds r0, r3, #0x1
	lsls r0, r0, #0x01
	adds r0, r0, r6
	ldrh r0, [r0, #0x00]
	strh r0, [r1, #0x00]
	adds r2, r4, #0x4
	adds r2, r5, r2
	adds r0, r4, #0x5
	adds r0, r5, r0
	ldrb r1, [r0, #0x00]
	lsls r1, r1, #0x05
	ldrb r2, [r2, #0x00]
	adds r1, r1, r2
	lsls r1, r1, #0x01
	adds r1, r1, r6
	adds r0, r3, #0x2
	lsls r0, r0, #0x01
	adds r0, r0, r6
	ldrh r0, [r0, #0x00]
	strh r0, [r1, #0x00]
	adds r1, r4, #0x6
	adds r1, r5, r1
	adds r4, #0x07
	adds r5, r5, r4
	ldrb r0, [r5, #0x00]
	lsls r0, r0, #0x05
	ldrb r1, [r1, #0x00]
	adds r0, r0, r1
	lsls r0, r0, #0x01
	adds r0, r0, r6
	adds r3, #0x03
	b _0803B058
_0803AFC8: .4byte 0x03000900
_0803AFCC: .4byte 0x08116748
_0803AFD0: .4byte 0x08116880
_0803AFD4:
	ldr r6, _0803B068 @ =0x03000900
	ldr r4, _0803B06C @ =0x08116748
	lsls r5, r3, #0x03
	adds r2, r5, r4
	adds r0, r4, #0x1
	adds r0, r5, r0
	ldrb r1, [r0, #0x00]
	lsls r1, r1, #0x05
	ldrb r2, [r2, #0x00]
	adds r1, r1, r2
	lsls r1, r1, #0x01
	movs r0, #0x80
	lsls r0, r0, #0x04
	adds r6, r6, r0
	adds r1, r1, r6
	ldr r0, _0803B070 @ =0x08116880
	adds r0, r3, r0
	ldrb r3, [r0, #0x00]
	adds r3, #0x1C
	lsls r3, r3, #0x05
	adds r0, r3, #0x4
	lsls r0, r0, #0x01
	adds r0, r0, r6
	ldrh r0, [r0, #0x00]
	strh r0, [r1, #0x00]
	adds r2, r4, #0x2
	adds r2, r5, r2
	adds r0, r4, #0x3
	adds r0, r5, r0
	ldrb r1, [r0, #0x00]
	lsls r1, r1, #0x05
	ldrb r2, [r2, #0x00]
	adds r1, r1, r2
	lsls r1, r1, #0x01
	adds r1, r1, r6
	adds r0, r3, #0x5
	lsls r0, r0, #0x01
	adds r0, r0, r6
	ldrh r0, [r0, #0x00]
	strh r0, [r1, #0x00]
	adds r2, r4, #0x4
	adds r2, r5, r2
	adds r0, r4, #0x5
	adds r0, r5, r0
	ldrb r1, [r0, #0x00]
	lsls r1, r1, #0x05
	ldrb r2, [r2, #0x00]
	adds r1, r1, r2
	lsls r1, r1, #0x01
	adds r1, r1, r6
	adds r0, r3, #0x6
	lsls r0, r0, #0x01
	adds r0, r0, r6
	ldrh r0, [r0, #0x00]
	strh r0, [r1, #0x00]
	adds r1, r4, #0x6
	adds r1, r5, r1
	adds r4, #0x07
	adds r5, r5, r4
	ldrb r0, [r5, #0x00]
	lsls r0, r0, #0x05
	ldrb r1, [r1, #0x00]
	adds r0, r0, r1
	lsls r0, r0, #0x01
	adds r0, r0, r6
	adds r3, #0x07
_0803B058:
	lsls r3, r3, #0x01
	adds r3, r3, r6
	ldrh r1, [r3, #0x00]
	strh r1, [r0, #0x00]
	add sp, #0x004
	pop {r4, r5, r6}
	pop {r0}
	bx r0
_0803B068: .4byte 0x03000900
_0803B06C: .4byte 0x08116748
_0803B070: .4byte 0x08116880
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
