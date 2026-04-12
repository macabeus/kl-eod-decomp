You are decompiling an assembly function called `StreamCmd_DmaSpriteData` in ARMv4T from a Game Boy Advance game.

# Examples

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

## `DispatchStreamCommand_C0EC`

```c
void DispatchStreamCommand_C0EC(void) {
    u8 **gp = (u8 **)0x03004D84;
    u8 *ptr = *gp;
    u8 byte = ptr[2];
    u8 val = byte & 0x7F;
    u8 flag = byte >> 7;
    *gp = ptr + 3;
    LoadGfxStreamEntry(val, flag);
}
```

```asm
         push {lr}
         ldr r3, [pc, #0x18] # REFERENCE_.L1c
         ldr r2, [r3, #0x0]
         ldrb r1, [r2, #0x2]
         mov r0, #0x7f
         and r0, r1
         lsr r1, #0x7
         add r2, #0x3
         str r2, [r3, #0x0]
         bl LoadGfxStreamEntry-0x4
         pop {r0}
         bx r0
         .hword 0x0
         .word 0x3004d84
```

## `GameUpdate`

```c
void GameUpdate(void) {
    if (gPauseFlag == 0) {
        UpdateWorldMapNodeState();
        UpdatePlayerInput();
        InitPlayerCollision();
        UpdateWorldMapCursor();
    }
    UpdatePaletteAnimations();
}
```

```asm
         push {lr}
         ldr r0, [pc, #0x20] # REFERENCE_.L24
         ldrb r0, [r0, #0x0]
         cmp r0, #0x0
         bne .L1a9
         bl UpdateWorldMapNodeState-0x4
         bl UpdatePlayerInput-0x4
         bl InitPlayerCollision-0x4
         bl UpdateWorldMapCursor-0x4
     4bl  UpdatePaletteAnimations-0x4
         pop {r0}
         bx r0
         .hword 0x0
         .word 0x30034e4
```

## `ShutdownGfxSubsystem`

```c
void ShutdownGfxSubsystem(void) {
    vu32 *dest = (vu32 *)0x03000814;
    u32 *src = (u32 *)0x03004C20;
    *dest = src[1];

    *(vu16 *)0x04000200 &= 0xFFFD; /* REG_IE &= ~INT_HBLANK */
    *(vu16 *)0x04000004 &= 0xFFEF; /* REG_DISPSTAT &= ~HBLANK_IRQ */

    ShutdownGfxStream();
    FreeSoundStruct();
    FreeBuffer_52A4();
    FreeGfxBuffer();
    FreeDecompStreamBuffer();
    m4aMPlayAllStop();
}
```

```asm
         push {lr}
         ldr r1, [pc, #0x38] # REFERENCE_.L3c
         ldr r0, [pc, #0x38] # REFERENCE_.L40
         ldr r0, [r0, #0x4]
         str r0, [r1, #0x0]
         ldr r2, [pc, #0x38] # REFERENCE_.L44
         ldrh r1, [r2, #0x0]
         ldr r0, [pc, #0x38] # REFERENCE_.L48
         and r0, r1
         strh r0, [r2, #0x0]
         ldr r2, [pc, #0x34] # REFERENCE_.L4c
         ldrh r1, [r2, #0x0]
         ldr r0, [pc, #0x34] # REFERENCE_.L50
         and r0, r1
         strh r0, [r2, #0x0]
         bl ShutdownGfxStream-0x4
         bl FreeSoundStruct-0x4
         bl FreeBuffer_52A4-0x4
         bl FreeGfxBuffer-0x4
         bl FreeDecompStreamBuffer-0x4
         bl m4aMPlayAllStop-0x4
         pop {r0}
         bx r0
         .hword 0x0
         .word 0x3000814
         .word 0x3004c20
         .word 0x4000200
         .word 0xfffd
         .word 0x4000004
         .word 0xffef
```

## `StreamCmd_ResetEntries`

```c
void StreamCmd_ResetEntries(void) {
    ResetGfxStreamEntries();
    gStreamPtr += 2;
}
```

```asm
         push {lr}
         bl ResetGfxStreamEntries-0x4
         ldr r1, [pc, #0xc] # REFERENCE_.L14
         ldr r0, [r1, #0x0]
         add r0, #0x2
         str r0, [r1, #0x0]
         pop {r0}
         bx r0
         .hword 0x0
         .word 0x3004d84
```









# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/gfx/StreamCmd_DmaSpriteData.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	non_word_aligned_thumb_func_start StreamCmd_DmaSpriteData
StreamCmd_DmaSpriteData: @ 0804C1FC
	push {lr}
	ldr r3, [pc, #0x014] @ =0x03004D84
	ldr r2, [r3, #0x00]
	ldrb r0, [r2, #0x02]
	ldrb r1, [r2, #0x03]
	adds r2, #0x04
	str r2, [r3, #0x00]
	bl DmaSpriteToObjVram
	pop {r0}
	bx r0
	lsls r0, r0, #0x00
	.4byte 0x03004D84
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
