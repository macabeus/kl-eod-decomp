#include "global.h"
#include "include_asm.h"

extern s16 FUN_080518a4(s32, s16);

s16 FUN_08000960(s16 arg0, s16 arg1) {
    return FUN_080518a4((s32)arg0 << 8, arg1);
}
INCLUDE_ASM("asm/nonmatchings/math", FUN_08000978);
INCLUDE_ASM("asm/nonmatchings/math", FUN_080009a8);
s16 FUN_080009c2(s16 arg0) {
    return FUN_080518a4(0x100, arg0);
}
INCLUDE_ASM("asm/nonmatchings/math", FUN_080009da);
INCLUDE_ASM("asm/nonmatchings/math", FUN_08000ab2);
