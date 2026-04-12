You are decompiling an assembly function called `ply_lfos` in ARMv4T from a Game Boy Advance game.

# Examples

## `ReadUnalignedU32`

```c
u32 ReadUnalignedU32(u8 *ptr) {
    return ptr[0] + (ptr[1] << 8) + (ptr[2] << 16) + (ptr[3] << 24);
}
```

```asm
         mov r2, r0
         ldrb r0, [r2, #0x0]
         ldrb r1, [r2, #0x1]
         lsl r1, #0x8
         add r0, r1
         ldrb r1, [r2, #0x2]
         lsl r1, #0x10
         add r0, r1
         ldrb r1, [r2, #0x3]
         lsl r1, #0x18
         add r0, r1
         bx lr
```

## `ReadUnalignedS16`

```c
s16 ReadUnalignedS16(u8 *ptr) {
    return (s16)(ptr[0] + (ptr[1] << 8));
}
```

```asm
         mov r1, r0
         ldrb r0, [r1, #0x0]
         ldrb r1, [r1, #0x1]
         lsl r1, #0x8
         add r0, r1
         lsl r0, #0x10
         asr r0, #0x10
         bx lr
```

## `ReadUnalignedU16`

```c
u32 ReadUnalignedU16(u8 *ptr) {
    return ptr[0] | (ptr[1] << 8);
}
```

```asm
         mov r1, r0
         ldrb r0, [r1, #0x0]
         ldrb r1, [r1, #0x1]
         lsl r1, #0x8
         orr r0, r1
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

## `IsSelectButtonPressed`

```c
u32 IsSelectButtonPressed(void) {
    if (gKeysPrevious & 0x40)
        return 1;
    return 0;
}
```

```asm
         ldr r0, [pc, #0xc] # REFERENCE_.L10
         ldrh r1, [r0, #0x0]
         mov r0, #0x40
         and r0, r1
         cmp r0, #0x0
         bne .L149
         mov r0, #0x0
         b .L1610
         .word 0x30051e4
     5mov r0, #0x1
     7bx  lr
```









# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/m4a/ply_lfos.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	non_word_aligned_thumb_func_start ply_lfos
ply_lfos: @ 08051402
	ldr r0, [r1, #0x40]
	ldrb r2, [r0, #0x00]
	strb r2, [r1, #0x1F]
	adds r0, #0x01
	str r0, [r1, #0x40]
	bx lr
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
