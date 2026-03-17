#include "global.h"
#include "gba.h"
#include "globals.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/gfx", InitGfxState);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateBGScrollRegisters);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateBGTileAnimation);
void UpdateBGScrollRegisters(void);
void m4aSoundVSyncOff(void);
void m4aMPlayAllStop(void);
void UpdateSceneTransition(void);
/**
 * FadeOutController: manages screen fade-out, updating fade counter
 * and switching to scene transition when complete.
 */
void FadeOutController(void) {
    u32 a0 = 0x03004C20;
    u32 *sceneCtrl;
    asm("" : "=r"(sceneCtrl) : "0"(a0));

    if (sceneCtrl[0] == 0)
        UpdateBGScrollRegisters();

    {
        u32 val = sceneCtrl[0];
        u32 a1 = 0x03005498;
        u8 *fadeCounter;
        asm("" : "+r"(val));
        asm("" : "=r"(fadeCounter) : "0"(a1));

        if (val > 0x0F)
            *fadeCounter = (val - 0x10) >> 1;

        if (*fadeCounter > 0x0F) {
            sceneCtrl[0] = (u32)-1;
            {
                u32 a2 = 0x03003510;
                u32 *cbState;
                asm("" : "=r"(cbState) : "0"(a2));
                cbState[1] = (u32)UpdateSceneTransition;
            }
        }
    }

    m4aSoundVSyncOff();
    m4aMPlayAllStop();
}
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateSceneTransition);
INCLUDE_ASM("asm/nonmatchings/gfx", SetupSceneGfx);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateUIElementAnimation);
INCLUDE_ASM("asm/nonmatchings/gfx", InitSceneGfxByType);
INCLUDE_ASM("asm/nonmatchings/gfx", LoadBGPalette);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateBGPaletteAnimation);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateMenuCursorInput);
INCLUDE_ASM("asm/nonmatchings/gfx", SetupLevelTilemap);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateWorldMapScene);

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
INCLUDE_ASM("asm/nonmatchings/gfx", ReadUnalignedU32_Alt);
INCLUDE_ASM("asm/nonmatchings/gfx", CalcBGScrollMapSize);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateAffineRegisters);
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
void DecompressAndDmaCopy(u32 src, u32 dest, u32 size) {
    u32 buf = AllocAndDecompress((u32 *)src);
    DecompressData(buf, src);

    {
        vu32 *dma3 = (vu32 *)0x040000D4;
        dma3[0] = buf + 4; /* DMA3SAD: skip 4-byte sub-header */
        dma3[1] = dest; /* DMA3DAD */
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
void FinalizeLevelLayerSetup(u8 idx) {
    u32 *table;
    u32 addr = 0x08189B4C;
    asm("" : "=r"(table) : "0"(addr));
    DecompressAndCopyToPalette((u32 *)table[idx], 0x05000000, 0x1C0);
}
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
void LoadAndDecompressStream(u32 idx) {
    u32 *table;
    u32 addr = 0x08189AFC;
    u32 buf;
    asm("" : "=r"(table) : "0"(addr));
    buf = AllocAndDecompress((u32 *)table[idx]);
    gDecompBuffer = buf;
    gStreamPtr = (u8 *)(buf + 4);
}

/**
 * FreeDecompStreamBuffer: frees the decompressed stream buffer.
 */
void FreeDecompStreamBuffer(void) {
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

INCLUDE_ASM("asm/nonmatchings/gfx", DeadCode_0804bb86);

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
INCLUDE_ASM("asm/nonmatchings/gfx", SetupTextBGLayer);
INCLUDE_ASM("asm/nonmatchings/gfx", ClearScreenBufferB_Alt);
/**
 * InitLevelStateDefaults: sets default dimensions, scroll, window regs.
 * Initializes map dimensions (0xE80 x 0xA00), scroll (0x700 x 0xA00),
 * REG_WININ=0x1F23, REG_WINOUT=0x003D, clears OBJ window in DISPCNT.
 */
INCLUDE_ASM("asm/nonmatchings/gfx", InitLevelStateDefaults);
void VBlankHandler_WithWindowScroll(void);
void UpdateBGScrollWithWave(void);
void ReadKeyInput(void);
void SoundMain(void);
void VBlankCallback_MapScreen(void);
/**
 * SetupGfxCallbacks: initializes VBlank/HBlank handlers and callback state
 * for the world map screen.
 */
void SetupGfxCallbacks(void) {
    u32 a0 = 0x030047C0;
    u32 *vblankCbs;
    asm("" : "=r"(vblankCbs) : "0"(a0));
    vblankCbs[0] = (u32)VBlankHandler_WithWindowScroll;
    vblankCbs[1] = (u32)UpdateBGScrollWithWave;

    {
        u32 a1 = 0x03003510;
        u32 *cbState;
        asm("" : "=r"(cbState) : "0"(a1));
        cbState[0x28 / 4] = (u32)ReadKeyInput;
        cbState[0x2C / 4] = (u32)SoundMain;
        cbState[0x30 / 4] = (u32)VBlankCallback_MapScreen;
        cbState[0x34 / 4] = 1;
        {
            u32 idx = *((u8 *)cbState + 0x78) - 1;
            cbState[idx] = 0;
        }
        *((u8 *)cbState + 0x79) = 4;
    }
}
INCLUDE_ASM("asm/nonmatchings/gfx", InitWorldMapGfx);
/**
 * ShutdownGfxSubsystem: tears down the graphics subsystem on scene exit.
 *
 * Saves the current scene callback, disables HBlank IRQ and HBlank STAT,
 * then shuts down the graphics stream, sound, and frees all three
 * dynamically allocated graphics buffers.
 */
void ShutdownGfxSubsystem(void) {
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
    m4aMPlayAllStop();
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
void StreamCmd_ResetEntries(void) {
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
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetupOAMSpriteGroup);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetEntityFlags);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetEntityTransform);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetBGPriority);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_FillBGTilemap);
/**
 * StreamCmd_EnableMosaic: enables mosaic on BG2/BG3 and sets mosaic level.
 *
 * Sets bit 6 (mosaic) on REG_BG2CNT and REG_BG3CNT, reads a 4-bit
 * mosaic value from stream byte[2], stores to global at 0x030007D8.
 * Advances stream pointer by 3.
 */
