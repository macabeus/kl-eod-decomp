#include "global.h"
#include "gba.h"
#include "globals.h"

void SoundChannelRelease(void);

/**
 * SoundContextRef: release all active sound channels on a track.
 *
 * Compiled with -ftst to produce tst instruction for the 0x80 status check.
 * Uses asm barriers for scheduling and register pinning.
 */
void SoundContextRef(u32 unused, u8 *track) {
    u8 *chan;
    u8 zero;
    register u8 st asm("r1") = track[0];
    register u32 m asm("r0") = 0x80;
    st &= m;
    if (!st)
        goto end;
    chan = *(u8 **)(track + 0x20);
    if (!chan)
        goto store_null;
    zero = 0;
    do {
        if (chan[0]) {
            register u32 type asm("r0") = chan[1];
            register u32 mask asm("r3") = 7;
            type &= mask;
            if (type) {
                u32 addr = 0x03007FF0;
                asm("" : "=r"(mask) : "0"(addr));
                mask = *(volatile u32 *)mask;
                mask = *(volatile u32 *)(mask + 0x2C);
                SoundChannelRelease();
            }
            asm("" : : "r"(type));
            chan[0] = zero;
        }
        *(u32 *)(chan + 0x2C) = zero;
        chan = *(u8 **)(chan + 0x34);
    } while (chan);
store_null:
    *(u8 **)(track + 0x20) = chan;
end:;
}
