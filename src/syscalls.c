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

/*
 * BIOS LZ77UnCompWram (SWI 0x11): LZ77 decompression to WRAM.
 *   r0: source pointer, r1: dest pointer
 *   no return value
 */
void LZ77UnCompWram(void *src, void *dest) {
    asm("swi #17");
}

/*
 * BIOS Sqrt (SWI 0x08): calculates square root.
 *   r0: input value
 *   returns: square root in r0
 */
u32 BiosSquareRoot(u32 val) {
    asm("swi #8");
}

INCLUDE_ASM("asm/nonmatchings/syscalls", FUN_08051448);
