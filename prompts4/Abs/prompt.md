You are decompiling an assembly function called `Abs` in ARMv4T from a Game Boy Advance game.

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

## `ReturnOne`

```c
s32 ReturnOne(void) {
    return 1;
}
```

```asm
         mov r0, #0x1
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

Decompile the following target assembly function from `asm/nonmatchings/system/Abs.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	thumb_func_start Abs
Abs: @ 0800043C
	cmp r0, #0x00
	bpl _08000444
	movs r1, #0x00
	subs r0, r1, r0
_08000444:
	bx lr
	lsls r0, r0, #0x00
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
