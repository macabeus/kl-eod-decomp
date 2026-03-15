#include "global.h"
#include "include_asm.h"

INCLUDE_ASM("asm/nonmatchings/gfx", FUN_080480f4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804832c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_08048498);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_08048768);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_080487b4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804886c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_080491c0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_08049348);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_08049726);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_08049bfc);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_08049efc);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804a070);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804af00);

// Reads a u16 from a potentially unaligned address (byte-by-byte, little-endian)
u16 ReadUnalignedU16(u8 *ptr) {
    return ptr[0] | (ptr[1] << 8);
}

// Reads a s16 from a potentially unaligned address (byte-by-byte, little-endian)
s16 ReadUnalignedS16(u8 *ptr) {
    return (s16)(ptr[0] + (ptr[1] << 8));
}

INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804b270);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804b2a0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804b2ec);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804b424);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804b464);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804b4b0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804b920);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bab4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bad4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bafc);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bb3c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bb74);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bb86);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bb88);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bbc0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bbd4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bd10);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bdb4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804be08);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804be58);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bf7c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804bfd0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c050);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c0be);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c0d4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c0ec);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c1a0);
INCLUDE_ASM("asm/nonmatchings/gfx", sub_0804C1C0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c1fc);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c230);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c24c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c300);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c3a4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c484);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c598);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c60c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c6a8);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c6e0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c798);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c7fc);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c844);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c86c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c898);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c8f4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804c9a8);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804ca6c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804cac8);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804cc4c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804cf26);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804cf80);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804cfd0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d0b0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d32c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d408);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d4d8);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d63c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d798);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d8d4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804d99c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804da60);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804db38);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804dbd4);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804dc64);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804dccc);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804dd48);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804de34);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804debc);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804df80);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e008);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e0e8);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e448);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e568);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e5f0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e634);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e6b6);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e708);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e76e);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e784);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e7a0);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e7d2);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e7fa);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e814);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e850);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e884);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e8fe);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e93c);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e974);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804e9dc);
INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804ea94);
