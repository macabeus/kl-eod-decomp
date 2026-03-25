#include "global.h"
#include "gba.h"
#include "globals.h"
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
        *(u32 *)&gEepromConfigTable = ROM_EEPROM_CONFIG_STD;
    } else if (mode == 0x40) {
        *(u32 *)&gEepromConfigTable = ROM_EEPROM_CONFIG_ALT;
    } else {
        *(u32 *)&gEepromConfigTable = ROM_EEPROM_CONFIG_STD;
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
/**
 * InitEepromTimer: sets up a hardware timer for EEPROM transfer timing.
 *
 * Stores the timer index (0-3), computes the timer register address
 * (REG_TM0CNT + index*4), and installs EepromTimerCallback.
 * Returns 0 on success, 1 if timerIdx > 3.
 */
u32 InitEepromTimer(u8 timerIdx, u32 *callbackPtr) {
    if (timerIdx > 3)
        return 1;

    gEepromTimerIdx = timerIdx;
    gEepromTimerRegPtr = REG_TM0CNT_BASE + gEepromTimerIdx * 4;
    *callbackPtr = (u32)EepromTimerCallback;
    return 0;
}
/**
 * EepromBeginTransfer: starts an EEPROM transfer using a hardware timer.
 *
 * Saves IME, enables the timer interrupt, writes the transfer descriptor
 * (size, timer value, timer control) to hardware registers.
 */
void EepromBeginTransfer(u16 *desc) {
    u32 zeroVal = 0;
    register u32 zero asm("r5");
    u32 imeAddr = 0x04000208;
    register vu16 *ime asm("r3");
    u32 ieAddr = 0x04000200;
    register vu16 *ie asm("r4");

    {
        u16 *savedPtr = &gEepromSavedIME;
        asm("" : "=r"(ime) : "0"(imeAddr));
        *savedPtr = *ime;
    }
    asm("" : "=r"(zero) : "0"(zeroVal));
    *ime = zero;

    asm("" : "=r"(ie) : "0"(ieAddr));
    {
        u32 tidxAddr = 0x03000378;
        register u32 timerIdx asm("r1");
        asm("" : "=r"(timerIdx) : "0"(tidxAddr));
        timerIdx = *(vu8 *)timerIdx;
        {
            u32 bit = 8;
            bit <<= timerIdx;
            timerIdx = *ie;
            timerIdx |= bit;
            *ie = timerIdx;
        }
    }

    *ime = 1;
    gEepromStateFlag = zero;
    *(u16 *)0x0300037A = desc[0];

    desc++;
    {
        vu32 *timerPtrAddr = &gEepromTimerRegPtr;
        u16 *timer = (u16 *)*timerPtrAddr;
        *(vu16 *)timer = desc[0];
        timer += 1;
        *timerPtrAddr = (u32)timer;
        *(vu16 *)timer = desc[1];
        timer -= 1;
        *timerPtrAddr = (u32)timer;
    }
}
/**
 * EepromEndTransfer: disables the EEPROM timer and its interrupt.
 *
 * Clears the timer control registers, disables the timer's interrupt bit
 * in REG_IE, and restores the saved IME state.
 */
void EepromEndTransfer(void) {
    vu32 *timerPtrAddr = &gEepromTimerRegPtr;
    u16 *timer = (u16 *)*timerPtrAddr;
    u16 zero = 0;

    *(vu16 *)timer = zero;
    timer += 1;
    *timerPtrAddr = (u32)timer;
    *(vu16 *)timer = zero;
    timer -= 1;
    *timerPtrAddr = (u32)timer;

    REG_IME = zero;

    {
        vu16 *ie = &REG_IE;
        u32 timerIdx = gEepromTimerIdx;
        u32 bit = 8;
        bit <<= timerIdx;
        {
            u16 val = *ie;
            val &= ~bit;
            *ie = val;
        }
    }

    REG_IME = gEepromSavedIME;
}
INCLUDE_ASM("asm/nonmatchings/util", EepromDmaTransfer);
INCLUDE_ASM("asm/nonmatchings/util", EepromReadSector);
INCLUDE_ASM("asm/nonmatchings/util", EepromWriteSector);
/**
 * EepromVerifySector: reads an EEPROM sector and compares against expected data.
 *
 * Reads 8 bytes (4 halfwords) from the given sector into a stack buffer,
 * then compares each halfword against the expected data.
 * Returns 0 on match, 0x8000 on mismatch, 0x80FF if sector out of range.
 */
u16 EepromVerifySector(u16 sector, u16 *expected) {
    u16 buf[4];
    u16 result = 0;
    u16 *table = gEepromConfigTable;

    if (sector >= table[2])
        return 0x80FF;

    EepromReadSector(sector, buf);

    {
        u16 *actual = buf;
        u8 i = 0;
        while (i <= 3) {
            u16 exp = *expected;
            u16 act = *actual;
            actual++;
            expected++;
            if (exp != act) {
                result = 0x8000;
                break;
            }
            i++;
        }
    }
    return result;
}
u16 EepromReadSector(u16, u16 *);
u16 EepromWriteSector(u16, u16 *);
/**
 * SaveGameRetry: attempts write+verify save up to 3 times.
 *
 * Writes a sector with EepromWriteSector, then verifies with
 * EepromVerifySector. Retries up to 3 times on failure.
 * Returns 0 on success, non-zero on persistent failure.
 */
u16 SaveGameRetry(u16 sector, u16 *data) {
    u16 result;
    u8 retryCount;

    for (retryCount = 0; retryCount <= 2; retryCount++) {
        result = EepromWriteSector(sector, data);
        if (result != 0)
            continue;
        result = EepromVerifySector(sector, data);
        if (result != 0)
            continue;
        break;
    }
    return result;
}
