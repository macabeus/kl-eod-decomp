#!/usr/bin/env python3
"""
Automated matcher for EntityItemDrop.
Tries many C source variations and checks ROM diff.
"""

import subprocess
import sys
import os
import struct

PROJ = "/Users/macabeus/ApenasMeu/decompiler/klonoa-empire-of-dreams"
SRC = os.path.join(PROJ, "src/code_1.c")
BASEROM = os.path.join(PROJ, "baserom.gba")
BUILT_ROM = os.path.join(PROJ, "klonoa-eod.gba")
OFFSET = 0x1F4D0
LENGTH = 0x174

# Read the original source
with open(SRC, "r") as f:
    original_src = f.read()

# Read target bytes
with open(BASEROM, "rb") as f:
    f.seek(OFFSET)
    target_bytes = f.read(LENGTH)

def inject_function(func_code):
    """Replace the INCLUDE_ASM for EntityItemDrop with actual C code."""
    marker = 'INCLUDE_ASM("asm/nonmatchings/code_1", EntityItemDrop);'
    if marker not in original_src:
        print("ERROR: Could not find INCLUDE_ASM marker!")
        sys.exit(1)
    return original_src.replace(marker, func_code)

def build():
    """Build and return (success, stderr)."""
    result = subprocess.run(
        ["bash", "-c", f"cd {PROJ} && rm -f build/src/code_1.o && make build/src/code_1.o 2>&1"],
        capture_output=True, text=True, timeout=60
    )
    return result.returncode == 0, result.stdout + result.stderr

def build_rom():
    """Full ROM build."""
    result = subprocess.run(
        ["bash", "-c", f"cd {PROJ} && rm -f build/src/code_1.o && make compare 2>&1"],
        capture_output=True, text=True, timeout=120
    )
    return result.returncode == 0, result.stdout + result.stderr

def count_diffs():
    """Compare built ROM against baserom at the function offset."""
    try:
        with open(BUILT_ROM, "rb") as f:
            f.seek(OFFSET)
            built_bytes = f.read(LENGTH)
    except FileNotFoundError:
        return 9999

    diffs = 0
    for i in range(min(len(target_bytes), len(built_bytes))):
        if target_bytes[i] != built_bytes[i]:
            diffs += 1
    return diffs

def show_diffs(max_show=30):
    """Show detailed byte diffs."""
    try:
        with open(BUILT_ROM, "rb") as f:
            f.seek(OFFSET)
            built_bytes = f.read(LENGTH)
    except FileNotFoundError:
        return "ROM not found"

    lines = []
    count = 0
    for i in range(min(len(target_bytes), len(built_bytes))):
        if target_bytes[i] != built_bytes[i]:
            lines.append(f"  offset 0x{i:03X}: target=0x{target_bytes[i]:02X} built=0x{built_bytes[i]:02X}")
            count += 1
            if count >= max_show:
                lines.append(f"  ... and more")
                break
    return "\n".join(lines)

def try_variation(name, func_code):
    """Try a variation, return diff count."""
    src = inject_function(func_code)
    with open(SRC, "w") as f:
        f.write(src)

    ok, output = build()
    if not ok:
        print(f"  [{name}] BUILD FAILED")
        if "error:" in output:
            for line in output.split("\n"):
                if "error:" in line:
                    print(f"    {line.strip()}")
                    break
        return 9999

    # Need full ROM for comparison
    ok2, output2 = build_rom()
    if not ok2:
        # Try to count diffs even if sha1 fails
        diffs = count_diffs()
        print(f"  [{name}] SHA1 mismatch, diffs={diffs}")
        if diffs <= 20:
            print(show_diffs())
        return diffs
    else:
        print(f"  [{name}] MATCH! 0 diffs!")
        return 0

def restore():
    with open(SRC, "w") as f:
        f.write(original_src)

# ============================================================
# VARIATIONS
# ============================================================

variations = {}

