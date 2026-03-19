#ifndef INCLUDE_ASM_H
#define INCLUDE_ASM_H

#ifndef INCLUDE_ASM
#define INCLUDE_ASM(FOLDER, NAME)                                                                                                     \
    asm(".syntax unified\n"                                                                                                           \
        ".include \"" FOLDER "/" #NAME ".s\"\n"                                                                                       \
        ".syntax divided\n")
#endif

/* INCLUDE_ASM for use with -ffunction-sections: places each function in its own
   .text.NAME section so the linker script can order functions independently. */
#define INCLUDE_ASM_SECTION(FOLDER, NAME)                                                                                             \
    asm(".section .text." #NAME ",\"ax\",%progbits\n"                                                                                 \
        ".syntax unified\n"                                                                                                           \
        ".include \"" FOLDER "/" #NAME ".s\"\n"                                                                                       \
        ".syntax divided\n")

// Set up macros once per translation unit
asm(".include \"asm/macros.inc\"\n");

#endif // INCLUDE_ASM_H
