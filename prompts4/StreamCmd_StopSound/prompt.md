You are decompiling an assembly function called `StreamCmd_StopSound` in ARMv4T from a Game Boy Advance game.

# Examples

## `EnableVBlankAndHandlers`

```c
void EnableVBlankAndHandlers(void) {
    REG_IE |= IE_VBLANK;
    REG_DISPSTAT |= DISPSTAT_VBLANK_IRQ_ENABLE;
    m4aSoundVSyncOn();
    StopSoundEffects();
    gStreamPtr += 2;
}
```

```asm
         push {lr}
         ldr r2, [pc, #0x28] # REFERENCE_.L2c
         ldrh r0, [r2, #0x0]
         mov r1, #0x1
         orr r0, r1
         strh r0, [r2, #0x0]
         ldr r2, [pc, #0x20] # REFERENCE_.L30
         ldrh r0, [r2, #0x0]
         mov r1, #0x8
         orr r0, r1
         strh r0, [r2, #0x0]
         bl m4aSoundVSyncOn-0x4
         bl StopSoundEffects-0x4
         ldr r1, [pc, #0x14] # REFERENCE_.L34
         ldr r0, [r1, #0x0]
         add r0, #0x2
         str r0, [r1, #0x0]
         pop {r0}
         bx r0
         .hword 0x0
         .word 0x4000200
         .word 0x4000004
         .word 0x3004d84
```

## `StreamCmd_StopMusicAndDisableIRQ`

```c
void StreamCmd_StopMusicAndDisableIRQ(void) {
    m4aMPlayAllStop();
    m4aSoundVSyncOff();
    gStreamPtr += 2;
}
```

```asm
         push {lr}
         bl m4aMPlayAllStop-0x4
         bl m4aSoundVSyncOff-0x4
         ldr r1, [pc, #0xc] # REFERENCE_.L18
         ldr r0, [r1, #0x0]
         add r0, #0x2
         str r0, [r1, #0x0]
         pop {r0}
         bx r0
         .hword 0x0
         .word 0x3004d84
```

## `StreamCmd_DisableVBlank`

```c
void StreamCmd_DisableVBlank(void) {
    *(vu16 *)0x04000200 &= 0xFFFE; /* REG_IE &= ~INT_VBLANK */
    *(vu16 *)0x04000004 &= 0xFFF7; /* REG_DISPSTAT &= ~VBLANK_IRQ */
    m4aSoundVSyncOff();
    gStreamPtr += 2;
}
```

```asm
         push {lr}
         ldr r2, [pc, #0x24] # REFERENCE_.L28
         ldrh r1, [r2, #0x0]
         ldr r0, [pc, #0x24] # REFERENCE_.L2c
         and r0, r1
         strh r0, [r2, #0x0]
         ldr r2, [pc, #0x20] # REFERENCE_.L30
         ldrh r1, [r2, #0x0]
         ldr r0, [pc, #0x20] # REFERENCE_.L34
         and r0, r1
         strh r0, [r2, #0x0]
         bl m4aSoundVSyncOff-0x4
         ldr r1, [pc, #0x1c] # REFERENCE_.L38
         ldr r0, [r1, #0x0]
         add r0, #0x2
         str r0, [r1, #0x0]
         pop {r0}
         bx r0
         .hword 0x0
         .word 0x4000200
         .word 0xfffe
         .word 0x4000004
         .word 0xfff7
         .word 0x3004d84
```

## `MPlayStop`

```c
void MPlayStop(u32 *player) {
    u32 magic = player[0x34 / 4];
    s32 numTracks;
    u8 *track;

    if (magic != 0x68736D53)
        return;

    player[0x34 / 4] = magic + 1;
    player[0x04 / 4] |= 0x80000000;

    numTracks = (s32)(u8)((u8 *)player)[0x08];
    track = (u8 *)player[0x2C / 4];

    while (numTracks > 0) {
        SoundContextRef((u32)player, (u32)track);
        numTracks--;
        track += 0x50;
    }

    player[0x34 / 4] = 0x68736D53;
}
```

```asm
         push {r4, r5, r6, lr}
         mov r6, r0
         ldr r1, [r6, #0x34]
         ldr r0, [pc, #0x34] # REFERENCE_.L3c
         cmp r1, r0
         bne .L3626
         add r0, r1, #0x1
         str r0, [r6, #0x34]
         ldr r0, [r6, #0x4]
         mov r1, #0x80
         lsl r1, #0x18
         orr r0, r1
         str r0, [r6, #0x4]
         ldrb r4, [r6, #0x8]
         ldr r5, [r6, #0x2c]
         cmp r4, #0x0
         ble .L3224
     23mov r0, r6
         mov r1, r5
         bl SoundContextRef-0x4
         sub r4, #0x1
         add r5, #0x50
         cmp r4, #0x0
         bgt .L2217
     16ldr r0, [pc, #0x8] # REFERENCE_.L3c
         str r0, [r6, #0x34]
     5pop {r4, r5, r6}
         pop {r0}
         bx r0
         .word 0x68736d53
```

## `SoundCmd_Dispatch`

```c
void SoundCmd_Dispatch(u32 ctx, u32 *channel) {
    u8 *cmdPtr = (u8 *)channel[0x40 / 4];
    u8 cmd = *cmdPtr;
    channel[0x40 / 4] = (u32)(cmdPtr + 1);
    FUN_08051870(ctx, channel, gSoundCmdTable[cmd]);
}
```

```asm
         push {lr}
         ldr r2, [r1, #0x40]
         ldrb r3, [r2, #0x0]
         add r2, #0x1
         str r2, [r1, #0x40]
         ldr r2, [pc, #0x10] # REFERENCE_.L1c
         lsl r3, #0x2
         add r3, r2
         ldr r2, [r3, #0x0]
         bl FUN_08051870-0x4
         pop {r0}
         bx r0
         .hword 0x0
         .word gSoundCmdTable
```









# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/gfx/StreamCmd_StopSound.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	non_word_aligned_thumb_func_start StreamCmd_StopSound
StreamCmd_StopSound: @ 0804E7D2
	push {lr}
	bl StopSoundEffects
	ldr r1, [pc, #0x00C] @ =0x03004D84
	ldr r0, [r1, #0x00]
	adds r0, #0x02
	str r0, [r1, #0x00]
	pop {r0}
	bx r0
	lsls r0, r0, #0x00
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
