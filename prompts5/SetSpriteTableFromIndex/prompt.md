You are decompiling an assembly function called `SetSpriteTableFromIndex` in ARMv4T from a Game Boy Advance game.

# Examples

## `ply_vol`

```c
void ply_vol(void *r0, TrackStruct *track) {
    track->unk2D = *track->unk40;
    track->unk40++;
}
```

```asm
         ldr r0, [r1, #0x40]
         ldrb r0, [r0, #0x0]
         mov r2, r1
         add r2, #0x2d
         strb r0, [r2, #0x0]
         ldr r0, [r1, #0x40]
         add r0, #0x1
         str r0, [r1, #0x40]
         bx lr
```

## `ply_pan`

```c
void ply_pan(void *r0, TrackStruct *track) {
    track->unk2E = *track->unk40;
    track->unk40++;
}
```

```asm
         ldr r0, [r1, #0x40]
         ldrb r0, [r0, #0x0]
         mov r2, r1
         add r2, #0x2e
         strb r0, [r2, #0x0]
         ldr r0, [r1, #0x40]
         add r0, #0x1
         str r0, [r1, #0x40]
         bx lr
```

## `StreamCmd_SetWindowRegs`

```c
void StreamCmd_SetWindowRegs(void) {
    vu16 *reg = (vu16 *)0x04000048;
    u8 **streamPtrAddr = (u8 **)0x03004D84;
    u8 *stream = *streamPtrAddr;

    *reg = stream[2] | (stream[3] << 8);
    reg++;
    *reg = stream[4] | (stream[5] << 8);
    *streamPtrAddr = stream + 6;
}
```

```asm
         push {r4, lr}
         ldr r3, [pc, #0x24] # REFERENCE_.L28
         ldr r4, [pc, #0x24] # REFERENCE_.L2c
         ldr r2, [r4, #0x0]
         ldrb r1, [r2, #0x2]
         ldrb r0, [r2, #0x3]
         lsl r0, #0x8
         orr r1, r0
         strh r1, [r3, #0x0]
         add r3, #0x2
         ldrb r1, [r2, #0x4]
         ldrb r0, [r2, #0x5]
         lsl r0, #0x8
         orr r1, r0
         strh r1, [r3, #0x0]
         add r2, #0x6
         str r2, [r4, #0x0]
         pop {r4}
         pop {r0}
         bx r0
         .word 0x4000048
         .word 0x3004d84
```

## `ply_lfos`

```c
void ply_lfos(void *r0, TrackStruct *r1) {
    u8 *ptr;
    ptr = r1->unk40;
    r1->unk1F = *ptr;
    r1->unk40 = ptr + 1;
}
```

```asm
         ldr r0, [r1, #0x40]
         ldrb r2, [r0, #0x0]
         strb r2, [r1, #0x1f]
         add r0, #0x1
         str r0, [r1, #0x40]
         bx lr
```

## `StreamCmd_EnableMosaic`

```c
void StreamCmd_EnableMosaic(void) {
    vu16 *bg2cnt = (vu16 *)0x0400000C;
    *bg2cnt |= 0x40;
    bg2cnt++;
    *bg2cnt |= 0x40;

    *(u8 *)0x030007D8 = gStreamPtr[2] & 0x0F;
    gStreamPtr += 3;
}
```

```asm
         push {r4, lr}
         ldr r1, [pc, #0x28] # REFERENCE_.L2c
         ldrh r0, [r1, #0x0]
         mov r2, #0x40
         orr r0, r2
         strh r0, [r1, #0x0]
         add r1, #0x2
         ldrh r0, [r1, #0x0]
         orr r0, r2
         strh r0, [r1, #0x0]
         ldr r4, [pc, #0x18] # REFERENCE_.L30
         ldr r3, [pc, #0x1c] # REFERENCE_.L34
         ldr r1, [r3, #0x0]
         ldrb r2, [r1, #0x2]
         mov r0, #0xf
         and r0, r2
         strb r0, [r4, #0x0]
         add r1, #0x3
         str r1, [r3, #0x0]
         pop {r4}
         pop {r0}
         bx r0
         .word 0x400000c
         .word 0x30007d8
         .word 0x3004d84
```

# Functions that call the target assembly

## `ProcessStreamCommand_C218`

```c
void ProcessStreamCommand_C218(void) {
    SetSpriteTableFromIndex(gStreamPtr[2]);
    gStreamPtr += 3;
}
```

```asm
         push {r4, lr}
         ldr r4, [pc, #0x14] # REFERENCE_.L18
         ldr r0, [r4, #0x0]
         ldrb r0, [r0, #0x2]
         bl SetSpriteTableFromIndex-0x4
         ldr r0, [r4, #0x0]
         add r0, #0x3
         str r0, [r4, #0x0]
         pop {r4}
         pop {r0}
         bx r0
         .word 0x3004d84
```







# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/gfx/SetSpriteTableFromIndex.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	thumb_func_start SetSpriteTableFromIndex
SetSpriteTableFromIndex: @ 0804C218
	ldr r2, _0804C228 @ =0x030051DC
	ldr r1, _0804C22C @ =0x08189E84
	lsls r0, r0, #0x02
	adds r0, r0, r1
	ldr r0, [r0, #0x00]
	str r0, [r2, #0x00]
	bx lr
	lsls r0, r0, #0x00
_0804C228: .4byte 0x030051DC
_0804C22C: .4byte 0x08189E84
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