void StreamCmd_EnableMosaic(void) {
    vu16 *bg2cnt = (vu16 *)0x0400000C;
    *bg2cnt |= 0x40;
    bg2cnt++;
    *bg2cnt |= 0x40;

    *(u8 *)0x030007D8 = gStreamPtr[2] & 0x0F;
    gStreamPtr += 3;
}
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetSpriteAttrs);
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
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateCursorBlink);
INCLUDE_ASM("asm/nonmatchings/gfx", ProcessAnimationSteps);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdateLinearInterpolation);
INCLUDE_ASM("asm/nonmatchings/gfx", CalcSinCosVelocity);
INCLUDE_ASM("asm/nonmatchings/gfx", ProcessMotionStep);
INCLUDE_ASM("asm/nonmatchings/gfx", ProcessMotionStepExtended);
INCLUDE_ASM("asm/nonmatchings/gfx", ProcessStaticBGScroll);
INCLUDE_ASM("asm/nonmatchings/gfx", ProcessFrameAnimation);
INCLUDE_ASM("asm/nonmatchings/gfx", ProcessSpriteOscillation);
INCLUDE_ASM("asm/nonmatchings/gfx", ProcessStarfieldEffect);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitStarfield);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitLinearMotion);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitLinearMotionExt);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitRotationMotion);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitMotionWithPalette);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitAngleMotion);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitOscillation);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitOscillationExt);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitStaticScroll);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitFrameAnimation);
INCLUDE_ASM("asm/nonmatchings/gfx", ProcessHBlankWait);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitHBlankWait);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitSpriteWave);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitButtonWait);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_StopMotion);
INCLUDE_ASM("asm/nonmatchings/gfx", ProcessScreenFade);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdatePaletteFadeStep);
INCLUDE_ASM("asm/nonmatchings/gfx", ProcessSceneTransitionOut);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetBGModeTiled);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_ClearRenderMode);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetTimerAndMode);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_ToggleDisplayFlag);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_ToggleLayerFlag);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetBlendMode);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetScrollPosition);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetBGScreenSize);
/**
 * StreamCmd_SetWindowRegs: writes WIN0H/WIN0V from stream bytes[2-5].
 * Advances stream by 6.
 */
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetWindowRegs);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_EnableScrollMode);
/**
 * StreamCmd_StopMusic: stream command to halt all music playback.
 * Calls m4aMPlayAllStop, advances stream by 2.
 */
