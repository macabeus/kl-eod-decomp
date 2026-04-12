# Integrator Plugin Gaps: Why Matched Functions Failed Integration

## What happened

Mizuchi matched 5 functions: `ply_lfos`, `ply_keysh`, `ply_vol`, `ply_pan`, `StreamCmd_StopSound`. Only `ply_lfos` integrated successfully. The other 4 failed `make compare` (SHA1 checksum mismatch) when their matched code was inserted into the decomp project.

The Integrator plugin's `replaceIncludeAsm()` does a straightforward substitution — it replaces the `INCLUDE_ASM(...)` line with the generated C code. This works for `ply_lfos` but breaks for the other 4 due to three categories of gaps.

## Gap 1: Dummy padding functions

**Affected functions:** `ply_keysh`, `ply_vol`, `ply_pan`

**The problem:** Mizuchi compiles each function in isolation. When a Thumb function has an odd number of instructions (not a multiple of 4 bytes), the assembler adds 2-byte padding. In the test compilation, this padding appears as an extra `mov r8, r8` instruction attributed to the function. To make objdiff report 0 differences, Claude discovered a workaround: append a dummy function that absorbs the padding.

For example, `ply_pan` matched with:
```c
void ply_pan(void *mplay, struct MusicPlayerTrack *track) {
    track->unk2E = track->unk40[0];
    track->unk40++;
}
void ply_pan_next(void) {}   // <-- dummy to absorb padding
```

In the full ROM build, this dummy function generates real code that doesn't exist in the original binary, breaking the checksum.

**The fix applied:** Remove all dummy functions. In the full build, the NEXT function in the .c file naturally follows, so padding is handled by the assembler's `.align 2, 0` directive between compiler-generated functions.

**What the Integrator should do:** Strip any function definitions that aren't the target function. The generated code block should contain ONLY the target function (and any required local type definitions). A heuristic: after extracting the code block, parse it for function definitions and remove any that don't match `functionName`.

## Gap 2: Missing `.align 2, 0` between C functions and INCLUDE_ASM blocks

**Affected functions:** `ply_keysh`, `ply_pan` (any C function followed by a `non_word_aligned` INCLUDE_ASM)

**The problem:** When agbcc compiles a C function, it emits `.align 2, 0` BEFORE each function (providing padding from the previous function). But it does NOT emit `.align 2, 0` AFTER the last C function in a sequence. If a C function is immediately followed by an `INCLUDE_ASM` that uses `non_word_aligned_thumb_func_start` (no alignment), there's no padding between them.

In the original ROM, these functions had 2 bytes of explicit padding (`lsls r0, r0, #0x00` = 0x0000) at the end of their assembly. When compiled from C, this padding disappears, making the function 2 bytes shorter and shifting all subsequent code.

Layout comparison:
```
BEFORE (INCLUDE_ASM):
  ply_keysh asm (18 bytes + 2 bytes padding) + ply_voice asm = correct layout

AFTER (C, no fix):
  ply_keysh C (18 bytes, NO padding) + ply_voice asm = ply_voice shifted 2 bytes
```

**The fix applied:** Add `asm(".align 2, 0");` as a file-scope directive between the C function and the next INCLUDE_ASM:
```c
void ply_keysh(void *r0, TrackStruct *track) {
    track->keyShift = *track->unk40;
    track->unk40++;
}
asm(".align 2, 0");
INCLUDE_ASM("asm/nonmatchings/m4a", ply_voice);
```

**When this is needed:** Only when a C function is followed by an INCLUDE_ASM whose original assembly uses `non_word_aligned_thumb_func_start`. Between two consecutive C functions, the compiler already generates `.align 2, 0`.

**What the Integrator should do:** After replacing `INCLUDE_ASM` with C code, check whether the NEXT line in the file is another `INCLUDE_ASM`. If so, check the corresponding `.s` file for `non_word_aligned_thumb_func_start`. If found, insert `asm(".align 2, 0");` between the new C function and the next `INCLUDE_ASM`.

Detection logic:
1. After inserting C code, read the next non-blank line in the file
2. If it's `INCLUDE_ASM("asm/nonmatchings/<module>", <nextFunc>)`, read `asm/nonmatchings/<module>/<nextFunc>.s`
3. If that file contains `non_word_aligned_thumb_func_start`, emit `asm(".align 2, 0");` after the C function

## Gap 3: Duplicate/conflicting type definitions

**Affected functions:** `ply_keysh`, `ply_vol`, `ply_pan`