# Key observations from the target assembly:
#
# PROLOGUE: push {r4, r5, lr}
# r4 = slot (arg0 << 24 >> 24)
# r5 = itemType ((arg0 << 24) + (0xE4 << 24)) >> 24  (= arg0 + 0xE4, truncated to u8)
#
# Early return path (gGameFlagsPtr[0x0A] == 1):
#   ldr r1, =0x03002920     ; base loaded into r1
#   r0 = r4*7*4             ; slot * 28
#   r0 += r1                ; entityBase
#   store 0x1C to [r0+0x0F], 0 to [r0+0x10]
#   branch to end
#
# Main path:
#   ldr r0, =0x03002920     ; base in r0
#   r2 = r4 << 3            ; slot*8
#   r1 = r2 - r4            ; slot*7
#   r1 = r1 << 2            ; slot*28
#   r3 = r1 + r0            ; entityBase in r3
#   r1 = [r3 + 0x0F]        ; state
#   mov r12, r0              ; save base to r12
#   switch on r1
#
# Case 3 (at _0801F52A):
#   entityBase[0x0F] = 0
#   entityBase[0x14..0x15] = 0
#   entityBase[0x10] = 1
#   entityBase[0x0C] &= 0xFC  (mask = 1-5 = 0xFC)
#   dir = (r12[0x204] >> 2) & 3   ; that's gEntityArray[0x204] bits 3:2
#   if dir == 0: entityBase[0x00] = r12[0x1F8..0x1F9] + 16
#   else:        entityBase[0x00] = r12[0x1F8..0x1F9] - 16
#   entityBase = (slot*7)*4 + r12   (recomputed)
#   entityBase[0x02] = r12[0x1FA..0x1FB]
#   romTable = 0x080E2ADE
#   entityBase[0x08] = romTable[itemType*2]
#   entityBase[0x09] = romTable[itemType*2 + 1]
#   entityBase[0x16] = 4
#
# Case 4 (at _0801F590):
#   Same as case 3 but:
#   romTable access uses +0x0A offset: romTable[itemType*2 + 0x0A]
#   romTable[0x09] uses +0x0B offset: romTable[itemType*2 + 0x0B]
#   entityBase[0x16] = 2
#
# Case 0 (at _0801F5FC):
#   amplitude = (s8)entityBase[0x09]
#   sineTable = 0x080D8E14
#   phase = entityBase[0x14..0x15]
#   sineVal = sineTable[phase]
#   tmp = amplitude * sineVal >> 8
#   entityBase[0x02] = 0x10C - tmp   (0x86<<1 = 0x10C)
#   entityBase[0x00] += (s8)entityBase[0x08]
#   phase += entityBase[0x16]
#   entityBase[0x14] = phase
#   if (u16)phase == 0x88:
#     entityBase[0x0F] = 0x1C
#     entityBase[0x10] = 0

# Let me analyze the exact register allocation in the target more carefully.
#
# In the early return path:
#   ldr r0, =0x03005400   ; gGameFlagsPtr
#   ldrb r0, [r0, #0x0A]  ; gGameFlagsPtr[0x0A]
#   cmp r0, #1
#   bne main_path
#   ldr r1, =0x03002920   ; entityBase loaded into r1 FIRST
#   lsls r0, r4, #3       ; then offset computed in r0
#   subs r0, r0, r4
#   lsls r0, r0, #2
#   adds r0, r0, r1       ; entityBase in r0
#
# In main path:
#   ldr r0, =0x03002920   ; base in r0
#   lsls r2, r4, #3       ; slot*8 in r2
#   subs r1, r2, r4       ; slot*7 in r1
#   lsls r1, r1, #2       ; slot*28 in r1
#   adds r3, r1, r0       ; entityBase in r3
#   ldrb r1, [r3, #0x0F]  ; state in r1
#   mov r12, r0            ; save base

