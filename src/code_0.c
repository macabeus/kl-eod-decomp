#include "global.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/code_0", SetupOAMSprite); /* DrawSpriteTiles — core sprite/tile VRAM writer */
INCLUDE_ASM("asm/nonmatchings/code_0", RenderHUDTop); /* RenderHUDTop */
INCLUDE_ASM("asm/nonmatchings/code_0", RenderHUDBottom); /* RenderHUDBottom */
INCLUDE_ASM("asm/nonmatchings/code_0", RenderMenuUI); /* RenderMenuUI */
INCLUDE_ASM("asm/nonmatchings/code_0", RenderDialogBox); /* RenderDialogBox */
INCLUDE_ASM("asm/nonmatchings/code_0", RenderDialogSprites); /* RenderDialogSprites */
/**
 * InitOamEntries: initialize 128 OAM entries from a ROM template.
 *
 * Copies a 2-word template from 0x080E2A7C to each 8-byte OAM slot
 * at 0x03004800, then overwrites bytes 6-7 of each slot with successive
 * halfwords from the rotation/scaling table at 0x03004680.
 */
void InitOamEntries(void) {
    u32 addr1 = 0x03004680;
    register u16 *rotTable asm("r5");
    u32 addr2 = 0x080E2A7C;
    register u32 template_lo asm("r3");
    register u32 template_hi asm("r4");
    u32 addr3 = 0x03004800;
    register u8 *oam asm("r1");
    register s32 i asm("r2");
    asm("" : "=r"(rotTable) : "0"(addr1));
    {
        u32 *tmpl;
        asm("" : "=r"(tmpl) : "0"(addr2));
        template_lo = tmpl[0];
        template_hi = tmpl[1];
    }
    asm("" : "=r"(oam) : "0"(addr3));
    i = 0x7F;
    do {
        *(u32 *)(oam + 0) = template_lo;
        *(u32 *)(oam + 4) = template_hi;
        *(u16 *)(oam + 6) = *rotTable;
        rotTable++;
        oam += 8;
        i--;
    } while (i >= 0);
}
INCLUDE_ASM("asm/nonmatchings/code_0", TransformEntityScreenPositions);
INCLUDE_ASM("asm/nonmatchings/code_0", TransformSingleEntityToScreen);
INCLUDE_ASM("asm/nonmatchings/code_0", TransformAllEntitiesToScreen);
INCLUDE_ASM("asm/nonmatchings/code_0", HandlePauseMenuInput);
INCLUDE_ASM("asm/nonmatchings/code_0", UpdateUIState); /* UpdateUIState */
INCLUDE_ASM("asm/nonmatchings/code_0", RenderCharacterTiles); /* RenderCharacterTiles */
INCLUDE_ASM("asm/nonmatchings/code_0", UpdateTextScroll); /* UpdateTextScroll */
INCLUDE_ASM("asm/nonmatchings/code_0", VBlankCallback_Gameplay);
INCLUDE_ASM("asm/nonmatchings/code_0", AnimatePaletteEffects);
INCLUDE_ASM("asm/nonmatchings/code_0", VBlankCallback_Dialog);
INCLUDE_ASM("asm/nonmatchings/code_0", VBlankCallback_MapScreen);
INCLUDE_ASM("asm/nonmatchings/code_0", VBlankCallback_GameplayWithHUD);
INCLUDE_ASM("asm/nonmatchings/code_0", VBlankCallback_MinimalHW);
INCLUDE_ASM("asm/nonmatchings/code_0", VBlankCallback_Cutscene); /* SetupDisplayConfig */
INCLUDE_ASM("asm/nonmatchings/code_0", VBlankCallback_TitleScreen);
INCLUDE_ASM("asm/nonmatchings/code_0", VBlankCallback_Credits); /* TextStateMachine — master UI/text state machine */
INCLUDE_ASM("asm/nonmatchings/code_0", FUN_0800de24);
