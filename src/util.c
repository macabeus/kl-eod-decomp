#include "global.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/util", SoftReset);
INCLUDE_ASM("asm/nonmatchings/util", FUN_0805146c);
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
INCLUDE_ASM("asm/nonmatchings/util", EepromEndTransfer);
INCLUDE_ASM("asm/nonmatchings/util", EepromDmaTransfer);
INCLUDE_ASM("asm/nonmatchings/util", EepromReadSector);
INCLUDE_ASM("asm/nonmatchings/util", EepromWriteSector);
INCLUDE_ASM("asm/nonmatchings/util", EepromVerifySector);
/**
 * SaveGameRetry: attempts write+verify save up to 3 times.
 * Calls EepromWriteSector (write) then EepromVerifySector (verify).
 */
INCLUDE_ASM("asm/nonmatchings/util", SaveGameRetry);