# The tricky part is getting the base address loaded BEFORE the offset computation
# in the early return path. Normally agbcc loads base AFTER computing offset.
# The asm barrier trick can force this.
#
# Also: r2 is preserved across case 3/4 for recomputing entityBase.
# In target case 3: line 78: subs r1, r2, r4  (r2 still has slot*8 from main path entry)
# That means the compiler keeps r2 = slot*8 alive across the case body!

# Base version: straightforward translation
variations["v01_base"] = r"""
void EntityItemDrop(u8 arg0) {
    register u8 slot asm("r4");
    register u8 itemType asm("r5");
    u8 *entityBase;
    u8 state;

    {
        u32 shifted = (u32)arg0 << 24;
        slot = shifted >> 24;
        itemType = (shifted + ((u32)0xE4 << 24)) >> 24;
    }
    if (gGameFlagsPtr[0x0A] == 1) {
        entityBase = ((u8 *)0x03002920) + (slot * 28);
        entityBase[0x0F] = 0x1C;
        entityBase[0x10] = 0;
        return;
    }
    entityBase = ((u8 *)0x03002920) + (((slot * 8) - slot) * 4);
    state = entityBase[0x0F];
    switch (state) {
    case 3: {
        u8 *new_var = (u8 *)0x03002920;
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = 1;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            entityBase[0x00] = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            entityBase[0x00] = *(u16 *)&new_var[0x1F8] - 16;
        }
        *(u16 *)entityBase = *(u16 *)entityBase;
        entityBase = ((u8 *)0x03002920) + (((slot * 8) - slot) * 4);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2];
        entityBase[0x09] = romTable[itemType * 2 + 1];
        entityBase[0x16] = 4;
        break;
    }
    case 4: {
        u8 *new_var = (u8 *)0x03002920;
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = 1;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            entityBase[0x00] = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            entityBase[0x00] = *(u16 *)&new_var[0x1F8] - 16;
        }
        *(u16 *)entityBase = *(u16 *)entityBase;
        entityBase = ((u8 *)0x03002920) + (((slot * 8) - slot) * 4);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2 + 0x0A];
        entityBase[0x09] = romTable[itemType * 2 + 0x0B];
        entityBase[0x16] = 2;
        break;
    }
    case 0: {
        s32 amplitude;
        s32 sineVal;
        s16 *sineTable;
        u16 phase;
        amplitude = (s8)entityBase[0x09];
        sineTable = (s16 *)0x080D8E14;
        phase = *(u16 *)&entityBase[0x14];
        sineVal = sineTable[phase];
        *(u16 *)&entityBase[0x02] = 0x10C - ((amplitude * sineVal) >> 8);
        *(u16 *)entityBase += (s8)entityBase[0x08];
        phase += entityBase[0x16];
        *(u16 *)&entityBase[0x14] = phase;
        if ((u16)phase == 0x88) {
            entityBase[0x0F] = 0x1C;
            entityBase[0x10] = 0;
        }
        break;
    }
    }
}
"""

