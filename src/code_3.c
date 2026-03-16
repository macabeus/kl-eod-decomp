#include "global.h"
#include "globals.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World1_Vision1);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World1_Vision2);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World2_Vision1);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World2_Vision2);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World3_Vision1);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World3_Vision2);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World4_Vision1);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World4_Vision2);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World5_Vision1);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World5_Vision2);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World6_Vision1);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World6_Vision2);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World7_Vision1);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World7_Vision2);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World8_Vision1);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World8_Vision2);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World9_Vision1);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_World9_Vision2);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadLevel_BossArena);
INCLUDE_ASM("asm/nonmatchings/code_3", InitGameplayState);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateOamSortOrder);
INCLUDE_ASM("asm/nonmatchings/code_3", ProcessInputAndUpdateEntities);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateWorldMapInput);
INCLUDE_ASM("asm/nonmatchings/code_3", CheckWorldCompletion); /* IsEntityActive */
INCLUDE_ASM("asm/nonmatchings/code_3", CopyWorldMapTiles); /* UpdateEntityState */
INCLUDE_ASM("asm/nonmatchings/code_3", SetWorldMapTilePalette);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateWorldMapNodeTile); /* UpdateEntityAnimation */
/*
 * Iterates over entity slots 0-6 and updates active ones.
 * For each slot, checks if the entity is active via CheckWorldCompletion.
 * If active, calls CopyWorldMapTiles and UpdateWorldMapNodeTile to update it.
 *   no parameters
 *   no return value
 */
void UpdateEntities(void) {
    u8 i;
    for (i = 0; i <= 6; i++) {
        if ((u8)CheckWorldCompletion(i)) {
            CopyWorldMapTiles(i);
            UpdateWorldMapNodeTile(i);
        }
    }
}
INCLUDE_ASM("asm/nonmatchings/code_3", CountCollectedGems);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateWorldMapNodeAnim);
INCLUDE_ASM("asm/nonmatchings/code_3", RunWorldMapTransition);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateAllEntities);
INCLUDE_ASM("asm/nonmatchings/code_3", GameplayMainLoop);
INCLUDE_ASM("asm/nonmatchings/code_3", InitLevelState);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateEntitySpawnState);
INCLUDE_ASM("asm/nonmatchings/code_3", SpawnEntitiesForVision);
INCLUDE_ASM("asm/nonmatchings/code_3", GetEntityLookupData);
INCLUDE_ASM("asm/nonmatchings/code_3", ComputeScrollLimits);
INCLUDE_ASM("asm/nonmatchings/code_3", ApplyPlayerMovement);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdatePlayerNormal);
INCLUDE_ASM("asm/nonmatchings/code_3", SetupEntitySpawnTable);
INCLUDE_ASM("asm/nonmatchings/code_3", RollRandomLevelVariant);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdatePlayerBoss);
INCLUDE_ASM("asm/nonmatchings/code_3", ConfigureEntityBehavior);
INCLUDE_ASM("asm/nonmatchings/code_3", ResetEntityTypesOnDeath);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdatePlayerMinigame);
INCLUDE_ASM("asm/nonmatchings/code_3", TransitionLevelVariant);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateLevelProgression);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdatePlayerAlternate);
INCLUDE_ASM("asm/nonmatchings/code_3", HandleSceneTransitionInput);
INCLUDE_ASM("asm/nonmatchings/code_3", DecompressRowToTilemap);
INCLUDE_ASM("asm/nonmatchings/code_3", SetEntityVisibility);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdatePlayerSpecial);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateLevelScrollDMA);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdatePlayerFinalBoss);
/**
 * DecompressData: process a compressed asset's sub-header and decompress.
 *
 * If the first word of src is negative (bit 31 set), the data uses two-stage
 * compression: first UnpackTilemap, then LZ77. Otherwise, it's LZ77 only.
 * In both cases, data starts at src+4 (after the sub-header).
 *
 *   dest: destination buffer (pre-allocated)
 *   src:  ROM pointer to compressed data with sub-header
 */
void DecompressData(u32 dest, u32 src)
{
    u32 *srcPtr = (u32 *)src;

    if ((s32)srcPtr[0] < 0) {
        /* Two-stage: Huffman/UnpackTilemap, then LZ77 */
        u32 tmpBuf = thunk_FUN_080001e0(srcPtr[1] >> 8, 0);
        UnpackTilemap((u8 *)src + 4, (u8 *)tmpBuf);
        LZ77UnCompWram((u8 *)tmpBuf, (u8 *)dest);
        thunk_FUN_0800020c(tmpBuf);
    } else {
        /* Single-stage: LZ77 only */
        LZ77UnCompWram((u8 *)src + 4, (u8 *)dest);
    }
}
/**
 * DecompressAndCopyToPalette: decompress and DMA to palette or OBJ VRAM.
 *
 * Allocates a buffer sized from the source header, decompresses the data,
 * DMAs the result (skipping the 4-byte sub-header) as 16-bit transfers to
 * the destination, then frees the buffer.
 *
 *   src:  ROM pointer to compressed data (first word = size | flags)
 *   dest: destination address (palette RAM or OBJ VRAM)
 *   size: byte count for DMA transfer
 */
