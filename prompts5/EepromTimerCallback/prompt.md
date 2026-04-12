You are decompiling an assembly function called `EepromTimerCallback` in ARMv4T from a Game Boy Advance game.

# Examples

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

## `UpdateFadeEffect`

```c
void UpdateFadeEffect(void) {
    vu8 *vcount_reg = (vu8 *)0x04000006;
    u8 *entity = (u8 *)0x03002920;
    u8 fade = FUN_08051a0c(*vcount_reg, entity[0x08]);

    if (fade <= 16) {
        *(vu16 *)0x04000052 = ((u32)fade << 8) | fade;
    }
}
```

```asm
         push {lr}
         ldr r0, [pc, #0x20] # REFERENCE_.L24
         ldr r1, [pc, #0x20] # REFERENCE_.L28
         ldrb r0, [r0, #0x0]
         ldrb r1, [r1, #0x8]
         bl FUN_08051a0c-0x4
         lsl r0, #0x18
         lsr r2, r0, #0x18
         cmp r2, #0x10
         bhi .L1e14
         ldr r1, [pc, #0x14] # REFERENCE_.L2c
         lsl r0, r2, #0x8
         orr r0, r2
         strh r0, [r1, #0x0]
     9pop {r0}
         bx r0
         .hword 0x0
         .word 0x4000006
         .word 0x3002920
         .word 0x4000052
```









# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/util/EepromTimerCallback.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	non_word_aligned_thumb_func_start EepromTimerCallback
EepromTimerCallback: @ 080514B2
	ldr r1, [pc, #0x018] @ =0x0300037A
	ldrh r0, [r1, #0x00]
	cmp r0, #0x00
	.2byte 0xD008 @ beq _080514CA
	ldrh r0, [r1, #0x00]
	subs r0, #0x01
	strh r0, [r1, #0x00]
	lsls r0, r0, #0x10
	cmp r0, #0x00
	.2byte 0xD102 @ bne _080514CA
	ldr r1, [pc, #0x008] @ =0x0300037C
	movs r0, #0x01
	strb r0, [r1, #0x00]
	bx lr
	.4byte 0x0300037A
	.4byte 0x0300037C
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