# Version 2: Use asm barrier for early return base address ordering
variations["v02_barrier_early"] = r"""
void EntityItemDrop(u8 arg0) {
    register u8 slot asm("r4");
    register u8 itemType asm("r5");
    u8 *entityBase;
    u8 state;

    {
        u32 shifted = (u32)arg0 << 24;
        slot = shifted >> 24;
        itemType = (shifted + ((u32)0xE4 << 24)) >> 24;
    }
    if (gGameFlagsPtr[0x0A] == 1) {
        u8 *base;
        asm("" : "=r"(base) : "0"((u8 *)0x03002920));
        entityBase = base + (slot * 28);
        entityBase[0x0F] = 0x1C;
        entityBase[0x10] = 0;
        return;
    }
    entityBase = ((u8 *)0x03002920) + (((slot * 8) - slot) * 4);
    state = entityBase[0x0F];
    switch (state) {
    case 3: {
        u8 *new_var = (u8 *)0x03002920;
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = 1;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            entityBase[0x00] = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            entityBase[0x00] = *(u16 *)&new_var[0x1F8] - 16;
        }
        *(u16 *)entityBase = *(u16 *)entityBase;
        entityBase = ((u8 *)0x03002920) + (((slot * 8) - slot) * 4);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2];
        entityBase[0x09] = romTable[itemType * 2 + 1];
        entityBase[0x16] = 4;
        break;
    }
    case 4: {
        u8 *new_var = (u8 *)0x03002920;
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = 1;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            entityBase[0x00] = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            entityBase[0x00] = *(u16 *)&new_var[0x1F8] - 16;
        }
        *(u16 *)entityBase = *(u16 *)entityBase;
        entityBase = ((u8 *)0x03002920) + (((slot * 8) - slot) * 4);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2 + 0x0A];
        entityBase[0x09] = romTable[itemType * 2 + 0x0B];
        entityBase[0x16] = 2;
        break;
    }
    case 0: {
        s32 amplitude;
        s32 sineVal;
        s16 *sineTable;
        u16 phase;
        amplitude = (s8)entityBase[0x09];
        sineTable = (s16 *)0x080D8E14;
        phase = *(u16 *)&entityBase[0x14];
        sineVal = sineTable[phase];
        *(u16 *)&entityBase[0x02] = 0x10C - ((amplitude * sineVal) >> 8);
        *(u16 *)entityBase += (s8)entityBase[0x08];
        phase += entityBase[0x16];
        *(u16 *)&entityBase[0x14] = phase;
        if ((u16)phase == 0x88) {
            entityBase[0x0F] = 0x1C;
            entityBase[0x10] = 0;
        }
        break;
    }
    }
}
"""

# Version 3: Try using gEntityArray extern + new_var for r12 pattern
# The target uses `mov r12, r0` to save the base address.
# That pattern typically comes from: store base to a variable, then reuse it.
variations["v03_new_var_before_switch"] = r"""
void EntityItemDrop(u8 arg0) {
    register u8 slot asm("r4");
    register u8 itemType asm("r5");
    u8 *entityBase;
    u8 state;
    u8 *new_var;

    {
        u32 shifted = (u32)arg0 << 24;
        slot = shifted >> 24;
        itemType = (shifted + ((u32)0xE4 << 24)) >> 24;
    }
    if (gGameFlagsPtr[0x0A] == 1) {
        u8 *base;
        asm("" : "=r"(base) : "0"((u8 *)0x03002920));
        entityBase = base + (slot * 28);
        entityBase[0x0F] = 0x1C;
        entityBase[0x10] = 0;
        return;
    }
    entityBase = ((u8 *)0x03002920) + (((slot * 8) - slot) * 4);
    state = entityBase[0x0F];
    new_var = (u8 *)0x03002920;
    switch (state) {
    case 3: {
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = 1;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            entityBase[0x00] = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            entityBase[0x00] = *(u16 *)&new_var[0x1F8] - 16;
        }
        *(u16 *)entityBase = *(u16 *)entityBase;
        entityBase = new_var + (((slot * 8) - slot) * 4);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2];
        entityBase[0x09] = romTable[itemType * 2 + 1];
        entityBase[0x16] = 4;
        break;
    }
    case 4: {
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = 1;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            entityBase[0x00] = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            entityBase[0x00] = *(u16 *)&new_var[0x1F8] - 16;
        }
        *(u16 *)entityBase = *(u16 *)entityBase;
        entityBase = new_var + (((slot * 8) - slot) * 4);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2 + 0x0A];
        entityBase[0x09] = romTable[itemType * 2 + 0x0B];
        entityBase[0x16] = 2;
        break;
    }
    case 0: {
        s32 amplitude;
        s32 sineVal;
        s16 *sineTable;
        u16 phase;
        amplitude = (s8)entityBase[0x09];
        sineTable = (s16 *)0x080D8E14;
        phase = *(u16 *)&entityBase[0x14];
        sineVal = sineTable[phase];
        *(u16 *)&entityBase[0x02] = 0x10C - ((amplitude * sineVal) >> 8);
        *(u16 *)entityBase += (s8)entityBase[0x08];
        phase += entityBase[0x16];
        *(u16 *)&entityBase[0x14] = phase;
        if ((u16)phase == 0x88) {
            entityBase[0x0F] = 0x1C;
            entityBase[0x10] = 0;
        }
        break;
    }
    }
}
"""

