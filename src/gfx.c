#include "global.h"
#include "gba.h"
#include "globals.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/gfx", InitGfxState);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateBGScrollRegisters);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateBGTileAnimation);
INCLUDE_ASM("asm/nonmatchings/gfx", FadeOutController);
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
/**
 * LoadBGTileData: load per-level tile data for one BG layer.
 *
 * Looks up the ROM tile pointer and VRAM destination from configuration
 * tables, then calls DecompressAndDmaCopy to decompress and DMA the
 * tile character data into the layer's assigned charblock.
 *
 *   levelIdx: level index (0-based)
 *   sublevel: sublevel / layer pair index
 */
INCLUDE_ASM("asm/nonmatchings/gfx", LoadBGTileData);
INCLUDE_ASM("asm/nonmatchings/gfx", LoadBGTilemapData);
INCLUDE_ASM("asm/nonmatchings/gfx", SetupLevelLayerConfig);
/**
 * FinalizeLevelLayerSetup: loads the level's BG palette into palette RAM.
 *
 * Looks up a compressed palette from ROM_LEVEL_PALETTE_TABLE (0x08189B4C)
 * by index, decompresses and DMAs 0x1C0 bytes to palette RAM (0x05000000).
 *
 *   idx: level palette index (u8, shifted to u32 table offset)
 */
INCLUDE_ASM("asm/nonmatchings/gfx", FinalizeLevelLayerSetup);
/**
 * LoadAndDecompressStream: decompress a data stream from a ROM table entry.
 *
 * Looks up a compressed data pointer from ROM_STREAM_TABLE (0x08189AFC)
 * by index, allocates and decompresses it, stores the raw buffer in
 * gDecompBuffer and sets gStreamPtr to buffer+4 (past the header).
 */
/**
 * LoadAndDecompressStream: decompress a data stream from ROM table entry.
 * Sets gDecompBuffer and gStreamPtr from ROM_STREAM_TABLE[idx].
 */
INCLUDE_ASM("asm/nonmatchings/gfx", LoadAndDecompressStream);

/**
 * FreeDecompStreamBuffer: frees the decompressed stream buffer.
 */
void FreeDecompStreamBuffer(void)
{
    thunk_FUN_0800020c(gDecompBuffer);
}

INCLUDE_ASM("asm/nonmatchings/gfx", ClearScreenBufferB);

/**
 * AllocAndClearGfxBuffer: allocates and zero-fills a 0x20-byte GFX buffer.
 * Stores pointer in gGfxBufferPtr, DMA3-fills with zeros.
 */
INCLUDE_ASM("asm/nonmatchings/gfx", AllocAndClearGfxBuffer);

/**
 * FreeGfxBuffer: frees the GFX buffer struct at gGfxBufferPtr.
 */
INCLUDE_ASM("asm/nonmatchings/gfx", FreeGfxBuffer);

INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bb86);

/**
 * AllocAndClearBuffer_52A4: allocates and zero-fills a 0x480-byte buffer.
 * Stores pointer in gBuffer_52A4, DMA3-fills with zeros.
 */
INCLUDE_ASM("asm/nonmatchings/gfx", AllocAndClearBuffer_52A4);
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
INCLUDE_ASM("asm/nonmatchings/gfx", ClearScreenBufferB_Alt);
/**
 * InitLevelStateDefaults: sets default dimensions, scroll, window regs.
 * Initializes map dimensions (0xE80 x 0xA00), scroll (0x700 x 0xA00),
 * REG_WININ=0x1F23, REG_WINOUT=0x003D, clears OBJ window in DISPCNT.
 */
INCLUDE_ASM("asm/nonmatchings/gfx", InitLevelStateDefaults);
INCLUDE_ASM("asm/nonmatchings/gfx", SetupGfxCallbacks);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804be58);
/**
 * ShutdownGfxSubsystem: tears down the graphics subsystem on scene exit.
 *
 * Saves the current scene callback, disables HBlank IRQ and HBlank STAT,
 * then shuts down the graphics stream, sound, and frees all three
 * dynamically allocated graphics buffers.
 */
void ShutdownGfxSubsystem(void)
{
{
    vu32 *dest = (vu32 *)0x03000814;
    u32 *src = (u32 *)0x03004C20;
    *dest = src[1];
}

    *(vu16 *)0x04000200 &= 0xFFFD; /* REG_IE &= ~INT_HBLANK */
    *(vu16 *)0x04000004 &= 0xFFEF; /* REG_DISPSTAT &= ~HBLANK_IRQ */

    ShutdownGfxStream();
    FreeSoundStruct();
    FreeBuffer_52A4();
    FreeGfxBuffer();
    FreeDecompStreamBuffer();
    StopAllMusicPlayers();
}
/**
 * InitGfxStreamState: allocates stream buffer, clears OAM, resets cursors.
 * Alloc 0x100 bytes for gGfxStreamBuffer, DMA-zero, clear OAM shadow,
 * DMA OAM to hardware, set object count to 13, init write cursors.
 */
INCLUDE_ASM("asm/nonmatchings/gfx", InitGfxStreamState);
/**
 * ResetGfxStreamEntries: frees all active stream entries and resets state.
 * Iterates 32 entries in gGfxStreamBuffer, frees those with non-zero flags,
 * clears OAM, resets write cursors.
 */
INCLUDE_ASM("asm/nonmatchings/gfx", ResetGfxStreamEntries);
/**
 * StreamCmd_ResetEntries: stream command handler that resets all entries.
 *
 * Calls ResetGfxStreamEntries to free all active stream entries,
 * then advances the data stream pointer by 2.
 */
