You are decompiling an assembly function called `StrCpy` in ARMv4T from a Game Boy Advance game.

# Examples

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

## `FixedMul8`

```c
s16 FixedMul8(s16 a, s16 b) {
    s32 result = (s32)a * (s32)b;
    register s32 shifted asm("r1") = result;
    if (result < 0)
        shifted += 0xFF;
    return (s16)(shifted >> 8);
}
```

```asm
         lsl r0, #0x10
         asr r0, #0x10
         lsl r1, #0x10
         asr r1, #0x10
         mul r0, r1
         mov r1, r0
         cmp r0, #0x0
         bge .L129
         add r1, #0xff
     7lsl r0, r1, #0x8
         asr r0, #0x10
         bx lr
```









# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/system/StrCpy.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	thumb_func_start StrCpy
StrCpy: @ 08000460
	ldrb r2, [r1, #0x00]
	strb r2, [r0, #0x00]
	adds r0, #0x01
	adds r1, #0x01
	cmp r2, #0x00
	bne StrCpy
	bx lr
	lsls r0, r0, #0x00
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
