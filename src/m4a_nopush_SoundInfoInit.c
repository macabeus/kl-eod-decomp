#include "global.h"
#include "gba.h"
#include "globals.h"

void SoundInfoInit(void) {
    u8 *soundInfo = (u8 *)*(u32 *)0x0300081C;
    u16 uval = *(u16 *)(soundInfo + 0x14);
    s16 val = *(s16 *)(soundInfo + 0x14);

    if (val <= 0) {
        *(u16 *)(soundInfo + 0x14) = 0;
        *(volatile u8 *)(soundInfo + 0x16) = *(volatile u8 *)(soundInfo + 0x16) & 0x7F;
        return;
    }

    if ((u16)(uval - 1) <= 14) {
        u16 *p = (u16 *)0x03003430;
        p[0x26 / 2] += 1;
        *(u16 *)(soundInfo + 0x14) -= 1;
    } else {
        u16 *p = (u16 *)0x03003430;
        p[0x26 / 2] += 2;
        *(u16 *)(soundInfo + 0x14) -= 2;
    }
}
