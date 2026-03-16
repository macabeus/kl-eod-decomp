#include "global.h"
#include "gba.h"
#include "globals.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/gfx", InitGfxState);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateBGScrollRegisters);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateBGTileAnimation);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_08048768);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_080487b4);
INCLUDE_ASM("asm/nonmatchings/gfx", SetupSceneGfx);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_080491c0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_08049348);
INCLUDE_ASM("asm/nonmatchings/gfx", LoadBGPalette);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateBGPaletteAnimation);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_08049efc);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804a070);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804af00);

/*
 * Reads an unsigned 16-bit value from a potentially unaligned address.
 * Assembles two bytes in little-endian order.
 *   ptr: pointer to the first of two consecutive bytes
 *   returns: the reconstructed u16 value
 */
u32 ReadUnalignedU16(u8 *ptr) {
    return ptr[0] | (ptr[1] << 8);
}

/*
 * Reads a signed 16-bit value from a potentially unaligned address.
 * Assembles two bytes in little-endian order with sign extension.
 *   ptr: pointer to the first of two consecutive bytes
 *   returns: the reconstructed s16 value
 */
s16 ReadUnalignedS16(u8 *ptr) {
    return (s16)(ptr[0] + (ptr[1] << 8));
}

/*
 * Reads an unsigned 32-bit value from a potentially unaligned address.
 * Assembles four bytes in little-endian order.
 *   ptr: pointer to the first of four consecutive bytes
 *   returns: the reconstructed u32 value
 */
u32 ReadUnalignedU32(u8 *ptr) {
    return ptr[0] + (ptr[1] << 8) + (ptr[2] << 16) + (ptr[3] << 24);
}
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804b28a);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804b2a0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804b2ec);
/**
 * DecompressAndDmaCopy: decompress ROM data and DMA to a VRAM destination.
 *
 * Allocates a temp buffer, decompresses the source data (processing the
 * 4-byte sub-header), DMAs the payload (skipping the sub-header) to the
 * destination, waits for DMA completion, then frees the temp buffer.
 *
 *   src:  ROM pointer to compressed data
 *   dest: VRAM destination address
 *   size: byte count for DMA transfer
 */
void DecompressAndDmaCopy(u32 src, u32 dest, u32 size)
{
    u32 buf = AllocAndDecompress((u32 *)src);
    DecompressData(buf, src);

    {
        vu32 *dma3 = (vu32 *)0x040000D4;
        dma3[0] = buf + 4;       /* DMA3SAD: skip 4-byte sub-header */
        dma3[1] = dest;           /* DMA3DAD */
        dma3[2] = (size >> 1) | 0x80000000; /* DMA3CNT: 16-bit, enable */
        (void)dma3[2];

        while (dma3[2] & 0x80000000)
            ;
    }

    thunk_FUN_0800020c(buf);
}
INCLUDE_ASM("asm/nonmatchings/gfx", LoadBGTileData);
INCLUDE_ASM("asm/nonmatchings/gfx", LoadBGTilemapData);
INCLUDE_ASM("asm/nonmatchings/gfx", SetupLevelLayerConfig);
INCLUDE_ASM("asm/nonmatchings/gfx", FinalizeLevelLayerSetup);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bad4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bafc);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bb12);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bb3c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bb74);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bb86);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bb88);
/*
 * Frees the memory buffer pointed to by the global at 0x030052A4.
 *   no parameters
 *   no return value
 */
void FreeBuffer_52A4(void) {
    thunk_FUN_0800020c(gBuffer_52A4);
}
INCLUDE_ASM("asm/nonmatchings/gfx", SetupWorldMapBG);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bd10);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bd8a);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bdb4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804be08);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804be58);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bf7c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bfd0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c050);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c0be);
/*
 * Shuts down the graphics stream: calls FUN_0804c050 to finalize,
 * then frees the buffer at 0x030007C8 via thunk_FUN_0800020c.
 *   no parameters
 *   no return value
 */
void ShutdownGfxStream(void) {
    FUN_0804c050();
    thunk_FUN_0800020c(gGfxStreamBuffer);
}
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c0ec); /* ProcessStreamOpcode */
/*
 * Reads a command byte from the data stream, splits it into a 7-bit value
 * and a 1-bit flag, then dispatches to FUN_0804c0ec. Advances stream by 3.
 *   no parameters (reads from global data stream pointer at 0x03004D84)
 *   no return value
 */
void DispatchStreamCommand_C0EC(void) {
    u8 **gp = (u8 **)0x03004D84;
    u8 *ptr = *gp;
    u8 byte = ptr[2];
    u8 val = byte & 0x7F;
    u8 flag = byte >> 7;
    *gp = ptr + 3;
    FUN_0804c0ec(val, flag);
}
INCLUDE_ASM("asm/nonmatchings/gfx", sub_0804C1C0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c1fc);
/*
 * Reads a command byte from the data stream and processes it via FUN_0804c218.
 * Byte[2] is the command argument. Advances the stream pointer by 3.
 *   no parameters (reads from global data stream pointer at 0x03004D84)
 *   no return value
 */
