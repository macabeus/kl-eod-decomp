#include "global.h"
#include "globals.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080301a8);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08030680);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080313f8);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08031adc);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08031e7e);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080326e8);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08032d3e);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080335d4);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08033aac);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803407a);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080348b0);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08035210);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08035ef8);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08037038);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080375a0);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08037dbc);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803881c);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080392a4);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08039920);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08039d8c);
INCLUDE_ASM("asm/nonmatchings/code_3", sub_0803A22C);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803a410);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803aaa0);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803ac18); /* IsEntityActive */
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803ad94); /* UpdateEntityState */
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803ae88);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803af38); /* UpdateEntityAnimation */
/*
 * Iterates over entity slots 0-6 and updates active ones.
 * For each slot, checks if the entity is active via FUN_0803ac18.
 * If active, calls FUN_0803ad94 and FUN_0803af38 to update it.
 *   no parameters
 *   no return value
 */
void UpdateEntities(void) {
    u8 i;
    for (i = 0; i <= 6; i++) {
        if ((u8)FUN_0803ac18(i)) {
            FUN_0803ad94(i);
            FUN_0803af38(i);
        }
    }
}
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803b0a0);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803b378);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803b3d2);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803b602);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803bf84);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803c808);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803ce16);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803cf08);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803d140);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803d15c);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803d4ac);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803d90c);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803e6d8);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803e8cc);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803e904);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803f68c);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803f952);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0803f9ee);
INCLUDE_ASM("asm/nonmatchings/code_3", sub_08040B50);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08040d68);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08040f1e);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08041df0);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08041e96);
INCLUDE_ASM("asm/nonmatchings/code_3", sub_08041F34);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08042024);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08042bee);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08042e66);
/**
 * DecompressData: process a compressed asset's sub-header and decompress.
 *
 * If the first word of src is negative (bit 31 set), the data uses two-stage
 * compression: first UnpackTilemap, then LZ77. Otherwise, it's LZ77 only.
 * In both cases, data starts at src+4 (after the sub-header).
 *
 *   dest: destination buffer (pre-allocated)
 *   src:  ROM pointer to compressed data with sub-header
 */
void DecompressData(u32 dest, u32 src)
{
    u32 *srcPtr = (u32 *)src;

    if ((s32)srcPtr[0] < 0) {
        /* Two-stage: Huffman/UnpackTilemap, then LZ77 */
        u32 tmpBuf = thunk_FUN_080001e0(srcPtr[1] >> 8, 0);
        UnpackTilemap((u8 *)src + 4, (u8 *)tmpBuf);
        LZ77UnCompWram((u8 *)tmpBuf, (u8 *)dest);
        thunk_FUN_0800020c(tmpBuf);
    } else {
        /* Single-stage: LZ77 only */
        LZ77UnCompWram((u8 *)src + 4, (u8 *)dest);
    }
}
/**
 * DecompressAndCopyToPalette: decompress and DMA to palette or OBJ VRAM.
 *
 * Allocates a buffer sized from the source header, decompresses the data,
 * DMAs the result (skipping the 4-byte sub-header) as 16-bit transfers to
 * the destination, then frees the buffer.
 *
 *   src:  ROM pointer to compressed data (first word = size | flags)
 *   dest: destination address (palette RAM or OBJ VRAM)
 *   size: byte count for DMA transfer
 */
void DecompressAndCopyToPalette(u32 *src, u32 dest, u16 size)
{
    u32 buf;
    vu32 *dma3;

    buf = thunk_FUN_080001e0(*src & 0x7FFFFFFF, 0);
    DecompressData(buf, (u32)src);

    dma3 = (vu32 *)0x040000D4;
    dma3[0] = buf + 4;
    dma3[1] = dest;
    dma3[2] = ((u32)size >> 1) | 0x80000000;
    (void)dma3[2];

    while (dma3[2] & 0x80000000)
        ;

    thunk_FUN_0800020c(buf);
}
/*
 * Allocates a buffer and decompresses/copies data into it.
 * Reads the size from the first word of the source (masking off the top bit),
 * allocates that many bytes, then calls DecompressData to fill the buffer.
 *   src: pointer to compressed data header (first word = size | flags)
 *   returns: pointer to the newly allocated and filled buffer
 */
u32 AllocAndDecompress(u32 *src) {
    u32 buf = thunk_FUN_080001e0(*src & 0x7FFFFFFF, 0);
    DecompressData(buf, (u32)src);
    return buf;
}
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08043ba4);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080441c8);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080446fa);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08044bb8);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08044f6c); /* DecompressLZ77 */
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0804517c);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080452ea);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0804539a);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080453f0);
/*
 * Runs the per-frame game update. If the pause flag at 0x030034E4 is zero,
 * calls four game subsystem update functions. Always calls FUN_08025ba4
 * at the end regardless of pause state.
 *   no parameters
 *   no return value
 */
void GameUpdate(void) {
    if (gPauseFlag == 0) {
        FUN_080468b0();
        FUN_08045874();
        FUN_08045f68();
        FUN_08046288();
    }
    FUN_08025ba4();
}
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0804575c);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08045874); /* UpdatePhysics */
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08045f68); /* UpdateCollision */
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08046288); /* UpdateCamera */
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080467f4);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080468b0); /* UpdateGameLogic */
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080469fc);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08046a64);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08046b6c);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08046db8);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08046f6c);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0804713e);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080471f4);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080472b0);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080472c8);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080475dc);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_080477a8);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08047b1e);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_08047eca);
INCLUDE_ASM("asm/nonmatchings/code_3", FUN_0804802a);
