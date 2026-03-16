#include "global.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000ac8);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000bd4);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000ce2);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000dc2);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000e16);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000e6a);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000f70);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000fa2);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08000fce);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08001028);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_0800107c);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_0800111e);
INCLUDE_ASM("asm/nonmatchings/engine", FUN_0800113e);
INCLUDE_ASM("asm/nonmatchings/engine", InitLevelBG); /* InitGraphicsSystem — full GFX init: decompress assets, configure BG/VRAM */
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08001cd0);
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
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08003d80); /* DrawSpriteTilesPartial */
INCLUDE_ASM("asm/nonmatchings/engine", FUN_08003da0); /* DrawSpriteTilesFlipped */
