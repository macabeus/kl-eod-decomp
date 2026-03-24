#include "global.h"
#include "globals.h"
#include "include_asm.h"

/** Abs: returns absolute value of a signed integer. */
INCLUDE_ASM("asm/nonmatchings/system", Abs);

/** StrCmp: byte-by-byte string comparison. Returns 0 if equal, 1 if different. */
INCLUDE_ASM("asm/nonmatchings/system", StrCmp);
/** ReturnOne: unconditionally returns 1. */
s32 ReturnOne(void) {
    return 1;
}
/** StrCpy: copies a null-terminated string from src to dst. */
void StrCpy(u8 *dst, u8 *src) {
    register u32 c asm("r2");
    do {
        c = *src;
        *dst = c;
        dst++;
        src++;
    } while (c != 0);
}

/**
 * AgbMain: game entry point.
 *
 * Clears IWRAM/VRAM/OAM/palette via DMA, initializes display, sound,
 * and input systems, then enters the main game loop dispatching
 * through a state machine.
 */
INCLUDE_ASM("asm/nonmatchings/system", AgbMain);

/**
 * ReadKeyInput: read GBA buttons, debounce, and check for reset combo.
 *
 * Reads REG_KEYINPUT (active-low), XORs with 0x3FF to get active-high
 * pressed state, edge-detects new presses against previous frame,
 * checks A+B+Start+Select combo (0x0F) for soft reset, and tracks
 * A-button hold duration for repeat input.
 */
void ReadKeyInput(void) {
    u16 raw = *(volatile u16 *)0x04000130;
    u32 maskAddr = 0x3FF;
    register u32 mask asm("r2");
    u32 nkAddr = 0x03004DA0;
    u32 pkAddr = 0x030051E4;
    register u16 *newKeys asm("r3");
    register u16 *prevKeys asm("r4");
    register u32 pressed asm("r1");
    u16 prev;
    u32 edge;

    asm("" : "=r"(mask) : "0"(maskAddr));
    pressed = mask;
    asm("" : "+r"(pressed));
    pressed ^= raw;
    asm("" : "=r"(newKeys) : "0"(nkAddr));
    asm("" : "=r"(prevKeys) : "0"(pkAddr));
    prev = *prevKeys;
    edge = pressed;
    asm("" : "+r"(edge));
    edge &= ~(u32)prev;
    *newKeys = edge;
    *prevKeys = pressed;
    pressed &= 0x0F;
    if (pressed == 0x0F) {
        SoftReset(0xFF);
    }
    {
        u16 cur = *prevKeys;
        u16 aBtn = 1;
        aBtn &= cur;
        if (aBtn) {
            u16 *counter = (u16 *)0x030034F0;
            *counter = *counter + 1;
        } else {
            *(u16 *)0x030034F0 = aBtn;
        }
    }
}

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
void m4aSongNumStart(u16);
void FreeAllDecompBuffers(void) {
    u32 *decompBuffers = (u32 *)gDecompBufferCtrl;

    thunk_FUN_0800020c(decompBuffers[1] - 4);
    thunk_FUN_0800020c(decompBuffers[0] - 4);
    thunk_FUN_0800020c(decompBuffers[3] - 4);
    thunk_FUN_0800020c(decompBuffers[2] - 4);
    thunk_FUN_0800020c(decompBuffers[5] - 4);
    thunk_FUN_0800020c(decompBuffers[4] - 4);

    if (gCollisionMapPtr != 0) {
        thunk_FUN_0800020c(gCollisionMapPtr - 4);
        gCollisionMapPtr = 0;
    }

    m4aSongNumStart(0x8D);
    m4aSongNumStart(0x8E);
    m4aSongNumStart(0x8F);
    m4aSongNumStart(0x90);
}

/**
 * FixedMul8: 8.8 fixed-point signed multiply (s16*s16 >> 8).
 * Rounds negative results toward zero by adding 255 before shift.
 */
s16 FixedMul8(s16 a, s16 b) {
    s32 result = (s32)a * (s32)b;
    register s32 shifted asm("r1") = result;
    if (result < 0)
        shifted += 0xFF;
    return (s16)(shifted >> 8);
}
