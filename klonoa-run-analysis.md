# Klonoa Run Analysis: Why Short Functions Failed at 35.7%

## 1. Function Profiles

| Function | Target instrs | Best diff | Matched? | Attempts | Root cause category |
|----------|:---:|:---:|:---:|:---:|---|
| Abs | 5 | 3 | No | 10 | Compiler idiom |
| FreeGfxBuffer | 8 | 1 | No | 10 | Objdiff false negative |
| MidiDecodeByte | 1 | 2 | No | 10 | Impossible in C |
| StrCmp | ~8 | N/A | No | 10 | Regex bug in Mizuchi |
| StrCpy | 7 | 1 | No | 10 | Alignment padding |
| StreamCmd_StopSound | 8 | 0 | **Yes** | 8 | — |
| VBlankHandlerMinimal | 10 | 2 | No | 10 | Objdiff false negative |
| ply_bend | 9 | 1 | No | 10 | Alignment padding |
| ply_keysh | 9 | 0 | **Yes** | 10 | — (dummy func workaround) |
| ply_lfodl | 9 | 1 | No | 10 | Alignment padding |
| ply_lfos | 6 | 0 | **Yes** | 1 | — (naturally aligned) |
| ply_pan | 9 | 0 | **Yes** | 4 | — (dummy func workaround) |
| ply_voice | 9 | 1 | No | 10 | Alignment padding |
| ply_vol | 9 | 0 | **Yes** | 9 | — (dummy func workaround) |

These functions are genuinely short (1-10 instructions). The complexity is low. **The failures are not caused by function difficulty.**

## 2. Root Cause Classification

| Cause | Count | Functions | Fixable? |
|-------|:---:|-----------|:---:|
| **Thumb alignment padding (`mov r8, r8`)** | 4 | ply_bend, ply_lfodl, ply_voice, StrCpy | Yes — toolchain |
| **Objdiff literal pool false negative** | 2 | FreeGfxBuffer, VBlankHandlerMinimal | Yes — toolchain |
| **Regex bug in `claude-runner-plugin.ts:360`** | 1 | StrCmp | Yes — one-line fix |
| **Impossible function (asm fallthrough)** | 1 | MidiDecodeByte | No |
| **Compiler idiom (`bpl`/`neg` vs `bge`/`mov+sub`)** | 1 | Abs | Maybe — needs research |

**7 of 9 failures are systemic toolchain/infrastructure issues.** Zero are caused by Claude's decompilation ability.

## 3. Detailed Failure Analysis

### Issue 1: Thumb Alignment Padding (4 functions)

**What happens:** When a Thumb function has an odd number of 2-byte instructions (not a multiple of 4 bytes), `arm-none-eabi-as` pads with `mov r8, r8` (0x46C0) to 4-byte-align the next function. In the target object (built from a full source file with many functions), the symbol table correctly bounds each function, so objdiff doesn't include the padding. In the test-compiled object (single function), the `compilerScript` runs `sed -i '' '/\.size/d'` which strips `.size` directives, so objdiff has no way to determine where the function ends and includes the padding.

**Evidence:** ply_bend, ply_lfodl, ply_voice, and StrCpy all have **perfect instruction-level matches** — every real instruction is identical. The ONLY difference is an extra `mov r8, r8` at the end of the compiled output:

```
Difference 1 (DELETION):
- Current: `mov r8, r8` [delete]
- Target:  `(empty)` [delete]
```

**Proof it's solvable:** ply_keysh, ply_pan, and ply_vol are **structurally identical** functions (same 9-instruction pattern, same struct offset pattern) that DID match — because Claude stochastically discovered a workaround: append a dummy function (`void ply_pan_next(void) {}`) that absorbs the padding. Claude found this trick in 3/7 attempts but missed it in 4/7.

**The compiled code is correct.** The decompilation is done. The infrastructure is rejecting it.

### Issue 2: Objdiff Literal Pool Representation (2 functions)