**The problem:** Each function's Mizuchi output includes its OWN struct definition with only the fields it uses:
```c
// ply_keysh's output:
struct TrackStruct { u8 unk00[0x24]; u8 keyShift; u8 unk25[0x1B]; u8 *cmdPtr; };

// ply_vol's output:
struct MusicPlayerTrack { u8 unk00[0x2D]; u8 unk2D; u8 filler[0x12]; u8 *unk40; };
```

These are different struct definitions for the same underlying type. When multiple functions are integrated into the same .c file, these conflict. The existing `TrackStruct` (used by `ply_lfos`) has yet another layout.

**The fix applied:** Created a unified `TrackStruct` with all fields at their correct offsets, placed before all the `ply_*` functions:
```c
typedef struct {
    u8 unk00[0x1F];   // 0x00
    u8 unk1F;         // 0x1F (ply_lfos)
    u8 unk20[0x04];   // 0x20
    u8 keyShift;      // 0x24 (ply_keysh)
    u8 unk25[0x08];   // 0x25
    u8 unk2D;         // 0x2D (ply_vol)
    u8 unk2E;         // 0x2E (ply_pan)
    u8 unk2F[0x11];   // 0x2F
    u8 *unk40;        // 0x40 (all ply_* use this)
} TrackStruct;
```

**What the Integrator should do:** The `stripDuplicateDeclarations()` helper already removes extern/forward declarations. It should be extended to also detect and merge struct definitions:
1. Parse struct definitions from the generated code
2. Check if a struct with the same or similar name exists in the target file
3. If so, merge the fields (union of both layouts, preserving offsets)
4. Replace the generated code's struct reference with the existing type name
5. If the existing struct needs new fields, extend it

This is the hardest gap to automate because it requires understanding struct layout semantics, not just text matching.

## Gap 4: Extern declarations and macro conflicts

**Affected function:** `StreamCmd_StopSound`

**The problem:** Mizuchi's matched output included:
```c
extern void StopSoundEffects(void);
#undef gStreamPtr
extern u8 *gStreamPtr;

void StreamCmd_StopSound(void) {
    StopSoundEffects();
    gStreamPtr += 2;
}
```

In the decomp project, `gStreamPtr` is a macro (`#define gStreamPtr (*(u8 **)0x03004D84)`) from `globals.h`. The `#undef` + `extern` breaks the build because it replaces a working macro with an extern that the linker can't resolve.

**The fix applied:** Strip all extern declarations and `#undef` directives. Use `gStreamPtr` directly (it works as a macro). The integrated code is simply:
```c
void StreamCmd_StopSound(void) {
    StopSoundEffects();
    gStreamPtr += 2;
}
```

**What the Integrator should do:** The `stripDuplicateDeclarations()` helper should also:
1. Remove `#undef` directives for symbols that are defined as macros in the project headers
2. Remove `extern` declarations for symbols that are defined as macros (they can't be redeclared as extern variables)
3. Detect when a global variable is actually a macro by checking `ctx.c` or the project headers

## Gap 5: Shared literal pools (StreamCmd_StopSound)

**Affected function:** `StreamCmd_StopSound`

**The problem:** In the original ROM, `StreamCmd_StopSound`'s literal pool (the address `0x03004D84`) is NOT stored within the function — the `ldr r1, [pc, #0x0C]` instruction reads 4 bytes that span across the function boundary into the NEXT function's code. This "shared literal pool" is an artifact of the original compilation.

When compiled from C, the compiler always generates a self-contained literal pool (`.word 0x3004d84`) within the function, making it 4 bytes larger than the original.

**Current status:** `StreamCmd_StopSound` cannot be integrated via simple C replacement due to this literal pool sharing. It remains as `INCLUDE_ASM` for now.

**What the Integrator should do:** Detect this case by comparing the compiled function's size against the original. If the C function is larger than the original assembly, flag it as a literal pool sharing issue and skip integration (or attempt workarounds like manually placing the literal pool).

## Summary of Integrator fixes needed

| Priority | Gap | Fix | Complexity |
|:---:|---|---|:---:|
| 1 | Dummy padding functions | Strip non-target function definitions from generated code | Low |
| 2 | Missing `.align 2, 0` | Detect when C→INCLUDE_ASM boundary needs alignment directive | Medium |
| 3 | Extern/macro conflicts | Extend `stripDuplicateDeclarations` to handle `#undef` and macro-defined globals | Low |
| 4 | Duplicate struct definitions | Merge struct definitions with existing types in the file | High |
| 5 | Shared literal pools | Detect and skip functions with shared literal pools | Medium |

Gaps 1-3 are the most impactful and relatively straightforward to implement. Gap 4 (struct merging) is the hardest but could be approximated by always stripping struct definitions from generated code and requiring the human (or AI build fixer) to handle them.
