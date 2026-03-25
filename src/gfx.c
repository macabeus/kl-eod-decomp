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
    u32 *sceneCtrl = (u32 *)0x03004C20;
    u32 fadeTimer;
    u8 *fadeCounter;

    if (sceneCtrl[0] == 0)
        UpdateBGScrollRegisters();

    fadeTimer = *(vu32 *)sceneCtrl;
    fadeCounter = (u8 *)0x03005498;

    if (fadeTimer > 0x0F)
        *fadeCounter = (fadeTimer - 0x10) >> 1;

    if (*fadeCounter > 0x0F) {
        sceneCtrl[0] = (u32)-1;
        gCallbackStateArray[1] = (u32)UpdateSceneTransition;
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

/**
 * ReadUnalignedU16: reads an unsigned 16-bit value from a potentially
 * unaligned address. Assembles two bytes in little-endian order.
 */
u32 ReadUnalignedU16(u8 *ptr) {
    return ptr[0] | (ptr[1] << 8);
}

/**
 * ReadUnalignedS16: reads a signed 16-bit value from a potentially
 * unaligned address. Assembles two bytes in little-endian order with sign extension.
 */
s16 ReadUnalignedS16(u8 *ptr) {
    return (s16)(ptr[0] + (ptr[1] << 8));
}

/**
 * ReadUnalignedU32: reads an unsigned 32-bit value from a potentially
 * unaligned address. Assembles four bytes in little-endian order.
 */
u32 ReadUnalignedU32(u8 *ptr) {
    return ptr[0] + (ptr[1] << 8) + (ptr[2] << 16) + (ptr[3] << 24);
}
s32 ReadUnalignedU32_Alt(u8 *ptr) {
    return ptr[0] + (ptr[1] << 8) + (ptr[2] << 0x10) + (ptr[3] << 0x18);
}
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
    vu32 *dma3;
    DecompressData(buf, src);

    dma3 = (vu32 *)0x040000D4;
    dma3[0] = buf + 4; /* DMA3SAD: skip 4-byte sub-header */
    dma3[1] = dest; /* DMA3DAD */
    dma3[2] = (size >> 1) | 0x80000000; /* DMA3CNT: 16-bit, enable */
    (void)dma3[2];

    while (dma3[2] & 0x80000000)
        ;

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
    DecompressAndCopyToPalette((u32 *)gLevelPaletteTable[idx], 0x05000000, 0x1C0);
}
/**
 * LoadAndDecompressStream: decompress a data stream from ROM table entry.
 *
 * Looks up a compressed data pointer from gStreamDataTable by index,
 * allocates and decompresses it, stores the raw buffer in gDecompBuffer
 * and sets gStreamPtr to buffer+4 (past the header).
 */