void ProcessStreamCommand_C218(void) {
    FUN_0804c218(gStreamPtr[2]);
    gStreamPtr += 3;
}
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c24c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c300);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c3a4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c484);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c598);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c60c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c6a8);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c6e0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c776);
INCLUDE_ASM("asm/nonmatchings/gfx", DispatchLevelLayerSetup);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c7fc);
/*
 * Reads a palette color entry from a data stream and writes it to BG palette RAM.
 * The stream at *0x03004D84 is a packed format: byte[2] is the palette index,
 * bytes[3..4] are a little-endian RGB555 color value. Advances the stream pointer by 5.
 *   no parameters (reads from global data stream pointer at 0x03004D84)
 *   no return value (writes directly to GBA palette RAM at 0x05000000)
 */
void WritePaletteColor(void) {
    u8 **gp = (u8 **)0x03004D84;
    u16 color = ReadUnalignedU16(*gp + 3);
    u8 *ptr = *gp;
    *(u16 *)(BG_PAL_RAM + ptr[2] * 2) = color;
    *gp = ptr + 5;
}
/*
 * Reads a 16-bit value from the data stream and writes it to two destinations:
 * the palette/color register at 0x03005420 and the mirror at 0x030034AC.
 * Advances the data stream pointer by 4.
 *   no parameters (reads from global data stream pointer at 0x03004D84)
 *   no return value
 */
void WriteStreamValue_Dual(void) {
    u16 *dest1 = (u16 *)0x030034AC;
    u8 **gp = (u8 **)0x03004D84;

    int val = ReadUnalignedU16(*gp + 2);
    *(u16 *)0x03005420 = val;
    *dest1 = val;

    *gp += 4;
}
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c898);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c8f4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c9a8);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804ca6c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804cac8);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804cc4c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804cf26);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804cf80);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804cfd0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d0b0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d32c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d408);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d4d8);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d63c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d798);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d8d4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d99c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804da60);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804db38);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804dbd4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804dc64);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804dccc);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804dd48);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804de34);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804debc);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804df80);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e008);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e0e8);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e3d6);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e42a);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e448);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e568);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e5c6);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e5f0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e634);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e6b6);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e708);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e73a);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e76e);
/*
 * Reads a command byte from the data stream and processes it via FUN_08050094.
 * Byte[2] is the command argument. Advances the stream pointer by 3.
 *   no parameters (reads from global data stream pointer at 0x03004D84)
 *   no return value
 */
void ProcessStreamCommand_50094(void) {
    FUN_08050094(gStreamPtr[2]);
    gStreamPtr += 3;
}
/*
 * Dispatches a sound/music stream command based on byte[2] of the data stream.
 * If byte[2] <= 0x22, passes it directly; otherwise re-reads and passes it.
 * Both paths call InitSceneState. Advances the stream pointer by 3.
 *   no parameters (reads from global data stream pointer at 0x03004D84)
 *   no return value
 */
void DispatchMusicStreamCommand(void) {
    u8 *ptr = *(u8 **)0x03004D84;

    if (ptr[2] <= 0x22) {
        InitSceneState(ptr[2]);
    } else {
        InitSceneState(ptr[2]);
    }

    *(u8 **)0x03004D84 += 3;
}
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e7d2);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e7e6);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e7fa);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e814);
/*
 * Enables VBlank interrupt and VBlank IRQ status, then calls
 * EnableInterruptsAfterGfxSetup to set up the handler. Advances the data stream by 2.
 *   no parameters
 *   no return value
 */
void EnableVBlankHandler(void) {
    REG_IE |= IE_VBLANK;
    REG_DISPSTAT |= DISPSTAT_VBLANK_IRQ_ENABLE;
    EnableInterruptsAfterGfxSetup();
    gStreamPtr += 2;
}
/*
 * Enables VBlank interrupt and IRQ, sets up handler via EnableInterruptsAfterGfxSetup,
 * then dispatches a music stream command via InitSceneState using byte[2].
 * Advances the data stream pointer by 3.
 *   no parameters (reads from global data stream pointer at 0x03004D84)
 *   no return value
 */
void EnableVBlankAndDispatchMusic(void) {
    u8 **gp = (u8 **)0x03004D84;
    u8 *ptr = *gp;

    if (ptr[2] <= 0x22) {
        *(vu16 *)0x04000200 |= 1;
        *(vu16 *)0x04000004 |= 8;
        EnableInterruptsAfterGfxSetup();
        InitSceneState((*gp)[2]);
    } else {
        *(vu16 *)0x04000200 |= 1;
        *(vu16 *)0x04000004 |= 8;
        EnableInterruptsAfterGfxSetup();
        InitSceneState((*gp)[2]);
    }

    *(u8 **)0x03004D84 += 3;
}
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e8fe);
/*
 * Enables VBlank interrupt and VBlank IRQ status, then calls two
 * interrupt setup handlers (EnableInterruptsAfterGfxSetup, FUN_08050134).
 * Advances the data stream pointer by 2.
 *   no parameters
 *   no return value
 */
void EnableVBlankAndHandlers(void) {
    REG_IE |= IE_VBLANK;
    REG_DISPSTAT |= DISPSTAT_VBLANK_IRQ_ENABLE;
    EnableInterruptsAfterGfxSetup();
    FUN_08050134();
    gStreamPtr += 2;
}
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e974);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e9dc);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804ea94);
