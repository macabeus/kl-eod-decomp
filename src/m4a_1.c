#include "global.h"
#include "gba.h"
#include "globals.h"
#include "include_asm.h"

/* ══════════════════════════════════════════════════════════════════════
 * m4a_1 — TST compilation unit of the MusicPlayer2000 sound engine
 *
 * These functions were compiled with a TST-capable compiler revision in
 * the original ROM. They use the tst instruction for bitwise flag tests
 * instead of the ands+cmp sequence used by the rest of m4a.
 *
 * This file is compiled with -ftst to match the original code generation.
 * Only decompiled C functions go here; INCLUDE_ASM functions stay in m4a.c
 * since they produce raw assembly unaffected by compiler flags.
 * See GitHub issue #54 for details.
 * ══════════════════════════════════════════════════════════════════════ */

void VoiceGetParams(u32 *);

/**
 * VoiceLookupAndApply: walk linked list of active voices and apply parameters.
 *
 * Iterates through the voice chain starting at info[0x20/4]. For each voice,
 * if any status bits in 0xC7 are set (active/keyon/sustain/release), marks
 * the voice as requiring update (sets bit 0x40). Then calls VoiceGetParams
 * to apply the voice's current parameters. Finally clears the caller's
 * status byte.
 *
 * @param unused   Unused first parameter (register r0 not referenced)
 * @param info     Pointer to sound channel/track structure
 */
void VoiceLookupAndApply(u32 unused, u32 *info) {
    u32 *node = (u32 *)info[0x20 / 4];

    while (node != NULL) {
        u8 status = *(u8 *)node;
        if (status & 0xC7) {
            *(u8 *)node = status | 0x40;
        }
        VoiceGetParams(node);
        node = (u32 *)node[0x34 / 4];
    }

    *(u8 *)info = 0;
}
