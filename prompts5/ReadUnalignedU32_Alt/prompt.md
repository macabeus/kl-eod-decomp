You are decompiling an assembly function called `ReadUnalignedU32_Alt` in ARMv4T from a Game Boy Advance game.

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









# Primary Objective

Decompile the following target assembly function from `asm/nonmatchings/gfx/ReadUnalignedU32_Alt.s` into clean, readable C code that compiles to an assembly matching EXACTLY the original one.

```asm
	non_word_aligned_thumb_func_start ReadUnalignedU32_Alt
ReadUnalignedU32_Alt: @ 0804B28A
	adds r2, r0, #0x0
	ldrb r0, [r2, #0x00]
	ldrb r1, [r2, #0x01]
	lsls r1, r1, #0x08
	adds r0, r0, r1
	ldrb r1, [r2, #0x02]
	lsls r1, r1, #0x10
	adds r0, r0, r1
	ldrb r1, [r2, #0x03]
	lsls r1, r1, #0x18
	adds r0, r0, r1
	bx lr
```

# Rules

- In order to decompile this function, you may need to create new types. Include them on the result.

- SHOW THE ENTIRE CODE WITHOUT CROPPING.
