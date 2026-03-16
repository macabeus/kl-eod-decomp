#include "global.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000ac8);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000bd4);
INCLUDE_ASM("asm/nonmatchings/engine", VBlankDmaTransfer);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000dc2);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000e16);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000e6a);
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
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000fce);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08001028);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_0800107c);
INCLUDE_ASM("asm/nonmatchings/engine", WaitVBlankAndClearMosaic);
INCLUDE_ASM("asm/nonmatchings/engine", AcknowledgeInterrupt);
INCLUDE_ASM("asm/nonmatchings/engine", InitLevelBG); /* InitGraphicsSystem — full GFX init: decompress assets, configure BG/VRAM */
INCLUDE_ASM("asm/nonmatchings/engine", ScrollBGLayer);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08001f58);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_0800247c);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_080027c4);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08002ac4);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08002fd0);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_0800343c);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_0800350c);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08003750);
INCLUDE_ASM("asm/nonmatchings/engine", ResetVideoRegisters); /* RenderFrame — per-frame rendering dispatch */
INCLUDE_ASM("asm/nonmatchings/engine", ClearVideoState);
INCLUDE_ASM("asm/nonmatchings/engine", ClearOamBufferExtended);
INCLUDE_ASM("asm/nonmatchings/engine", ClearOamEntries6Plus); /* DrawSpriteTilesFlipped */