# Version 4: strh for x position instead of byte write, and don't do the NOP assign
# In the target, after the if/else, we see:
#   strh r0, [r3, #0x00]    ; store halfword to entityBase[0]
# This is a strh, so entityBase x-pos is a u16 write.
# Then it recomputes entityBase = (slot*8-slot)*4 + base
# Let's try storing via strh directly
variations["v04_strh_xpos"] = r"""
void EntityItemDrop(u8 arg0) {
    register u8 slot asm("r4");
    register u8 itemType asm("r5");
    u8 *entityBase;
    u8 state;
    u8 *new_var;

    {
        u32 shifted = (u32)arg0 << 24;
        slot = shifted >> 24;
        itemType = (shifted + ((u32)0xE4 << 24)) >> 24;
    }
    if (gGameFlagsPtr[0x0A] == 1) {
        u8 *base;
        asm("" : "=r"(base) : "0"((u8 *)0x03002920));
        entityBase = base + (slot * 28);
        entityBase[0x0F] = 0x1C;
        entityBase[0x10] = 0;
        return;
    }
    entityBase = ((u8 *)0x03002920) + (((slot * 8) - slot) * 4);
    state = entityBase[0x0F];
    new_var = (u8 *)0x03002920;
    switch (state) {
    case 3: {
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = 1;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] - 16;
        }
        entityBase = new_var + (((slot * 8) - slot) * 4);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2];
        entityBase[0x09] = romTable[itemType * 2 + 1];
        entityBase[0x16] = 4;
        break;
    }
    case 4: {
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = 1;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] - 16;
        }
        entityBase = new_var + (((slot * 8) - slot) * 4);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2 + 0x0A];
        entityBase[0x09] = romTable[itemType * 2 + 0x0B];
        entityBase[0x16] = 2;
        break;
    }
    case 0: {
        s32 amplitude;
        s32 sineVal;
        s16 *sineTable;
        u16 phase;
        amplitude = (s8)entityBase[0x09];
        sineTable = (s16 *)0x080D8E14;
        phase = *(u16 *)&entityBase[0x14];
        sineVal = sineTable[phase];
        *(u16 *)&entityBase[0x02] = 0x10C - ((amplitude * sineVal) >> 8);
        *(u16 *)entityBase += (s8)entityBase[0x08];
        phase += entityBase[0x16];
        *(u16 *)&entityBase[0x14] = phase;
        if ((u16)phase == 0x88) {
            entityBase[0x0F] = 0x1C;
            entityBase[0x10] = 0;
        }
        break;
    }
    }
}
"""

# Version 5: Looking at the target more carefully...
# In case 3, after the if/else for x-pos:
#   strh r0, [r3, #0x00]   ; this stores to entityBase (r3)
# Then:
#   subs r1, r2, r4         ; r1 = slot*8 - slot = slot*7
#   lsls r1, r1, #2         ; r1 = slot*28
#   add r1, r12             ; r1 = base + slot*28 (entityBase recomputed in r1)
#
# So after the strh to entityBase[0], the compiler recomputes entityBase
# into r1 (not r3). This is because the compiler decided to put the
# recomputed address into a different register.
#
# Then it uses r1 for the rest: strh r0, [r1, #2], strb r0, [r1, #8], etc.
#
# The `subs r1, r2, r4` uses r2 which still holds slot*8 from the main path entry.
# This means r2 is live across the case 3 body!
#
# Looking at case 4 more carefully, it's the same pattern but with different romTable offsets.
# In case 4, the romTable access is:
#   ldr r2, =0x080E2ADE
#   lsls r3, r5, #1         ; itemType * 2
#   adds r0, r2, #0         ; r0 = romTable
#   adds r0, #0x0A           ; r0 = romTable + 10
#   adds r0, r3, r0          ; r0 = romTable + 10 + itemType*2
#   ldrb r0, [r0]
#
# vs case 3:
#   ldr r2, =0x080E2ADE
#   lsls r3, r5, #1
#   adds r0, r3, r2          ; r0 = romTable + itemType*2
#   ldrb r0, [r0]
#   adds r2, #1              ; r2 = romTable + 1
#   adds r3, r3, r2          ; r3 = romTable + 1 + itemType*2
#   ldrb r0, [r3]
#
# So case 3 uses romTable[itemType*2 + 0] and romTable[itemType*2 + 1]
# Case 4 uses romTable[itemType*2 + 0x0A] and romTable[itemType*2 + 0x0B]

