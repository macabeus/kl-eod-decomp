#include "global.h"
#include "globals.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/engine", VBlankHandler_ModeA);
INCLUDE_ASM("asm/nonmatchings/engine", VBlankHandler_ModeB);
INCLUDE_ASM("asm/nonmatchings/engine", VBlankDmaTransfer);
INCLUDE_ASM("asm/nonmatchings/engine", VBlankHandler_OamOnly);
INCLUDE_ASM("asm/nonmatchings/engine", VBlankHandler_OamOnlyAlt);
INCLUDE_ASM("asm/nonmatchings/engine", VBlankHandler_WithWindowScroll);
/**
 * UpdateFadeEffect: applies brightness fade using REG_BLDY.
 *
 * Reads REG_VCOUNT and entity brightness value, computes fade level
 * via FUN_08051a0c, then writes to REG_BLDY if within valid range (<=16).
 */
void UpdateFadeEffect(void) {
    vu8 *vcount_reg = (vu8 *)0x04000006;
    u8 *entity = (u8 *)0x03002920;
    u8 fade = FUN_08051a0c(*vcount_reg, entity[0x08]);

    if (fade <= 16) {
        *(vu16 *)0x04000052 = ((u32)fade << 8) | fade;
    }
}
INCLUDE_ASM("asm/nonmatchings/engine", HBlankScrollUpdate);
INCLUDE_ASM("asm/nonmatchings/engine", UpdateAffineBGParams);
/**
 * UpdateWindowCircleEffect: compute circle window bounds for iris transition.
 *
 * Reads the current scanline from REG_VCOUNT, computes left/right bounds
 * of a circular window using BiosSquareRoot, and writes to REG_WIN0H.
 * Used for iris-in/out screen transitions.
 */
void UpdateWindowCircleEffect(void) {
    u16 vcount = *(volatile u16 *)0x04000006;
    u32 radius = *(u32 *)0x03005488;
    u32 half_r = radius >> 1;
    s32 y = vcount - half_r;
    s32 y_adj = y + 12;
    s32 x_span;
    u32 val;
    u32 sqr;
    u32 hw;
    x_span = 0xE4 - y;
    x_span -= radius;
    val = (u32)(x_span * y_adj) << 2;
    sqr = BiosSquareRoot(val);
    hw = (u8)sqr >> 1;
    if (hw <= 0x78) {
        u32 winAddr = 0x04000042;
        volatile u16 *win;
        asm("" : "=r"(win) : "0"(winAddr));
        *win = ((0x78 - hw) << 8) | (hw + 0x78);
    } else {
        *(volatile u16 *)0x04000042 = 0;
    }
}
INCLUDE_ASM("asm/nonmatchings/engine", UpdateBGScrollWithWave);
INCLUDE_ASM("asm/nonmatchings/engine", WaitVBlankAndClearMosaic);
INCLUDE_ASM("asm/nonmatchings/engine", AcknowledgeInterrupt);
INCLUDE_ASM("asm/nonmatchings/engine", InitLevelBG); /* InitGraphicsSystem — full GFX init: decompress assets, configure BG/VRAM */
INCLUDE_ASM("asm/nonmatchings/engine", ScrollBGLayer);
INCLUDE_ASM("asm/nonmatchings/engine", ProcessOamSpriteLayout);
INCLUDE_ASM("asm/nonmatchings/engine", UpdateCameraScroll);
INCLUDE_ASM("asm/nonmatchings/engine", UpdateCameraScrollPlayer2);
INCLUDE_ASM("asm/nonmatchings/engine", CameraModeSwitchHandler);
INCLUDE_ASM("asm/nonmatchings/engine", InitLevelFromROMTable);
INCLUDE_ASM("asm/nonmatchings/engine", ScrollBGColumnLoad);
INCLUDE_ASM("asm/nonmatchings/engine", InitVideoAndBG);
INCLUDE_ASM("asm/nonmatchings/engine", ComputeRotationMatrix);
INCLUDE_ASM("asm/nonmatchings/engine", ResetVideoRegisters); /* RenderFrame — per-frame rendering dispatch */
/**
 * ClearVideoState: zeroes all 99 OAM shadow entries then calls InitOamEntries.
 */
void ClearVideoState(void) {
    register u32 *oamEntry asm("r0") = (u32 *)gOamBuffer0;
    register s32 entryCount asm("r1") = 0x63;
    register s32 zeroFill asm("r2") = 0;
    do {
        s32 bytesLeft = 0x1C;
        do {
            oamEntry++;
            bytesLeft -= 4;
            *oamEntry = zeroFill;
        } while (bytesLeft != 0);
        entryCount--;
    } while (entryCount != 0);
    InitOamEntries();
}
/**
 * ClearOamBufferExtended: zero OAM shadow buffer entries 1-98.
 */
void ClearOamBufferExtended(void) {
    register u32 *oamEntry asm("r0") = (u32 *)gOamBuffer1;
    register s32 entryCount asm("r1") = 0x62;
    register s32 zeroFill asm("r2") = 0;
    do {
        s32 bytesLeft = 0x1C;
        do {
            oamEntry++;
            bytesLeft -= 4;
            *oamEntry = zeroFill;
        } while (bytesLeft != 0);
        entryCount--;
    } while (entryCount != 0);
}
/**
 * ClearOamEntries6Plus: zero OAM shadow buffer entries 6-91.
 */
void ClearOamEntries6Plus(void) {
    register u32 *oamEntry asm("r0") = (u32 *)gOamBuffer6;
    register s32 entryCount asm("r1") = 0x56;
    register s32 zeroFill asm("r2") = 0;
    do {
        s32 bytesLeft = 0x1C;
        do {
            oamEntry++;
            bytesLeft -= 4;
            *oamEntry = zeroFill;
        } while (bytesLeft != 0);
        entryCount--;
    } while (entryCount != 0);
}