void LoadAndDecompressStream(u32 idx) {
    u32 buf = AllocAndDecompress((u32 *)gStreamDataTable[idx]);
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
 * AllocAndClearGfxBuffer: allocate and DMA-fill a 32-byte GFX buffer.
 *
 * Allocates 32 bytes via thunk_FUN_080001e0 (malloc), stores the pointer
 * at gGfxBufferPtr, then DMA3-fills the buffer with zero using a
 * stack-local halfword as the fill source.
 */
void AllocAndClearGfxBuffer(void) {
    u16 zero_src;
    u32 addr = 0x030034A0;
    register u32 *gfxBuf asm("r4");
    u32 *buf;
    asm("" : "=r"(gfxBuf) : "0"(addr));
    buf = (u32 *)thunk_FUN_080001e0(0x20, 0);
    *gfxBuf = (u32)buf;
    {
        u32 dma_addr = 0x040000D4;
        register volatile u32 *dma3 asm("r1");
        u32 sp_ptr = (u32)&zero_src;
        zero_src = 0;
        asm("" : "=r"(dma3) : "0"(dma_addr));
        dma3[0] = sp_ptr;
        dma3[1] = (u32)buf;
        {
            u32 ctrl = 0x81000010;
            asm("" : "=r"(ctrl) : "0"(ctrl));
            dma3[2] = ctrl;
            dma3[2];
        }
    }
}

/**
 * FreeGfxBuffer: frees the GFX buffer struct at gGfxBufferPtr.
 */
void FreeGfxBuffer(void) {
    thunk_FUN_0800020c((void *)gGfxBufferPtr);
}

INCLUDE_ASM("asm/nonmatchings/gfx", DeadCode_0804bb86);

/**
 * AllocAndClearBuffer_52A4: allocate and DMA-fill a 1152-byte buffer.
 *
 * Allocates 0x480 bytes (0x90 << 3) via thunk_FUN_080001e0, stores the
 * pointer at gBuffer_52A4, then DMA3-fills with zero using a stack-local
 * halfword as fill source.
 */
void AllocAndClearBuffer_52A4(void) {
    u16 zero_src;
    u32 addr = 0x030052A4;
    register u32 *bufPtr asm("r4");
    u32 *buf;
    asm("" : "=r"(bufPtr) : "0"(addr));
    buf = (u32 *)thunk_FUN_080001e0(0x90 << 3, 0);
    *bufPtr = (u32)buf;
    {
        u32 dma_addr = 0x040000D4;
        register volatile u32 *dma3 asm("r1");
        u32 sp_ptr = (u32)&zero_src;
        zero_src = 0;
        asm("" : "=r"(dma3) : "0"(dma_addr));
        dma3[0] = sp_ptr;
        dma3[1] = (u32)buf;
        {
            u32 ctrl = 0x81000240;
            asm("" : "=r"(ctrl) : "0"(ctrl));
            dma3[2] = ctrl;
            dma3[2];
        }
    }
}
/** FreeBuffer_52A4: frees the memory buffer at gBuffer_52A4. */
void FreeBuffer_52A4(void) {
    thunk_FUN_0800020c(gBuffer_52A4);
}
INCLUDE_ASM("asm/nonmatchings/gfx", SetupWorldMapBG);
/**
 * SetupTextBGLayer: initialize BG3 for text/UI rendering.
 *
 * Sets up the BG layer state table entry for BG3: tile base, map base,
 * scroll, map width. Calls SoundDmaInit to load font tiles, DMA-copies
 * text palette from ROM, sets REG_BG3CNT=0x700, clears BG3 scroll.
 */
void SetupTextBGLayer(void) {
    u32 tblAddr = 0x03003430;
    register u8 *tbl asm("r4");
    asm("" : "=r"(tbl) : "0"(tblAddr));
    {
        u32 base = *(u16 *)(tbl + 0x16) + 0x20;
        u32 srcAddr = 0x0804BB11;
        u8 *src;
        asm("" : "=r"(src) : "0"(srcAddr));
        SoundDmaInit(base, (u32)src, 0x1D, 0x10);
    }
    *(u32 *)(tbl + 0x1C) = 0xC0 << 19;
    *(u32 *)(tbl + 0x20) = 0x06003800;
    {
        u16 zero = 0;
        *(u16 *)(tbl + 0x24) = zero;
        *(u16 *)(tbl + 0x26) = zero;
        {
            u8 *mapW = tbl + 0x34;
            *mapW = 0x20;
        }
        *(u16 *)(tbl + 0x30) = *(u16 *)(tbl + 0x16);
        {
            u32 dmaAddr = 0x040000D4;
            register volatile u32 *dma asm("r1");
            u32 romPal = 0x080576B4;
            u32 palDst = 0x050001E0;
            u32 ctrl = 0x80000020;
            asm("" : "=r"(dma) : "0"(dmaAddr));
            asm("" : "=r"(romPal) : "0"(romPal));
            dma[0] = romPal;
            asm("" : "=r"(palDst) : "0"(palDst));
            dma[1] = palDst;
            asm("" : "=r"(ctrl) : "0"(ctrl));
            dma[2] = ctrl;
            dma[2];
            asm("add\t%0, #-0xCA" : "+r"(dma));
            *(volatile u16 *)dma = 0x700;
        }
        {
            u32 scrollAddr = 0x04000014;
            volatile u16 *scroll;
            asm("" : "=r"(scroll) : "0"(scrollAddr));
            *scroll = zero;
            asm("add\t%0, #0x02" : "+r"(scroll));
            *scroll = zero;
        }
    }
}
INCLUDE_ASM("asm/nonmatchings/gfx", ClearScreenBufferB_Alt);
/**
 * InitLevelStateDefaults: set default level dimensions, scroll, and window regs.
 *
 * Initializes map dimensions (0xE80 x 0xA00), scroll (0x700 x 0xA00),
 * calls UpdateAffineRegisters, sets REG_WININ=0x1F23, REG_WINOUT=0x003D,
 * clears OBJ window enable (bit 14) in REG_DISPCNT.
 */
void InitLevelStateDefaults(void) {
    u32 addr = 0x030034A0;
    u32 *bufAddr;
    register u16 *buf asm("r1");

    asm("" : "=r"(bufAddr) : "0"(addr));
    buf = *(u16 **)bufAddr;
    {
        u16 val = 0;
        u16 scroll;
        u16 dim;
        buf[4] = val;
        val = 0xE80;
        buf[8] = val;
        scroll = 0x700;
        buf[5] = scroll;
        dim = 0xA00;
        buf[9] = dim;
        buf[6] = scroll;
        buf[11] = dim;
    }
    UpdateAffineRegisters();
    {
        u32 winAddr = 0x04000048;
        u32 wiVal = 0x1F23;
        register u16 *winin asm("r1");
        register u32 wiConst asm("r2");
        register u32 val asm("r0");
        asm("" : "=r"(winin) : "0"(winAddr));
        asm("" : "=r"(wiConst) : "0"(wiVal));
        val = wiConst;
        asm("" : "+r"(val));
        *winin = val;
        asm("add\t%0, #0x02" : "+r"(winin));
        *winin = 0x3D;
    }
    {
        volatile u16 *dispcnt = (volatile u16 *)(0x80 << 19);
        u32 bfAddr = 0xBFFF;
        u16 val = *dispcnt;
        u16 mask;
        asm("" : "=r"(mask) : "0"(bfAddr));
        mask &= val;
        *dispcnt = mask;
    }
}
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
    u32 *vblankHandlers = (u32 *)gVBlankCallbackArray;
    u32 *callbackState;
    u32 slotIdx;
    vblankHandlers[0] = (u32)VBlankHandler_WithWindowScroll;
    vblankHandlers[1] = (u32)UpdateBGScrollWithWave;

    callbackState = (u32 *)0x03003510;
    callbackState[0x28 / 4] = (u32)ReadKeyInput;
    callbackState[0x2C / 4] = (u32)SoundMain;
    callbackState[0x30 / 4] = (u32)VBlankCallback_MapScreen;
    callbackState[0x34 / 4] = 1;
    slotIdx = *((u8 *)callbackState + 0x78) - 1;
    callbackState[slotIdx] = 0;
    *((u8 *)callbackState + 0x79) = 4;
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
    vu32 *dest = (vu32 *)0x03000814;
    u32 *src = (u32 *)0x03004C20;
    *dest = src[1];

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
 * InitGfxStreamState: allocate stream buffer, clear OAM, reset state.
 *
 * Allocates 0x100 bytes for gGfxStreamBuffer, DMA-fills with zero,
 * calls ClearVideoState, DMA-copies OAM shadow to hardware OAM,
 * sets stream mode to 0x0D, initializes two write cursor pointers.
 */
void InitGfxStreamState(void) {
    u16 zero_src;
    u32 bufAddr = 0x030007C8;
    register u32 *bufPtr asm("r4");
    u32 *buf;
    asm("" : "=r"(bufPtr) : "0"(bufAddr));
    buf = (u32 *)thunk_FUN_080001e0(0x80 << 1, 0);
    *bufPtr = (u32)buf;
    {
        u32 dmaAddr = 0x040000D4;
        register volatile u32 *dma asm("r4");
        u32 sp_ptr = (u32)&zero_src;
        zero_src = 0;
        asm("" : "=r"(dma) : "0"(dmaAddr));
        dma[0] = sp_ptr;
        dma[1] = (u32)buf;
        {
            u32 ctrl = 0x81000080;
            asm("" : "=r"(ctrl) : "0"(ctrl));
            dma[2] = ctrl;
            dma[2];
        }
        ClearVideoState();
        {
            u32 oamSrc = 0x03004800;
            u32 ctrl2 = 0x84000100;
            asm("" : "=r"(oamSrc) : "0"(oamSrc));
            dma[0] = oamSrc;
            dma[1] = 0xE0 << 19;
            asm("" : "=r"(ctrl2) : "0"(ctrl2));
            dma[2] = ctrl2;
            dma[2];
        }
    }
    {
        u32 modeAddr = 0x03005428;
        u8 *mode;
        asm("" : "=r"(mode) : "0"(modeAddr));
        *mode = 0x0D;
    }
    {
        u32 d1 = 0x030007DC;
        u32 s1 = 0x030034F4;
        u32 *dst1;
        u32 *src1;
        asm("" : "=r"(dst1) : "0"(d1));
        asm("" : "=r"(src1) : "0"(s1));
        *dst1 = *src1;
    }
    {
        u32 d2 = 0x03005490;
        u32 s2 = 0x030052AC;
        u32 *dst2;
        u32 *src2;
        asm("" : "=r"(dst2) : "0"(d2));
        asm("" : "=r"(src2) : "0"(s2));
        *dst2 = *src2;
    }
}
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
/**
 * DmaSpriteToObjVram: DMA sprite tile data from ROM to OBJ VRAM.
 */
void DmaSpriteToObjVram(u32 entryIdx, u32 frameIdx) {
    vu32 *dma3 = &REG_DMA3SAD;
    u32 base = *(vu32 *)&gGfxStreamBuffer;
    u8 *entry = (u8 *)(entryIdx * 8 + base);
    u16 tileCount = *(u16 *)(entry + 6);
    dma3[0] = *(u32 *)entry + tileCount * (frameIdx << 5);

    dma3[1] = (u32) * (u16 *)(entry + 4) * 32 + OBJ_VRAM;

    dma3[2] = (*(u16 *)(entry + 6) << 4) | (0x80 << 24);
    dma3[2];
}
/**
 * StreamCmd_DmaSpriteData: reads sprite entry/frame from stream, DMAs to OBJ VRAM.
 */
void StreamCmd_DmaSpriteData(void) {
    u8 *ptr = gStreamPtr;
    u8 a = ptr[2];
    u8 b = ptr[3];
    gStreamPtr = ptr + 4;
    DmaSpriteToObjVram(a, b);
}
void SetSpriteTableFromIndex(u32 arg0) {
    gSpriteRenderPtr = gSpriteDataTable[arg0];
}
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

    gBldyFadeLevel = gStreamPtr[2] & 0x0F;
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
 * StreamCmd_SetBGScroll: set BG layer scroll from stream data.
 *
 * Reads scrollX (bytes[3-4]) and scrollY (bytes[5-6]) via ReadUnalignedU16,
 * shifts each left 4 for subpixel precision, stores to the BG layer state
 * table indexed by byte[2]. Advances stream by 7.
 */
void StreamCmd_SetBGScroll(void) {
    u32 spAddr = 0x03004D84;
    register u8 **sp asm("r4");
    register u16 scrollX asm("r3");
    u32 tblAddr = 0x03003430;
    register u8 *tbl asm("r5");
    u8 *p;
    u8 layer;
    u32 off;

    asm("" : "=r"(sp) : "0"(spAddr));
    scrollX = ReadUnalignedU16(*sp + 3);
    asm("" : "=r"(tbl) : "0"(tblAddr));
    p = *sp;
    layer = p[2];
    off = (u32)(layer * 7) << 2;
    off += (u32)tbl;
    *(u16 *)(off + 8) = scrollX << 4;
    {
        u16 scrollY = ReadUnalignedU16(p + 5);
        u8 *p2 = *sp;
        u8 layer2 = p2[2];
        u32 off2 = (u32)(layer2 * 7) << 2;
        off2 += (u32)tbl;
        *(u16 *)(off2 + 10) = scrollY << 4;
        *sp = p2 + 7;
    }
}
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
/**
 * ProcessHBlankWait: process HBlank wait timer for a stream entry.
 *
 * Enables HBlank interrupt (bit 1 in REG_IE) and VCount interrupt
 * (bit 4 in REG_DISPSTAT). Checks the entry's timer at offset 0x14;
 * if expired (negative after decrement), disables both interrupts
 * and returns 0. If bit 7 of entry[3] is set or timer still positive,
 * returns 1.
 *
 * @param idx  Stream entry index
 * @return     1 if still waiting, 0 if timer expired
 */
u32 ProcessHBlankWait(u32 idx) {
    u32 ieAddr = 0x04000200;
    register volatile u16 *ie asm("r3");
    u32 dsAddr = 0x04000004;
    register volatile u16 *dispstat asm("r4");
    u8 *buf;
    u32 off;
    u8 *entry;

    asm("" : "=r"(ie) : "0"(ieAddr));
    *ie |= 2;
    asm("" : "=r"(dispstat) : "0"(dsAddr));
    *dispstat |= 0x10;

    {
        u8 **bufPtr;
        u32 bAddr = 0x030052A4;
        asm("" : "=r"(bufPtr) : "0"(bAddr));
        buf = *bufPtr;
    }
    off = idx * 9;
    off <<= 2;
    off += (u32)buf;
    entry = (u8 *)off;

    if (entry[3] >> 7) {
        return 1;
    }
    {
        u32 timer = *(u16 *)(entry + 0x14) - 1;
        *(u16 *)(entry + 0x14) = timer;
        if ((s32)(timer << 16) < 0) {
            *ie &= 0xFFFD;
            *dispstat &= 0xFFEF;
            return 0;
        }
    }
    return 1;
}
/**
 * StreamCmd_InitHBlankWait: initialize an HBlank wait entry from stream data.
 */
void StreamCmd_InitHBlankWait(void) {
    s16 timerVal;
    u32 spAddr = (u32)&gStreamPtr;
    register u8 **streamPP asm("r5");
    u32 bAddr = (u32)&gBuffer_52A4;
    register u8 **basePP asm("r6");

    asm("" : "=r"(streamPP) : "0"(spAddr));
    timerVal = ReadUnalignedS16(*streamPP + 3);

    {
        u8 *sp;
        u8 idx;
        u8 *base;

        sp = *streamPP;
        idx = sp[2];
        asm("" : "=r"(basePP) : "0"(bAddr));
        base = *basePP;

        {
            u32 off = (u32)(idx * 9) * 4;
            off += (u32)base;
            *(s16 *)(off + 0x14) = timerVal;
        }

        idx = sp[2];
        {
            u32 off = (u32)(idx * 9) * 4;
            off += (u32)base;
            {
                u8 *entry = (u8 *)off;
                s16 val = *(s16 *)(entry + 0x14);
                u32 signBit = ((u32)val >> 31) << 7;
                entry[3] = (entry[3] & 0x7F) | signBit;
            }
        }
    }

    {
        u8 *sp = *streamPP;
        u8 idx = sp[2];
        u8 *base = *basePP;

        {
            u32 off = (u32)(idx * 9) * 4;
            off += (u32)base;
            *(u32 *)(off + 0x20) = (u32)ProcessHBlankWait;
        }

        idx = sp[2];
        {
            u32 off = (u32)(idx * 9) * 4;
            off += (u32)base;
            {
                u8 *entry = (u8 *)off;
                u8 flags = entry[0];
                s32 mask = -8;
                mask &= flags;
                entry[0] = mask | 1;
            }
        }
    }

    *streamPP += 5;
}
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitSpriteWave);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_InitButtonWait);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_StopMotion);
INCLUDE_ASM("asm/nonmatchings/gfx", ProcessScreenFade);
INCLUDE_ASM("asm/nonmatchings/gfx", UpdatePaletteFadeStep);
INCLUDE_ASM("asm/nonmatchings/gfx", ProcessSceneTransitionOut);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetBGModeTiled);
/**
 * StreamCmd_SetRenderModeTiled: set render mode to 2.
 * Clears low 2 bits of gGfxBuffer[0], sets bit 1. Advances stream by 2.
 */