# The difference in code gen between case 3 and case 4 romTable access is interesting.
# Case 3: direct offset 0 and 1 → uses adds r0, r3, r2 then adds r2, #1
# Case 4: offset 0x0A and 0x0B → uses adds r0, r2, #0 / adds r0, #0x0A / adds r0, r3, r0

# For case 4, the compiler generates:
#   adds r0, r2, #0    ; copy romTable to r0
#   adds r0, #0x0A     ; r0 = romTable + 10
#   adds r0, r3, r0    ; r0 = itemType*2 + romTable + 10
# This is the pattern for (romTable + 10)[itemType * 2] or romTable[itemType*2 + 10]

# Let's try a version where case 4 uses a different table pointer
# romTable + 10 as a separate expression

# Actually wait, let me re-examine the permuter trick mentioned:
# "entityBase[0x09] = romTable[(itemType * 2) + (slot = 1)]"
# This overwrites slot to 1, and case 4 uses entityBase[0x10] = slot (which is now 1).

# But in case 3 and case 4, the target has entityBase[0x10] = 1 explicitly
# (movs r0, #1 / strb r0, [r3, #0x10]). So the permuter trick may not be needed
# for the actual output, just for getting the compiler to produce the right code.

# Let me look at this differently. The key challenge is the 140 diffs.
# Let me first just compile the base version and see what diffs we get.

# Version 5: Try with the slot=1 permuter trick for case 3
variations["v05_slot1_trick_case3"] = r"""
void EntityItemDrop(u8 arg0) {
    register u8 slot asm("r4");
    register u8 itemType asm("r5");
    u8 *entityBase;
    u8 state;
    u8 *new_var;

    {
        u32 shifted = (u32)arg0 << 24;
        slot = shifted >> 24;
        itemType = (shifted + ((u32)0xE4 << 24)) >> 24;
    }
    if (gGameFlagsPtr[0x0A] == 1) {
        u8 *base;
        asm("" : "=r"(base) : "0"((u8 *)0x03002920));
        entityBase = base + (slot * 28);
        entityBase[0x0F] = 0x1C;
        entityBase[0x10] = 0;
        return;
    }
    entityBase = ((u8 *)0x03002920) + (((slot * 8) - slot) * 4);
    state = entityBase[0x0F];
    new_var = (u8 *)0x03002920;
    switch (state) {
    case 3: {
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = 1;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] - 16;
        }
        entityBase = new_var + (((slot * 8) - slot) * 4);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2];
        entityBase[0x09] = romTable[(itemType * 2) + (slot = 1)];
        entityBase[0x16] = 4;
        break;
    }
    case 4: {
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = slot;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] - 16;
        }
        entityBase = new_var + (((slot * 8) - slot) * 4);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2 + 0x0A];
        entityBase[0x09] = romTable[itemType * 2 + 0x0B];
        entityBase[0x16] = 2;
        break;
    }
    case 0: {
        s32 amplitude;
        s32 sineVal;
        s16 *sineTable;
        u16 phase;
        amplitude = (s8)entityBase[0x09];
        sineTable = (s16 *)0x080D8E14;
        phase = *(u16 *)&entityBase[0x14];
        sineVal = sineTable[phase];
        *(u16 *)&entityBase[0x02] = 0x10C - ((amplitude * sineVal) >> 8);
        *(u16 *)entityBase += (s8)entityBase[0x08];
        phase += entityBase[0x16];
        *(u16 *)&entityBase[0x14] = phase;
        if ((u16)phase == 0x88) {
            entityBase[0x0F] = 0x1C;
            entityBase[0x10] = 0;
        }
        break;
    }
    }
}
"""