void StreamCmd_StopMusic(void) {
    m4aMPlayAllStop();
    gStreamPtr += 2;
}
/*
 * Reads a command byte from the data stream and processes it via m4aMPlayCommand.
 * Byte[2] is the command argument. Advances the stream pointer by 3.
 *   no parameters (reads from global data stream pointer at 0x03004D84)
 *   no return value
 */
void ProcessStreamCommand_50094(void) {
    m4aMPlayCommand(gStreamPtr[2]);
    gStreamPtr += 3;
}
/*
 * Dispatches a sound/music stream command based on byte[2] of the data stream.
 * If byte[2] <= 0x22, passes it directly; otherwise re-reads and passes it.
 * Both paths call m4aSongNumStart. Advances the stream pointer by 3.
 *   no parameters (reads from global data stream pointer at 0x03004D84)
 *   no return value
 */
void DispatchMusicStreamCommand(void) {
    u8 *ptr = *(u8 **)0x03004D84;

    if (ptr[2] <= 0x22) {
        m4aSongNumStart(ptr[2]);
    } else {
        m4aSongNumStart(ptr[2]);
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
 * Calls m4aMPlayAllStop + m4aSoundVSyncOff, advances by 2.
 */
void StreamCmd_StopMusicAndDisableIRQ(void) {
    m4aMPlayAllStop();
    m4aSoundVSyncOff();
    gStreamPtr += 2;
}
/**
 * StreamCmd_DisableVBlank: disables VBlank interrupt and calls
 * m4aSoundVSyncOff. Advances stream by 2.
 */
void StreamCmd_DisableVBlank(void) {
    *(vu16 *)0x04000200 &= 0xFFFE; /* REG_IE &= ~INT_VBLANK */
    *(vu16 *)0x04000004 &= 0xFFF7; /* REG_DISPSTAT &= ~VBLANK_IRQ */
    m4aSoundVSyncOff();
    gStreamPtr += 2;
}
/*
 * Enables VBlank interrupt and VBlank IRQ status, then calls
 * m4aSoundVSyncOn to set up the handler. Advances the data stream by 2.
 *   no parameters
 *   no return value
 */
void EnableVBlankHandler(void) {
    REG_IE |= IE_VBLANK;
    REG_DISPSTAT |= DISPSTAT_VBLANK_IRQ_ENABLE;
    m4aSoundVSyncOn();
    gStreamPtr += 2;
}
/*
 * Enables VBlank interrupt and IRQ, sets up handler via m4aSoundVSyncOn,
 * then dispatches a music stream command via m4aSongNumStart using byte[2].
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
        m4aSoundVSyncOn();
        m4aSongNumStart((*gp)[2]);
    } else {
        *(vu16 *)0x04000200 |= 1;
        *(vu16 *)0x04000004 |= 8;
        m4aSoundVSyncOn();
        m4aSongNumStart((*gp)[2]);
    }

    *(u8 **)0x03004D84 += 3;
}
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_DisableVBlankAndStopMusic);
/*
 * Enables VBlank interrupt and VBlank IRQ status, then calls two
 * interrupt setup handlers (m4aSoundVSyncOn, StopSoundEffects).
 * Advances the data stream pointer by 2.
 *   no parameters
 *   no return value
 */
void EnableVBlankAndHandlers(void) {
    REG_IE |= IE_VBLANK;
    REG_DISPSTAT |= DISPSTAT_VBLANK_IRQ_ENABLE;
    m4aSoundVSyncOn();
    StopSoundEffects();
    gStreamPtr += 2;
}
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetMusicParams);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_ConfigureBlend);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_RunScript);
