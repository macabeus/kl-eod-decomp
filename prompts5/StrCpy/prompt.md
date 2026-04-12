You are decompiling an assembly function called `StrCpy` in ARMv4T from a Game Boy Advance game.

# Examples

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

## `ply_keysh`

```c
void ply_keysh(void *r0, TrackStruct *track) {
    track->keyShift = *track->unk40;
    track->unk40++;
}
```

```asm
         ldr r0, [r1, #0x40]
         ldrb r2, [r0, #0x0]
         mov r0, r1
         add r0, #0x24
         strb r2, [r0, #0x0]
         ldr r0, [r1, #0x40]
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

## `ply_vol`

```c
void ply_vol(void *r0, TrackStruct *track) {
    track->unk2D = *track->unk40;
    track->unk40++;
}
```

```asm
         ldr r0, [r1, #0x40]
         ldrb r0, [r0, #0x0]
         mov r2, r1
         add r2, #0x2d
         strb r0, [r2, #0x0]
         ldr r0, [r1, #0x40]
         add r0, #0x1
         str r0, [r1, #0x40]
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
