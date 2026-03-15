#include "global.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/math", FUN_08000960);
INCLUDE_ASM("asm/nonmatchings/math", FUN_08000978);
INCLUDE_ASM("asm/nonmatchings/math", FUN_080009a8);
s16 FUN_080518a4(s16 a, s16 b);

s16 FUN_080009c2(s16 arg0) {
    return FUN_080518a4(0x100, arg0);
}
INCLUDE_ASM("asm/nonmatchings/math", FUN_080009da);
INCLUDE_ASM("asm/nonmatchings/math", FUN_08000ab2);
