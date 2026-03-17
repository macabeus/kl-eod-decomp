#include "global.h"
#include "include_asm.h"

/** Abs: returns absolute value of a signed integer. */
INCLUDE_ASM("asm/nonmatchings/system", Abs);

/** StrCmp: byte-by-byte string comparison. Returns 0 if equal, 1 if different. */
INCLUDE_ASM("asm/nonmatchings/system", StrCmp);
/** ReturnOne: unconditionally returns 1. */
INCLUDE_ASM("asm/nonmatchings/system", ReturnOne);
/** StrCpy: copies a null-terminated string from src to dst. */
INCLUDE_ASM("asm/nonmatchings/system", StrCpy);

/**
 * AgbMain: game entry point.
 *
 * Clears IWRAM/VRAM/OAM/palette via DMA, initializes display, sound,
 * and input systems, then enters the main game loop dispatching
 * through a state machine.
 */
INCLUDE_ASM("asm/nonmatchings/system", AgbMain);

/**
 * ReadKeyInput: reads REG_KEYINPUT, debounces, checks for reset combo.
 *
 * XORs raw key register to get active-high pressed state, filters
 * for edge detection, checks A+B+Start+Select for soft reset.
 */
INCLUDE_ASM("asm/nonmatchings/system", ReadKeyInput);

/**
 * ProcessInputAndTimers: extended input handler with timer management.
 *
 * Reads keys, processes directional input with acceleration, decrements
 * scene control timer, dispatches through state table at 0x080D9150.
 */
INCLUDE_ASM("asm/nonmatchings/system", ProcessInputAndTimers);

/**
 * LoadSpriteFrame: DMAs sprite frame data from ROM_TILESET_TABLE.
 *
 * Uses level/world indices from gSceneControl to look up the sprite
 * tileset, DMAs the frame's tile data to a decompression buffer.
 */
INCLUDE_ASM("asm/nonmatchings/system", LoadSpriteFrame);

/**
 * FreeAllDecompBuffers: frees all 6 decomp buffers + collision map.
 *
 * Frees gDecompBufferCtrl entries [0]-[5] (offset -4 for sub-header),
 * then conditionally frees gCollisionMapPtr.
 */
void thunk_FUN_0800020c(u32);
void InitSceneState(u32);
void FreeAllDecompBuffers(void)
{
    u32 a0 = 0x03004790;
    u32 *bufs;
    asm("" : "=r"(bufs) : "0"(a0));

    thunk_FUN_0800020c(bufs[1] - 4);
    thunk_FUN_0800020c(bufs[0] - 4);
    thunk_FUN_0800020c(bufs[3] - 4);
    thunk_FUN_0800020c(bufs[2] - 4);
    thunk_FUN_0800020c(bufs[5] - 4);
    thunk_FUN_0800020c(bufs[4] - 4);

    {
        u32 a1 = 0x03005290;
        u32 *collisionPtr;
        asm("" : "=r"(collisionPtr) : "0"(a1));
        if (*collisionPtr != 0) {
            thunk_FUN_0800020c(*collisionPtr - 4);
            *collisionPtr = 0;
        }
    }

    InitSceneState(0x8D);
    InitSceneState(0x8E);
    InitSceneState(0x8F);
    InitSceneState(0x90);
}

/**
 * FixedMul8: 8.8 fixed-point signed multiply (s16*s16 >> 8).
 * Rounds negative results toward zero by adding 255 before shift.
 */
s16 FixedMul8(s16 a, s16 b)
{
    s32 result = (s32)a * (s32)b;
    register s32 shifted asm("r1");
    shifted = result;
    asm("" : "+r"(shifted));
    if (result < 0)
        shifted += 0xFF;
    return (s16)(shifted >> 8);
}