void StreamCmd_ResetEntries(void)
{
    ResetGfxStreamEntries();
    gStreamPtr += 2;
}
/*
 * Shuts down the graphics stream: calls ResetGfxStreamEntries to finalize,
 * then frees the buffer at 0x030007C8 via thunk_FUN_0800020c.
 *   no parameters
 *   no return value
 */
void ShutdownGfxStream(void) {
    ResetGfxStreamEntries();
    thunk_FUN_0800020c(gGfxStreamBuffer);
}
INCLUDE_ASM("asm/nonmatchings/gfx", LoadGfxStreamEntry); /* ProcessStreamOpcode */
/*
 * Reads a command byte from the data stream, splits it into a 7-bit value
 * and a 1-bit flag, then dispatches to LoadGfxStreamEntry. Advances stream by 3.
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
    LoadGfxStreamEntry(val, flag);
}
INCLUDE_ASM("asm/nonmatchings/gfx", DmaSpriteToObjVram);
/**
 * StreamCmd_DmaSpriteData: reads sprite entry/frame from stream, DMAs to OBJ VRAM.
 * Also contains SetSpriteTableFromIndex (0x0804C218) as an embedded sub-function.
 */
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_DmaSpriteData);
/*
 * Reads a command byte from the data stream and processes it via SetSpriteTableFromIndex.
 * Byte[2] is the command argument. Advances the stream pointer by 3.
 *   no parameters (reads from global data stream pointer at 0x03004D84)
 *   no return value
 */
void ProcessStreamCommand_C218(void) {
    SetSpriteTableFromIndex(gStreamPtr[2]);
    gStreamPtr += 3;
}
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_ConfigureSprite);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c300);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c3a4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c484);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c598);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c60c);
/**
 * StreamCmd_EnableMosaic: enables mosaic on BG2/BG3 and sets mosaic level.
 *
 * Sets bit 6 (mosaic) on REG_BG2CNT and REG_BG3CNT, reads a 4-bit
 * mosaic value from stream byte[2], stores to global at 0x030007D8.
 * Advances stream pointer by 3.
 */
void StreamCmd_EnableMosaic(void)
{
    vu16 *bg2cnt = (vu16 *)0x0400000C;
    *bg2cnt |= 0x40;
    bg2cnt++;
    *bg2cnt |= 0x40;

    *(u8 *)0x030007D8 = gStreamPtr[2] & 0x0F;
    gStreamPtr += 3;
}
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c6e0);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetRenderMode);
INCLUDE_ASM("asm/nonmatchings/gfx", DispatchLevelLayerSetup);
/**
 * StreamCmd_SetBGScroll: sets scroll position for a BG layer from stream data.
 *
 * Stream format: byte[2]=layer index, bytes[3-4]=scrollX, bytes[5-6]=scrollY.
 * Values are shifted left 4 for subpixel precision before storing to
 * gBGLayerState[layer].scrollX/Y. Advances stream by 7.
 */
/**
 * StreamCmd_SetBGScroll: sets BG scroll from stream data.
 * byte[2]=layer, bytes[3-4]=scrollX, bytes[5-6]=scrollY (shifted <<4).
 */
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetBGScroll);
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
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_ClearRenderMode);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e448);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e568);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_ToggleLayerFlag);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetBlendMode);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e634);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e6b6);
/**
 * StreamCmd_SetWindowRegs: writes REG_WININ/WINOUT from stream bytes[2-5].
 * Advances stream by 6.
 */
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetWindowRegs);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_EnableScrollMode);
/**
 * StreamCmd_StopMusic: stream command to halt all music playback.
 * Calls StopAllMusicPlayers, advances stream by 2.
 */
void StreamCmd_StopMusic(void)
{
    StopAllMusicPlayers();
    gStreamPtr += 2;
}
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
/**
 * StreamCmd_StopSound: stream command to stop sound effects.
 * Calls StopSoundEffects, advances stream by 2.
 * (non_word_aligned — can't match from C due to alignment)
 */
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_StopSound);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_Nop3);
/**
 * StreamCmd_StopMusicAndDisableIRQ: stops all music and disables interrupts.
 * Calls StopAllMusicPlayers + DisableInterruptsForGfxSetup, advances by 2.
 */
void StreamCmd_StopMusicAndDisableIRQ(void)
{
    StopAllMusicPlayers();
    DisableInterruptsForGfxSetup();
    gStreamPtr += 2;
}
/**
 * StreamCmd_DisableVBlank: disables VBlank interrupt and calls
 * DisableInterruptsForGfxSetup. Advances stream by 2.
 */
void StreamCmd_DisableVBlank(void)
{
    *(vu16 *)0x04000200 &= 0xFFFE; /* REG_IE &= ~INT_VBLANK */
    *(vu16 *)0x04000004 &= 0xFFF7; /* REG_DISPSTAT &= ~VBLANK_IRQ */
    DisableInterruptsForGfxSetup();
    gStreamPtr += 2;
}
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
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_DisableVBlankAndStopMusic);
/*
 * Enables VBlank interrupt and VBlank IRQ status, then calls two
 * interrupt setup handlers (EnableInterruptsAfterGfxSetup, StopSoundEffects).
 * Advances the data stream pointer by 2.
 *   no parameters
 *   no return value
 */
void EnableVBlankAndHandlers(void) {
    REG_IE |= IE_VBLANK;
    REG_DISPSTAT |= DISPSTAT_VBLANK_IRQ_ENABLE;
    EnableInterruptsAfterGfxSetup();
    StopSoundEffects();
    gStreamPtr += 2;
}
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetMusicParams);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_ConfigureBlend);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_RunScript);
