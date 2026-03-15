#include "global.h"
#include "include_asm.h"

/*
 * BIOS BitUnPack (SWI 0x0B): unpacks bit-packed data.
 *   r0: source pointer, r1: dest pointer, r2: unpack info pointer
 *   no return value
 */
void BitUnPack(void *src, void *dest, void *info) {
    asm("swi #11");
}

/*
 * BIOS HuffUnComp (SWI 0x13): Huffman decompression.
 *   r0: source pointer, r1: dest pointer
 *   no return value
 */
void UnpackTilemap(void *src, void *dest) {
    asm("swi #19");
}

INCLUDE_ASM("asm/nonmatchings/syscalls", FUN_08051440);
INCLUDE_ASM("asm/nonmatchings/syscalls", FUN_08051444);
INCLUDE_ASM("asm/nonmatchings/syscalls", FUN_08051448);
