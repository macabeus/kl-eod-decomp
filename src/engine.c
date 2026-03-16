#include "global.h"
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
void UpdateFadeEffect(void)
{
    vu8 *vcount_reg = (vu8 *)0x04000006;
    u8 *entity = (u8 *)0x03002920;
    u8 fade = FUN_08051a0c(*vcount_reg, entity[0x08]);

    if (fade <= 16) {
        *(vu16 *)0x04000052 = ((u32)fade << 8) | fade;
    }
}
INCLUDE_ASM("asm/nonmatchings/engine", HBlankScrollUpdate);
INCLUDE_ASM("asm/nonmatchings/engine", UpdateAffineBGParams);
INCLUDE_ASM("asm/nonmatchings/engine", UpdateWindowCircleEffect);
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
INCLUDE_ASM("asm/nonmatchings/engine", ClearVideoState);
INCLUDE_ASM("asm/nonmatchings/engine", ClearOamBufferExtended);
INCLUDE_ASM("asm/nonmatchings/engine", ClearOamEntries6Plus); /* DrawSpriteTilesFlipped */
