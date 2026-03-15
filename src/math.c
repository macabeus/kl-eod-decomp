#include "global.h"
#include "include_asm.h"

s16 FUN_080518a4(s32 a, s16 b);

s16 FUN_08000960(s16 arg0, s16 arg1) {
    return (s16)FUN_080518a4((s16)arg0 << 8, arg1);
}
INCLUDE_ASM("asm/nonmatchings/math", FUN_08000978);
INCLUDE_ASM("asm/nonmatchings/math", FUN_080009a8);
INCLUDE_ASM("asm/nonmatchings/math", FUN_080009c2);
INCLUDE_ASM("asm/nonmatchings/math", FUN_080009da);
INCLUDE_ASM("asm/nonmatchings/math", FUN_08000ab2);
