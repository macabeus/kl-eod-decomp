#include "global.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/util", SoftReset);
INCLUDE_ASM("asm/nonmatchings/util", EepromTimerCallback);
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
