You are decompiling an assembly function called `FixedMul4` in ARMv4T from a Game Boy Advance game.

# Examples

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

## `FixedMulShift4`

```c
s16 FixedMulShift4(s16 arg0, s16 arg1) {
    return FUN_080518a4((s32)arg0 << 4, arg1);
}
```

```asm
         push {lr}
         lsl r0, #0x10
         asr r0, #0xc
         lsl r1, #0x10
         asr r1, #0x10
         bl FUN_080518a4-0x4
         lsl r0, #0x10
         asr r0, #0x10
         pop {r1}
         bx r1
```









# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/math/FixedMul4.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	non_word_aligned_thumb_func_start FixedMul4
FixedMul4: @ 08000992
	lsls r0, r0, #0x10
	asrs r0, r0, #0x10
	lsls r1, r1, #0x10
	asrs r1, r1, #0x10
	muls r0, r1
	adds r1, r0, #0x0
	cmp r0, #0x00
	.2byte 0xDA00 @ bge _080009A2
	adds r1, #0x0F
	lsls r0, r1, #0x0C
	asrs r0, r0, #0x10
	bx lr
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
