You are decompiling an assembly function called `FreeGfxBuffer` in ARMv4T from a Game Boy Advance game.

# Examples

## `FreeBuffer_52A4`

```c
void FreeBuffer_52A4(void) {
    thunk_FUN_0800020c(gBuffer_52A4);
}
```

```asm
         push {lr}
         ldr r0, [pc, #0xc] # REFERENCE_.L10
         ldr r0, [r0, #0x0]
         bl thunk_FUN_0800020c-0x4
         pop {r0}
         bx r0
         .hword 0x0
         .word 0x30052a4
```

## `FreeDecompStreamBuffer`

```c
void FreeDecompStreamBuffer(void) {
    thunk_FUN_0800020c(gDecompBuffer);
}
```

```asm
         push {lr}
         ldr r0, [pc, #0xc] # REFERENCE_.L10
         ldr r0, [r0, #0x0]
         bl thunk_FUN_0800020c-0x4
         pop {r0}
         bx r0
         .hword 0x0
         .word 0x30007d0
```

## `FreeSoundStruct`

```c
void FreeSoundStruct(void) {
    u32 *p = (u32 *)0x0300081C; /* &gSoundInfo */
    thunk_FUN_0800020c(*(u32 *)(*p) - 4);
    thunk_FUN_0800020c(*p);
}
```

```asm
         push {r4, lr}
         ldr r4, [pc, #0x18] # REFERENCE_.L1c
         ldr r0, [r4, #0x0]
         ldr r0, [r0, #0x0]
         sub r0, #0x4
         bl thunk_FUN_0800020c-0x4
         ldr r0, [r4, #0x0]
         bl thunk_FUN_0800020c-0x4
         pop {r4}
         pop {r0}
         bx r0
         .hword 0x0
         .word 0x300081c
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

# Functions that call the target assembly

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



# Declarations for the functions called from the target assembly

- `void thunk_FUN_0800020c(u32);`



# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/gfx/FreeGfxBuffer.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	thumb_func_start FreeGfxBuffer
FreeGfxBuffer: @ 0804BB74
	push {lr}
	ldr r0, _0804BB84 @ =0x030034A0
	ldr r0, [r0, #0x00]
	bl thunk_FUN_0800020c
	pop {r0}
	bx r0
	lsls r0, r0, #0x00
_0804BB84:
	.byte 0xA0, 0x34
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
