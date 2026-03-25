#include "global.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/util", SoftReset);
/**
 * SelectTimerTableByMode: sets the timer/EEPROM transfer table pointer
 * based on the given mode parameter.
 *
 * Mode 4 and default use 0x08188DE4, mode 0x40 uses 0x08188DF0.
 * Returns 0 on recognized mode, 1 on default fallback.
 */
u32 SelectTimerTableByMode(u16 mode) {
    u32 result = 0;
    if (mode == 4) {
        *(u32 *)0x030066F0 = 0x08188DE4;
    } else if (mode == 0x40) {
        *(u32 *)0x030066F0 = 0x08188DF0;
    } else {
        *(u32 *)0x030066F0 = 0x08188DE4;
        result = 1;
    }
    return result;
}
extern vu16 gEepromTimer;
extern vu8 gEepromReady;

void EepromTimerCallback(void) {
    u32 val;
    if (gEepromTimer == 0) {
        return;
    }
    val = gEepromTimer - 1;
    gEepromTimer = val;
    if (val << 16) {
        return;
    }
    gEepromReady = 1;
}
INCLUDE_ASM("asm/nonmatchings/util", FUN_080514d4);
INCLUDE_ASM("asm/nonmatchings/util", EepromBeginTransfer);
/**
 * EepromEndTransfer: disables the EEPROM timer and its interrupt.
 *
 * Clears the timer control registers, disables the timer's interrupt bit
 * in REG_IE, and restores the saved IME state.
 */
void EepromEndTransfer(void) {
    vu32 *timerPtrAddr = (vu32 *)0x03000380;
    u16 *timer = (u16 *)*timerPtrAddr;
    u16 zero = 0;

    *(vu16 *)timer = zero;
    timer += 1;
    *timerPtrAddr = (u32)timer;
    *(vu16 *)timer = zero;
    timer -= 1;
    *timerPtrAddr = (u32)timer;

    *(vu16 *)0x04000208 = zero;

    {
        vu16 *ie = (vu16 *)0x04000200;
        u32 timerIdx = *(vu8 *)0x03000378;
        u32 bit = 8;
        bit <<= timerIdx;
        {
            u16 val = *ie;
            val &= ~bit;
            *ie = val;
        }
    }

    *(vu16 *)0x04000208 = *(vu16 *)0x03000384;
}
INCLUDE_ASM("asm/nonmatchings/util", EepromDmaTransfer);
INCLUDE_ASM("asm/nonmatchings/util", EepromReadSector);
INCLUDE_ASM("asm/nonmatchings/util", EepromWriteSector);
INCLUDE_ASM("asm/nonmatchings/util", EepromVerifySector);
/**
 * SaveGameRetry: attempts write+verify save up to 3 times.
 * Calls EepromWriteSector (write) then EepromVerifySector (verify).
 */
INCLUDE_ASM("asm/nonmatchings/util", SaveGameRetry);
