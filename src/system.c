#include "global.h"
#include "include_asm.h"

/** Abs: returns absolute value of a signed integer. */
INCLUDE_ASM("asm/nonmatchings/system", Abs);
/** StrCmp: byte-by-byte string comparison. Returns 0 if equal, 1 if different. */
INCLUDE_ASM("asm/nonmatchings/system", StrCmp);
INCLUDE_ASM("asm/nonmatchings/system", ReturnOne);
/** StrCpy: copies a null-terminated string from src to dst. */
INCLUDE_ASM("asm/nonmatchings/system", StrCpy);
INCLUDE_ASM("asm/nonmatchings/system", AgbMain);
INCLUDE_ASM("asm/nonmatchings/system", FUN_080006cc);
INCLUDE_ASM("asm/nonmatchings/system", FUN_0800072c);
INCLUDE_ASM("asm/nonmatchings/system", sub_0800087C);
INCLUDE_ASM("asm/nonmatchings/system", FUN_080008dc);
/** FixedMul8: 8.8 fixed-point signed multiply (s16*s16 >> 8). */
INCLUDE_ASM("asm/nonmatchings/system", FixedMul8);
