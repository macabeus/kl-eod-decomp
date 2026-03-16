#include "global.h"
#include "include_asm.h"

extern s16 FUN_080518a4(s32, s16);

/**
 * FixedDivShift8: fixed-point division with 8-bit left shift.
 * Computes FUN_080518a4(arg0 << 8, arg1).
 */
s16 FixedDivShift8(s16 arg0, s16 arg1) {
    return FUN_080518a4((s32)arg0 << 8, arg1);
}

/**
 * FixedReciprocal: fixed-point reciprocal (1.0 / arg0).
 * Computes FUN_080518a4(0x10000, arg0) = 65536 / arg0.
 */
s16 FixedReciprocal(s16 arg0) {
    return FUN_080518a4(0x10000, arg0);
}

/** FixedMul4: 4.4 fixed-point signed multiply (s16*s16 >> 4). */
INCLUDE_ASM("asm/nonmatchings/math", FixedMul4);

/**
 * FixedMulShift4: fixed-point multiply with 4-bit left shift.
 * Computes FUN_080518a4(arg0 << 4, arg1).
 */
s16 FixedMulShift4(s16 arg0, s16 arg1) {
    return FUN_080518a4((s32)arg0 << 4, arg1);
}

/**
 * FixedDivUnit: fixed-point division by unit scale (256).
 * Computes FUN_080518a4(0x100, arg0) = 256 / arg0.
 */
s16 FixedDivUnit(s16 arg0) {
    return FUN_080518a4(0x100, arg0);
}

/**
 * VBlankHandler: main VBlank interrupt handler.
 *
 * DMAs screen buffers A/B/C to VRAM screenbases, DMAs the tilemap work
 * buffer, DMAs OAM shadow to hardware OAM, checks sound reset flag,
 * sets IME to acknowledge the interrupt.
 */
INCLUDE_ASM("asm/nonmatchings/math", VBlankHandler);

/**
 * VBlankHandlerMinimal: simplified VBlank for scene transitions.
 * Calls sound update + SoundInit, sets IME.
 */
INCLUDE_ASM("asm/nonmatchings/math", VBlankHandlerMinimal);