void DecompressAndCopyToPalette(u32 *src, u32 dest, u16 size)
{
    u32 buf;
    vu32 *dma3;

    buf = thunk_FUN_080001e0(*src & 0x7FFFFFFF, 0);
    DecompressData(buf, (u32)src);

    dma3 = (vu32 *)0x040000D4;
    dma3[0] = buf + 4;
    dma3[1] = dest;
    dma3[2] = ((u32)size >> 1) | 0x80000000;
    (void)dma3[2];

    while (dma3[2] & 0x80000000)
        ;

    thunk_FUN_0800020c(buf);
}
/*
 * Allocates a buffer and decompresses/copies data into it.
 * Reads the size from the first word of the source (masking off the top bit),
 * allocates that many bytes, then calls DecompressData to fill the buffer.
 *   src: pointer to compressed data header (first word = size | flags)
 *   returns: pointer to the newly allocated and filled buffer
 */
u32 AllocAndDecompress(u32 *src) {
    u32 buf = thunk_FUN_080001e0(*src & 0x7FFFFFFF, 0);
    DecompressData(buf, (u32)src);
    return buf;
}
INCLUDE_ASM("asm/nonmatchings/code_3", InitSceneGraphics);
INCLUDE_ASM("asm/nonmatchings/code_3", RunSceneScript);
INCLUDE_ASM("asm/nonmatchings/code_3", RunTitleSequence);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateStageSelectScreen);
INCLUDE_ASM("asm/nonmatchings/code_3", DecompressAndLoadLevel); /* DecompressLZ77 */
INCLUDE_ASM("asm/nonmatchings/code_3", CheckTileCollisionRect);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateScrollPosition);
INCLUDE_ASM("asm/nonmatchings/code_3", InitPauseMenu);
INCLUDE_ASM("asm/nonmatchings/code_3", InitGameplayFromWorldMap);
/*
 * Runs the per-frame game update. If the pause flag at 0x030034E4 is zero,
 * calls four game subsystem update functions. Always calls UpdatePaletteAnimations
 * at the end regardless of pause state.
 *   no parameters
 *   no return value
 */
void GameUpdate(void) {
    if (gPauseFlag == 0) {
        UpdateWorldMapNodeState();
        UpdatePlayerInput();
        InitPlayerCollision();
        UpdateWorldMapCursor();
    }
    UpdatePaletteAnimations();
}
INCLUDE_ASM("asm/nonmatchings/code_3", SpawnLevelEntities);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdatePlayerInput); /* UpdatePhysics */
INCLUDE_ASM("asm/nonmatchings/code_3", InitPlayerCollision); /* UpdateCollision */
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateWorldMapCursor); /* UpdateCamera */
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateVisionStarIcons);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateWorldMapNodeState); /* UpdateGameLogic */
INCLUDE_ASM("asm/nonmatchings/code_3", FindNextUnlockedVision);
INCLUDE_ASM("asm/nonmatchings/code_3", SortEntityDrawOrder);
INCLUDE_ASM("asm/nonmatchings/code_3", SaveGameToSRAM);
INCLUDE_ASM("asm/nonmatchings/code_3", SaveGameWithVerify);
INCLUDE_ASM("asm/nonmatchings/code_3", LoadGameFromSRAM);
INCLUDE_ASM("asm/nonmatchings/code_3", SaveGameRetryLoop);
INCLUDE_ASM("asm/nonmatchings/code_3", SavePlayerProgress);
/**
 * IsSelectButtonPressed: returns 1 if Select (bit 6) is held, 0 otherwise.
 */
u32 IsSelectButtonPressed(void)
{
    u32 addr = 0x030051E4;
    u16 *keys;
    asm("" : "=r"(keys) : "0"(addr));
    if (*keys & 0x40)
        return 1;
    return 0;
}
INCLUDE_ASM("asm/nonmatchings/code_3", ConfigureInterruptsForGameplay);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdatePlayerEntity);
INCLUDE_ASM("asm/nonmatchings/code_3", MainGameFrameLoop);
INCLUDE_ASM("asm/nonmatchings/code_3", InitFadeTransition);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateScreenWipe);
INCLUDE_ASM("asm/nonmatchings/code_3", UpdateWorldMapLogic);