# Version 6: Looking at the generated asm more carefully.
# In the main path, the target puts:
#   r0 = base (0x03002920)
#   r2 = slot * 8
#   then r1 = r2 - slot
#   r1 = r1 * 4
#   r3 = r1 + r0  (entityBase)
#
# The key insight: the compiler keeps slot*8 in r2 alive!
# This happens when the expression is (slot << 3) and is used again later.
#
# In C: entityBase = base + ((slot * 8 - slot) * 4)
# The compiler first computes slot*8 into r2, then slot*8 - slot into r1.
# r2 stays alive because it's needed again in the case bodies for recomputing entityBase.
#
# So the "slot * 8" subexpression MUST be reused. Let me try storing it.

# Version 6: explicit slot8 variable
variations["v06_slot8_var"] = r"""
void EntityItemDrop(u8 arg0) {
    register u8 slot asm("r4");
    register u8 itemType asm("r5");
    u8 *entityBase;
    u8 state;
    u8 *new_var;
    u32 slot8;

    {
        u32 shifted = (u32)arg0 << 24;
        slot = shifted >> 24;
        itemType = (shifted + ((u32)0xE4 << 24)) >> 24;
    }
    if (gGameFlagsPtr[0x0A] == 1) {
        u8 *base;
        asm("" : "=r"(base) : "0"((u8 *)0x03002920));
        entityBase = base + (slot * 28);
        entityBase[0x0F] = 0x1C;
        entityBase[0x10] = 0;
        return;
    }
    new_var = (u8 *)0x03002920;
    slot8 = slot * 8;
    entityBase = new_var + ((slot8 - slot) * 4);
    state = entityBase[0x0F];
    switch (state) {
    case 3: {
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = 1;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] - 16;
        }
        entityBase = new_var + ((slot8 - slot) * 4);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2];
        entityBase[0x09] = romTable[itemType * 2 + 1];
        entityBase[0x16] = 4;
        break;
    }
    case 4: {
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = 1;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] - 16;
        }
        entityBase = new_var + ((slot8 - slot) * 4);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2 + 0x0A];
        entityBase[0x09] = romTable[itemType * 2 + 0x0B];
        entityBase[0x16] = 2;
        break;
    }
    case 0: {
        s32 amplitude;
        s32 sineVal;
        s16 *sineTable;
        u16 phase;
        amplitude = (s8)entityBase[0x09];
        sineTable = (s16 *)0x080D8E14;
        phase = *(u16 *)&entityBase[0x14];
        sineVal = sineTable[phase];
        *(u16 *)&entityBase[0x02] = 0x10C - ((amplitude * sineVal) >> 8);
        *(u16 *)entityBase += (s8)entityBase[0x08];
        phase += entityBase[0x16];
        *(u16 *)&entityBase[0x14] = phase;
        if ((u16)phase == 0x88) {
            entityBase[0x0F] = 0x1C;
            entityBase[0x10] = 0;
        }
        break;
    }
    }
}
"""

