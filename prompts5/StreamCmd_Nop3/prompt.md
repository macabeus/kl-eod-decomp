You are decompiling an assembly function called `StreamCmd_Nop3` in ARMv4T from a Game Boy Advance game.

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

## `WritePaletteColor`

```c
void WritePaletteColor(void) {
    u8 **gp = (u8 **)0x03004D84;
    u16 color = ReadUnalignedU16(*gp + 3);
    u8 *ptr = *gp;
    *(u16 *)(BG_PAL_RAM + ptr[2] * 2) = color;
    *gp = ptr + 5;
}
```

```asm
         push {r4, lr}
         ldr r4, [pc, #0x20] # REFERENCE_.L24
         ldr r0, [r4, #0x0]
         add r0, #0x3
         bl ReadUnalignedU16-0x4
         ldr r3, [r4, #0x0]
         ldrb r1, [r3, #0x2]
         lsl r1, #0x1
         mov r2, #0xa0
         lsl r2, #0x13
         add r1, r2
         strh r0, [r1, #0x0]
         add r3, #0x5
         str r3, [r4, #0x0]
         pop {r4}
         pop {r0}
         bx r0
         .word 0x3004d84
```

## `PlaySoundWithContext_DC`

```c
void PlaySoundWithContext_DC(u32 soundId) {
    FUN_0805186c(soundId, gMPlayInfo_SE);
}
```

```asm
         push {lr}
         ldr r1, [pc, #0xc] # REFERENCE_.L10
         ldr r1, [r1, #0x0]
         bl FUN_0805186c-0x4
         pop {r0}
         bx r0
         .hword 0x0
         .word 0x30064dc
```

## `PlaySoundWithContext_D8`

```c
void PlaySoundWithContext_D8(u32 soundId) {
    FUN_0805186c(soundId, gMPlayInfo_BGM);
}
```

```asm
         push {lr}
         ldr r1, [pc, #0xc] # REFERENCE_.L10
         ldr r1, [r1, #0x0]
         bl FUN_0805186c-0x4
         pop {r0}
         bx r0
         .hword 0x0
         .word 0x30064d8
```









# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/gfx/StreamCmd_Nop3.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	non_word_aligned_thumb_func_start StreamCmd_Nop3
StreamCmd_Nop3: @ 0804E7E6
	.4byte 0x03004D84
	ldr r1, [pc, #0x008] @ =0x03004D84
	ldr r0, [r1, #0x00]
	adds r0, #0x03
	str r0, [r1, #0x00]
	bx lr
	lsls r0, r0, #0x00
	.4byte 0x03004D84
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