**What happens:** After `bx lr` (return), agbcc emits literal pool entries as `.word` directives. In the target object, objdiff disassembles these data bytes as Thumb instructions (e.g., `0x03007FF8` -> `ldrb r0, [r7, #0x1f]` + `lsl r0, #0xc`). In the compiled object, they're correctly marked as `.word 0x3007ff8`. **The bytes are bit-for-bit identical** but the textual representation differs, causing objdiff to report mismatches.

**FreeGfxBuffer evidence:**
```
Target:  lsl r0, #0x0    <- 0x0000 decoded as instruction
Current: .hword 0x0       <- same 0x0000 as data
```

**VBlankHandlerMinimal evidence:**
```
Target:  ldrb r0, [r7, #0x1f] / lsl r0, #0xc  <- 0x03007FF8 as two instructions
Current: .word 0x3007ff8                         <- same bytes as data
```

Claude correctly identified these as false negatives in attempt 1:
> *"The 2 differences are just the literal pool representation -- the bytes of 0x03007FF8 displayed as two Thumb instructions vs a .word. These are identical bytes, just displayed differently by the disassembler."*

Yet the pipeline burned 10 attempts each trying to "fix" something that was already correct.

### Issue 3: Regex Bug in `claude-runner-plugin.ts:360` (1 function)

**The bug:** Line 360 validates extracted code with:
```js
const hasFunctionPattern = /\w+\s+\w+\s*\([^)]*\)\s*\{/.test(code);
```

This regex requires `<word> <word>(...)` — it rejects pointer return types like `u8 *StrCmp(...)` because `*` is not a `\w` character. Every attempt that returned `u8 *StrCmp(...)` or `const u8 *StrCmp(...)` was rejected with "Invalid code structure: No function definition found."

**Impact:** The compiler never ran once across 10 attempts. Claude's code compiled fine inside the `compile_and_view_assembly` MCP tool — Claude was actively iterating on register allocation differences (cmp r2 vs r3) and had near-matches. But the post-extraction validation killed every submission. $5.52 and 43 minutes wasted.

### Issue 4: MidiDecodeByte — Impossible in C

The target assembly is literally ONE instruction:
```asm
MidiDecodeByte:
    ldr r2, [r1, #0x40]
; falls through to FUN_0804f75a
```

`MidiDecodeByte` loads a pointer and falls through to the next function `FUN_0804f75a`. This is not expressible in C — any C function must have a return (`bx lr`) or a call (`bl`), neither of which appears in the target. Claude correctly diagnosed this by attempt 10:

> *"The push/lr and bl are unavoidable when calling a function in C. The target only has one instruction because MidiDecodeByte falls through directly to FUN_0804f75a in assembly -- this is unrepresentable in pure C without asm()."*

**This function should be excluded from automated runs or handled by the integrator as hand-written assembly.**

### Issue 5: Abs — Compiler Idiom Mismatch

Target: `cmp r0, #0; bpl; mov r1, #0; sub r0, r1, r0`
Compiled: `cmp r0, #0; bge; neg r0, r0`

Two mismatches:
1. `bpl` vs `bge` — different condition codes (both correct for `>= 0`, but different encodings)
2. `mov+sub` vs `neg` — agbcc optimizes `r1=0; r0=r1-r0` into the single-instruction `neg r0, r0`

Claude tried `register s32 r1 asm("r1")` with explicit assignment, but agbcc still optimizes to `neg`. This may require a different agbcc version or compiler flags, or the original was compiled differently.

## 4. Attempt-by-Attempt Evidence for Failed Functions

### ply_bend (10 attempts, all diff=1, all from `mov r8, r8`)

