#ifndef INCLUDE_ASM_H
#define INCLUDE_ASM_H

#ifndef INCLUDE_ASM
#define INCLUDE_ASM(FOLDER, NAME)                                                                                                     \
    asm(".syntax unified\n"                                                                                                           \
        ".include \"" FOLDER "/" #NAME ".s\"\n"                                                                                       \
        ".syntax divided\n")
#endif

// Set up macros once per translation unit
asm(".include \"asm/macros.inc\"\n");

#endif // INCLUDE_ASM_H