# Version 7: The `mov r12, r0` pattern.
# In the target, after computing entityBase:
#   adds r3, r1, r0    ; r3 = entityBase
#   ldrb r1, [r3, #0x0F]  ; state
#   mov r12, r0          ; save base to r12
#
# This `mov r12, r0` typically happens when the compiler needs to spill
# a value but has no lo registers free. It uses r12 as a scratch hi register.
# This pattern happens when `new_var` is loaded before the switch and
# the compiler keeps it in r12 across the switch cases.
#
# The key question: does the compiler generate this from `new_var = base`
# being declared before the switch, or from the base address being used
# inside cases?
#
# Looking at case 3: `add r0, r12` is used for new_var[0x204], new_var[0x1F8], etc.
# So r12 holds the base = 0x03002920 throughout.
#
# I think the compiler automatically promotes the base address to r12 when
# it runs out of lo registers. With r4=slot, r5=itemType, r3=entityBase,
# and needing r0-r2 for computation, the base has to go to r12.

# Let me try with explicit slot8 reuse but new_var computed differently

# Version 7: Direct switch on entityBase[0x0F] without separate state var
variations["v07_direct_switch"] = r"""
void EntityItemDrop(u8 arg0) {
    register u8 slot asm("r4");
    register u8 itemType asm("r5");
    u8 *entityBase;
    u8 *new_var;

    {
        u32 shifted = (u32)arg0 << 24;
        slot = shifted >> 24;
        itemType = (shifted + ((u32)0xE4 << 24)) >> 24;
    }
    if (gGameFlagsPtr[0x0A] == 1) {
        u8 *base;
        asm("" : "=r"(base) : "0"((u8 *)0x03002920));
        entityBase = base + (slot * 28);
        entityBase[0x0F] = 0x1C;
        entityBase[0x10] = 0;
        return;
    }
    new_var = (u8 *)0x03002920;
    entityBase = new_var + (((slot << 3) - slot) << 2);
    switch (entityBase[0x0F]) {
    case 3: {
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = 1;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] - 16;
        }
        entityBase = new_var + (((slot << 3) - slot) << 2);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2];
        entityBase[0x09] = romTable[itemType * 2 + 1];
        entityBase[0x16] = 4;
        break;
    }
    case 4: {
        u32 dir;
        u8 *romTable;
        entityBase[0x0F] = 0;
        *(u16 *)&entityBase[0x14] = 0;
        entityBase[0x10] = 1;
        entityBase[0x0C] &= 0xFC;
        dir = (new_var[0x204] >> 2) & 3;
        if (dir == 0) {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] + 16;
        } else {
            *(u16 *)entityBase = *(u16 *)&new_var[0x1F8] - 16;
        }
        entityBase = new_var + (((slot << 3) - slot) << 2);
        *(u16 *)&entityBase[0x02] = *(u16 *)&new_var[0x1FA];
        romTable = (u8 *)0x080E2ADE;
        entityBase[0x08] = romTable[itemType * 2 + 0x0A];
        entityBase[0x09] = romTable[itemType * 2 + 0x0B];
        entityBase[0x16] = 2;
        break;
    }
    case 0: {
        s32 amplitude;
        s32 sineVal;
        s16 *sineTable;
        u16 phase;
        amplitude = (s8)entityBase[0x09];
        sineTable = (s16 *)0x080D8E14;
        phase = *(u16 *)&entityBase[0x14];
        sineVal = sineTable[phase];
        *(u16 *)&entityBase[0x02] = 0x10C - ((amplitude * sineVal) >> 8);
        *(u16 *)entityBase += (s8)entityBase[0x08];
        phase += entityBase[0x16];
        *(u16 *)&entityBase[0x14] = phase;
        if ((u16)phase == 0x88) {
            entityBase[0x0F] = 0x1C;
            entityBase[0x10] = 0;
        }
        break;
    }
    }
}
"""

# ============================================================
# RUN
# ============================================================

best_name = None
best_diffs = 9999

for name in sorted(variations.keys()):
    print(f"\nTrying {name}...")
    diffs = try_variation(name, variations[name])
    if diffs < best_diffs:
        best_diffs = diffs
        best_name = name
    if diffs == 0:
        print(f"\n*** MATCH FOUND: {name} ***")
        break

print(f"\n{'='*60}")
print(f"Best: {best_name} with {best_diffs} diffs")

# Restore original
restore()