| Attempt | Diff | Duration | Cost | What happened |
|:---:|:---:|---:|---:|---|
| 1 | 1 | 160s | $0.59 | Perfect code, trailing NOP |
| 2 | 1 | 442s | $0.98 | Same code, tried variations |
| 3 | 1 | 951s | $1.84 | Tried preceding dummy func |
| 4 | 1 | 814s | $1.60 | Stall detected, same result |
| 5 | 1 | 568s | $0.93 | Same result |
| 6 | 1 | 584s | $1.10 | Tried 25 turns, same result |
| 7 | 1 | 369s | $0.59 | Stall, gave up early |
| 8 | 1 | 1201s | $0.09 | Soft timeout (81 turns looping) |
| 9 | 1 | 1110s | $0.08 | Soft timeout (88 turns looping) |
| 10 | 1 | 517s | $0.69 | Stall, same result |

**$8.49 and 2 hours wasted.** The code was correct on attempt 1.

### StrCmp (10 attempts, all rejected by regex)

| Attempt | Compiled? | Code submitted | Error |
|:---:|:---:|---|---|
| 1 | Skipped | `const u8 *StrCmp(...)` | "No function definition found" |
| 2 | Skipped | `u8 *StrCmp(...)` | "No function definition found" |
| 3 | Skipped | `u8 *StrCmp(...)` | "No function definition found" |
| 4-10 | Skipped | All pointer-returning | Same |

Claude's code compiled fine inside the MCP tool. The pipeline's regex rejected it.

### VBlankHandlerMinimal (10 attempts, interleaved with TTFT timeouts)

| Attempt | Diff | TTFT timeout? | Duration | Notes |
|:---:|:---:|:---:|---:|---|
| 1 | 2 | No | 35s | Correct code, false negative |
| 2 | 2 | No | 740s | Tried variations, same literal pool issue |
| 3 | N/A | **Yes** | 182s | API never responded |
| 4 | 2 | No | 26s | Same correct code |
| 5 | 2 | No | 1044s | $2.07 trying alternatives |
| 6 | N/A | **Yes** | 181s | API timeout |
| 7 | 2 | No | 44s | Same correct code |
| 8 | N/A | **Yes** | 180s | API timeout |
| 9 | 2 | No | 26s | Same correct code |
| 10 | 2 | No | 948s | $1.93, same literal pool issue |

3 of 10 attempts were TTFT timeouts (API never responded). The other 7 all produced correct code that objdiff falsely rejected.

## 5. Token and Cost Analysis

| Metric | Value |
|--------|-------|
| **Total cost** | **$86.17** |
| Total output tokens | 2.36M |
| Total input (cache read) | 37.2M |
| Cache creation | 4.5M |
| Model | claude-sonnet-4-6 |
| Total pipeline time | 793 minutes (13.2 hours) |
| Average TTFT | 51.2 seconds |
| TTFT timeouts | 6/122 attempts (4.9%) |
| Soft timeouts | 4/122 (3.3%) |
| Stall detections | 33/122 (27%) |
| Output throughput | 50.8 tokens/sec |

### Cost wasted on systemic issues

| Issue | Functions | Cost wasted | Time wasted |
|-------|:---:|---:|---:|
| Alignment padding | 4 | ~$30.61 | ~155 min |
| Objdiff false negative | 2 | ~$12.42 | ~112 min |
| Regex bug | 1 | $5.52 | 43 min |
| MidiDecodeByte (impossible) | 1 | $10.50 | 100 min |
| **Total wasted** | **8** | **~$59.05** | **~410 min** |

**68.5% of the total cost was spent on functions that couldn't possibly match due to toolchain issues**, not Claude's ability.

## 6. Comparison to SA3 Easy Functions

SA3 (also GBA/agbcc) achieved ~77% match rate. Key differences:

