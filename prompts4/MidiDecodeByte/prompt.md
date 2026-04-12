You are decompiling an assembly function called `MidiDecodeByte` in ARMv4T from a Game Boy Advance game.

# Examples

## `MPlayMain_SetAndProcess`

```c
u32 MPlayMain_SetAndProcess(u32 val) {
    u8 *info = (u8 *)gSoundInfo;
    info[0x0D] = val;
    return MidiProcessEvent();
}
```

```asm
         push {lr}
         ldr r1, [pc, #0xc] # REFERENCE_.L10
         ldr r1, [r1, #0x0]
         strb r0, [r1, #0xd]
         bl MidiProcessEvent-0x4
         pop {r1}
         bx r1
         .word 0x300081c
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

## `SetupGfxCallbacks`

```c
void SetupGfxCallbacks(void) {
    u32 *vblankHandlers = (u32 *)gVBlankCallbackArray;
    u32 *callbackState;
    u32 slotIdx;
    vblankHandlers[0] = (u32)VBlankHandler_WithWindowScroll;
    vblankHandlers[1] = (u32)UpdateBGScrollWithWave;

    callbackState = (u32 *)0x03003510;
    callbackState[0x28 / 4] = (u32)ReadKeyInput;
    callbackState[0x2C / 4] = (u32)SoundMain;
    callbackState[0x30 / 4] = (u32)VBlankCallback_MapScreen;
    callbackState[0x34 / 4] = 1;
    slotIdx = *((u8 *)callbackState + 0x78) - 1;
    callbackState[slotIdx] = 0;
    *((u8 *)callbackState + 0x79) = 4;
}
```

```asm
         ldr r1, [pc, #0x30] # REFERENCE_.L34
         ldr r0, [pc, #0x34] # REFERENCE_.L38
         str r0, [r1, #0x0]
         ldr r0, [pc, #0x34] # REFERENCE_.L3c
         str r0, [r1, #0x4]
         ldr r2, [pc, #0x34] # REFERENCE_.L40
         ldr r0, [pc, #0x34] # REFERENCE_.L44
         str r0, [r2, #0x28]
         ldr r0, [pc, #0x34] # REFERENCE_.L48
         str r0, [r2, #0x2c]
         ldr r0, [pc, #0x34] # REFERENCE_.L4c
         str r0, [r2, #0x30]
         mov r0, #0x1
         str r0, [r2, #0x34]
         mov r0, r2
         add r0, #0x78
         ldrb r0, [r0, #0x0]
         sub r0, #0x1
         lsl r0, #0x2
         add r0, r2
         mov r1, #0x0
         str r1, [r0, #0x0]
         add r2, #0x79
         mov r0, #0x4
         strb r0, [r2, #0x0]
         bx lr
         .word gVBlankCallbackArray
         .word VBlankHandler_WithWindowScroll
         .word UpdateBGScrollWithWave
         .word 0x3003510
         .word ReadKeyInput
         .word SoundMain
         .word VBlankCallback_MapScreen
```

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









# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/m4a/MidiDecodeByte.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	thumb_func_start MidiDecodeByte
MidiDecodeByte: @ 0804F758
	ldr r2, [r1, #0x40]
.global FUN_0804f75a
	.thumb_func
FUN_0804f75a: @ 0804F75A
	adds r3, r2, #0x1
	str r3, [r1, #0x40]
	ldrb r3, [r2, #0x00]
	b MidiNoteDispatch
	lsls r0, r0, #0x00
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