void StreamCmd_SetRenderModeTiled(void) {
    u8 *buf = *(u8 **)0x030034A0;
    u8 val = buf[0];
    s32 mask = -4;
    mask &= val;
    buf[0] = mask | 2;
    gStreamPtr += 2;
}
void StreamCmd_ClearRenderMode(void) {
    s8 *p;
    u8 **streamPtr;

    p = *(s8 **)0x030034A0;
    *p &= -4;
    streamPtr = (u8 **)0x03004D84;
    *streamPtr += 2;
}
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetTimerAndMode);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_ToggleDisplayFlag);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_ToggleLayerFlag);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetBlendMode);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetScrollPosition);
INCLUDE_ASM("asm/nonmatchings/gfx", StreamCmd_SetBGScreenSize);
/**
 * StreamCmd_SetWindowRegs: writes WIN0H/WIN0V from stream bytes[2-5].
 * Reads two pairs of bytes from the command stream at gStreamPtr,
 * packs each pair into a 16-bit value (low | high<<8), and writes
 * them to REG_WININ (0x04000048) and REG_WINOUT (0x0400004A).
 * Advances stream by 6.
 */
void StreamCmd_SetWindowRegs(void) {
    vu16 *reg = (vu16 *)0x04000048;
    u8 **streamPtrAddr = (u8 **)0x03004D84;
    u8 *stream = *streamPtrAddr;

    *reg = stream[2] | (stream[3] << 8);
    reg++;
    *reg = stream[4] | (stream[5] << 8);
    *streamPtrAddr = stream + 6;
}
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
 */
void StreamCmd_StopSound(void) {
    StopSoundEffects();
    gStreamPtr += 2;
}
void StreamCmd_Nop3(void) {
    gStreamPtr += 3;
}
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
