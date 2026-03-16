#include "global.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/code_0", SetupOAMSprite); /* DrawSpriteTiles — core sprite/tile VRAM writer */
INCLUDE_ASM("asm/nonmatchings/code_0", RenderHUDTop); /* RenderHUDTop */
INCLUDE_ASM("asm/nonmatchings/code_0", RenderHUDBottom); /* RenderHUDBottom */
INCLUDE_ASM("asm/nonmatchings/code_0", RenderMenuUI); /* RenderMenuUI */
INCLUDE_ASM("asm/nonmatchings/code_0", RenderDialogBox); /* RenderDialogBox */
INCLUDE_ASM("asm/nonmatchings/code_0", RenderDialogSprites); /* RenderDialogSprites */
INCLUDE_ASM("asm/nonmatchings/code_0", InitOamEntries); /* InitOamEntries */
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