| Factor | SA3 | Klonoa |
|--------|-----|--------|
| Matched function examples | Many (mature project, pokeemerald-derived) | Few (newer project, limited examples) |
| `.size` handling | Likely retained or handled correctly | Removed by `sed`, causing padding mismatch |
| Objdiff literal pool | Not an issue (or worked around) | Causes 2 false negatives |
| Code validation regex | N/A or broader | Rejects pointer return types |
| Impossible functions | Probably excluded | Included (MidiDecodeByte) |
| Example quality | Domain-relevant (similar struct patterns) | Mixed (ReadUnaligned* examples for sound driver functions) |
| Model | claude-sonnet-4-5 | claude-sonnet-4-6 |

The prompts themselves are reasonable — each has 4-5 examples with paired C/asm. But the example functions are generic utilities (ReadUnaligned*, FixedMul8) rather than structurally similar sound driver functions. The matched `ply_*` functions that DID succeed all discovered the struct pattern by reading the codebase via MCP tools — the prompt examples weren't directly helpful.

## 7. Is the Project Setup Correct?

**Partially. Three fixable issues are causing most failures:**

### Fix 1: Stop removing `.size` directives (HIGH — unblocks 4 functions)

The `compilerScript` runs `sed -i '' '/\.size/d' "$ASM_FILE"`. This removes symbol size metadata from the test-compiled object. Without it, objdiff can't determine function boundaries, so Thumb alignment padding (`mov r8, r8`) gets attributed to the function and compared against the target (which has proper boundaries from its multi-function object file).

**Options:**
- Remove the `sed` line entirely (if agbcc `.size` values are correct)
- If `.size` must be removed for other reasons, add a trailing dummy symbol in the compile script (e.g., append `.thumb_func\n_end_of_test:\n.space 0` to the assembly)
- Configure objdiff to ignore trailing NOP padding

### Fix 2: Fix the function validation regex (HIGH — unblocks 1+ functions)

`claude-runner-plugin.ts:360`:
```js
// Current (broken for pointer return types):
const hasFunctionPattern = /\w+\s+\w+\s*\([^)]*\)\s*\{/.test(code);

// Should also match: u8 *Func(), const u8 *Func(), struct Foo *Func(), etc.
```

### Fix 3: Handle literal pool comparison in objdiff (MEDIUM — unblocks 2 functions)

The data-vs-instruction representation difference for literal pool entries after `bx lr` causes false negatives. Both FreeGfxBuffer and VBlankHandlerMinimal produce **bit-identical binaries** that objdiff rejects.

**Options:**
- Compare at binary level rather than disassembly text for data sections
- Strip literal pool entries from comparison
- Use a custom objdiff post-processor that normalizes data representations

### Fix 4: Exclude impossible functions (LOW — saves cost)

MidiDecodeByte is a single-instruction fallthrough function. No C code can reproduce it. Detect these in the prompt builder (function body = 1 instruction, no return) and skip them.

## 8. Prioritized Fix List

| Priority | Fix | Functions unblocked | Cost saved | Effort |
|:---:|---|:---:|---:|---|
| 1 | **Stop stripping `.size`** or add dummy symbol | 4 (ply_bend, ply_lfodl, ply_voice, StrCpy) | ~$31 | Low |
| 2 | **Fix regex for pointer return types** | 1+ (StrCmp, any future pointer-returning fn) | ~$6+ | Trivial |
| 3 | **Fix objdiff literal pool comparison** | 2 (FreeGfxBuffer, VBlankHandlerMinimal) | ~$12 | Medium |
| 4 | **Exclude impossible functions** | 0 (saves cost only) | ~$11 | Low |

**Fixing just #1 and #2 would raise the match rate from 35.7% to at least 64.3% (9/14).** Fixing all four would reach **85.7% (12/14)** — in line with the SA3 benchmark for easy functions.

## Bottom Line

**The low success rate is not caused by Claude failing to decompile these functions.** In 7 of 9 failures, Claude produced correct or near-correct code that was rejected by infrastructure issues. The toolchain setup has three bugs: alignment padding attribution, a regex that rejects pointer return types, and literal pool comparison at text level instead of binary level. Fixing these would bring the Klonoa run in line with (or above) the published benchmark rates.
