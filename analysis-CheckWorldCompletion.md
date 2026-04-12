# Analysis: `CheckWorldCompletion` — 30-Attempt Failure on Klonoa: Empire of Dreams

## 1. Function Profile

**Name:** `CheckWorldCompletion`
**Size:** ~190 ARM Thumb instructions
**Compiler:** agbcc (GCC-based, ARM7TDMI Thumb mode, -O2)
**Model used:** claude-sonnet-4-6

**What it does:** This is a **save-data completion checker** for Klonoa's world map. It reads a save structure at `gCurrentEntityCtx` (pointer at `0x03004670`) and determines whether a given world (0–6) is "complete" by checking bit flags and counting specific vision/item states. Returns 1 (complete) or 0 (incomplete).

**Structure:**
- **Worlds 0–3**: Simple two-flag check — verify bit 7 of a flag at offset `0x0F + world*8`, then check if the next slot's lower 7 bits equal `0x7F`
- **Worlds 4–6**: Complex — checks bit 7 at offset `0x37`, then runs a **nested loop** (5 outer × 7 inner) counting items in 3 categories (`r8`=completed/0x64, `r12`=special/0x1E, `r6`=active/bit7), followed by 2 bonus checks at offsets `0x30`/`0x31`, then 3 world-specific threshold comparisons

**What makes it structurally hard:**

1. **Nested loop with conditional `goto`s**: The inner loop body has an if/else-if/goto chain that creates a specific branch structure GCC encodes with a particular register allocation. The inner loop compares `j` against 3, 5, and 7, with a `goto` that skips the "special" check when `j==3 || j==5` matches the 0x64 case:

   ```asm
        ; Inner loop top — j in r2, byteOffset in r3
   109  cmp r2, #0x3
        beq .L8064          ; j==3 → check 0x64
        cmp r2, #0x5
        bne .La080          ; j!=5 → skip to "not special" path
   61   ldr r0, [r4, #0x0]  ; j==3 or j==5: load base ptr
        add r0, #0x8
        add r0, r3          ; base + 8 + byteOffset
        ldrb r1, [r0, #0x0]
        mov r0, r7           ; r7 = 0x7F mask
        and r0, r1
        cmp r0, #0x64
        bne .La080           ; not 0x64 → fall through to "not special"
        mov r0, r8
        add r0, #0x1
        lsl r0, #0x18
        lsr r0, #0x18
        mov r8, r0           ; r8++ (completed count, u8 truncated)
        b .Lbe95             ; GOTO bit7_check — skips the 0x1E test
        .word 0x3004670      ; literal pool embedded mid-loop
   63   cmp r2, #0x7
        beq .Lbe95           ; j==7 → skip 0x1E check entirely
        ldr r0, [r4, #0x0]  ; "not special" path: reload base
        add r0, #0x8
        add r0, r3
        ldrb r1, [r0, #0x0]
        mov r0, r7
        and r0, r1
        cmp r0, #0x1e
        bne .Lbe95
        mov r0, r12
        add r0, #0x1
        lsl r0, #0x18
        lsr r0, #0x18
        mov r12, r0          ; r12++ (special count, u8 truncated)
   ```

   The `goto` after the `r8++` increment (the `b .Lbe95` that skips the 0x1E check) and the literal pool word placed mid-loop are non-obvious patterns that require the C code to use a specific `goto`/label structure to reproduce.

2. **8 live variables across high registers** (r4–r12, r10): GCC must pack r8, r9, r10, r12 (callee-saved high regs) and r4–r7 (low regs), leaving almost no slack for different allocation decisions. The prologue/epilogue and initialization block show this pressure:

   ```asm
        ; Prologue — saves r4-r7 plus r8/r9/r10 via low-reg shuffle
        push {r4, r5, r6, r7, lr}
        mov r7, r10
        mov r6, r9
        mov r5, r8
        push {r5, r6, r7}

        ; Initialization of all 8 counter/state registers
   47   mov r1, #0x0
        mov r8, r1           ; r8 = completed count
        mov r12, r1          ; r12 = special count
        mov r6, #0x0         ; r6 = active/bit7 count
        mov r10, r6          ; r10 = bonus count
        mov r0, #0x0         ; r0 = outer loop var (i), then consumed
        mov r4, r2           ; r4 = saved pointer to gCurrentEntityCtx
        mov r7, #0x7f        ; r7 = constant mask 0x7F

        ; Epilogue — restores all high regs
   36   pop {r3, r4, r5}
        mov r8, r3
        mov r9, r4
        mov r10, r5
        pop {r4, r5, r6, r7}
        pop {r1}
        bx r1
   ```

   Every low register r4–r7 is occupied: r4=base ptr, r5=outer loop next, r6=bit7 count, r7=0x7F mask. Every high register is occupied: r8=completed, r9=world (parameter), r10=bonus, r12=special. Any C code change that introduces an extra temporary can cause GCC to spill to the stack, completely changing the output.

3. **Multiple pointer re-loads from the global**: Each branch of the inner loop re-dereferences `gCurrentEntityCtx` rather than keeping the base pointer in a register — the exact C expression controls whether GCC reuses a register or emits a fresh `ldr`:

   ```asm
        ; 0x64 check path — loads gCurrentEntityCtx fresh
   61   ldr r0, [r4, #0x0]   ; r4 points to &gCurrentEntityCtx
        add r0, #0x8
        add r0, r3
        ldrb r1, [r0, #0x0]
        ...

        ; 0x1E check path — loads gCurrentEntityCtx AGAIN
   63   ...
        ldr r0, [r4, #0x0]   ; same reload
        add r0, #0x8
        add r0, r3
        ldrb r1, [r0, #0x0]
        ...

        ; bit7 check path — loads gCurrentEntityCtx a THIRD time
   77   ldr r0, [r4, #0x0]   ; yet another reload
        add r0, #0x8
        add r0, r3
        ldrb r1, [r0, #0x0]
   ```

   The target dereferences `*(u32 *)0x03004670` three separate times per inner iteration. If the C code caches this pointer in a local variable, GCC may optimize away the reloads and produce different code. The C must use `((u8 *)gCurrentEntityCtx)[0x08 + byteOffset]` (an expression that re-reads the global each time) rather than caching `data = (u8 *)gCurrentEntityCtx` once.

4. **Three separate tail comparisons** (worlds 4, 5, 6) with different return paths that GCC interleaves with a shared `return 1` block placed mid-function:

   ```asm
        ; World 4 check
   134  mov r1, r9           ; r1 = world
        cmp r1, #0x4
        bne .L126147         ; skip if world != 4
        cmp r3, #0x7f        ; r3 = data[0x30] & 0x7F from earlier
        beq .L126147         ; skip if == 0x7F
        cmp r6, #0x23        ; r6 = bit7Count
        beq .L4635           ; bit7Count == 0x23 → return 1

        ; World 5 check
   142  mov r0, r9
        cmp r0, #0x5
        bne .L146164
        ldr r1, [pc, #0x48]  ; reload gCurrentEntityCtx for world 5
        ldr r0, [r1, #0x0]
        add r0, #0x31
        ldrb r1, [r0, #0x0]
        mov r0, #0x7f
        and r0, r1
        cmp r0, #0x7f
        beq .L146164         ; data[0x31] == 0x7F → skip
        mov r0, r12
        add r0, r8           ; r12 + r8 = special + completed
        cmp r0, #0x18
        ble .L146164         ; <= 0x18 → skip
        b .L4635             ; > 0x18 → return 1

        ; World 6 check
   149  mov r0, r9
        cmp r0, #0x6
        bne .L168182         ; skip if world != 6
        ...
        b .L4635             ; → return 1

        ; Shared return 0 (at .L168182)
   22   mov r0, #0x0

        ; Shared return 1 (at .L4635, placed mid-function after worlds 0-3)
   33   mov r0, #0x1
        b .L16a183           ; jump to epilogue
   ```

   The `return 1` block (label `.L4635`) is placed **between** the worlds 0–3 code and the worlds 4–6 code — not at the end. GCC chose this layout to minimize branch distances. Worlds 4, 5, and 6 each branch backward to this shared block. The C code must use `goto return1` or equivalent to make GCC place this block mid-function; a normal `return 1` at each site would produce three separate blocks.

5. **agbcc quirks**: This GCC fork has known divergences from standard GCC register allocation, especially around `u8` truncation patterns. Every `u8` increment is compiled as a 24-bit shift pair:

   ```asm
        ; Pattern: r8 = (u8)(r8 + 1) — appears 5 times in the function
        mov r0, r8
        add r0, #0x1
        lsl r0, #0x18        ; shift left 24 to clear upper bits
        lsr r0, #0x18        ; shift right 24 → zero-extend to u8
        mov r8, r0

        ; Same pattern for r12, r6, r10:
        mov r0, r12
        add r0, #0x1
        lsl r0, #0x18
        lsr r0, #0x18
        mov r12, r0

        ; Special case: r6 uses lsr into r6 directly (different dest reg form)
        add r0, r6, #0x1
        lsl r0, #0x18
        lsr r6, r0, #0x18   ; result goes directly to r6
   ```

   The C code must cast each increment as `(u8)(var + 1)` to trigger this pattern. But subtle differences — like whether the increment uses `var = (u8)(var + 1)` vs `var++` vs `var += 1` — can change whether agbcc emits the shift pair, uses a different truncation idiom, or elides the truncation entirely. The r6 variant (`lsr r6, r0, #0x18` — writing directly to the destination without a `mov`) suggests agbcc treats that particular allocation site differently, possibly because r6 is a low register while r8/r10/r12 are high registers requiring an extra `mov`.

**No similar matched functions** were included in the prompt. The context file contains only type definitions, register defines, and global variable macros — no example decompiled functions.

---

## 2. Attempt-by-Attempt Trajectory

| Att | Diff | Best So Far | Compiled? | Soft TO? | Stall? | Fresh? | Out Tokens | Cost | What Claude Tried | What Went Wrong |
|-----|------|-------------|-----------|----------|--------|--------|-----------|------|-------------------|-----------------|
| 1 | 95 | 95 | YES | YES | no | FRESH | 1,260 | $0.04 | Baseline decompile with struct, `for` loops | Soft TO forced early submit; decent baseline |
| 2 | — | 95 | NO | YES | no | — | 1,168 | $0.04 | Retry with renamed vars, mid-block decls | C99 declarations → compile fail |
| 3 | — | 95 | NO | YES | no | — | 1,129 | $0.03 | Another structural variant | Mid-block `v30` declaration → compile fail |
| 4 | 89 | 89 | YES | no | no | — | 25,314 | $1.01 | Used tools (7 turns), `u32 i,j`, extra vars | Best so far; fixed loop var types |
| 5 | 96 | 89 | YES | YES | no | — | 1,301 | $0.04 | Array struct `visions[5][7][1]` | Regression — different struct layout hurt |
| 6 | — | 89 | NO | YES | YES | — | 1,268 | $0.04 | Stall msg received; tried rewrite | Still mid-block decl errors |
| 7 | — | 89 | NO | YES | no | — | 1,303 | $0.04 | Struct with ptr approach | Compile fail (mid-block `r3val`) |
| 8 | — | 89 | NO | YES | no | — | 1,147 | $0.04 | More variable rearrangement | Compile fail (mid-block decl) |
| 9 | 95 | 89 | YES | no | no | — | 38,657 | $1.40 | Used tools (7 turns), careful analysis | Same 95 as att 1 — no progress |
| 10 | — | 89 | *empty* | YES | no | — | 0 | $0.00 | Empty response (TTFT+soft TO) | Zero output — wasted attempt |
| 11 | **70** | **70** | YES | no | no | FRESH | 46,760 | $1.62 | Fresh start, `gCurrentEntityCtx` macro, `do-while` | Big jump! goto-free, clean structure |
| 12 | 88 | 70 | YES | YES | no | — | 1,173 | $0.03 | Added padding to struct | Regression from 70 to 88 |
| 13 | — | 70 | NO | YES | no | — | 1,108 | $0.03 | Struct variant | Compile fail (`v31` decl) |
| 14 | — | 70 | NO | YES | no | — | 10,632 | $0.35 | Analysis + partial tool use | Compile fail |
| 15 | **61** | **61** | YES | no | no | — | 38,840 | $1.33 | Used tools (8 turns), `goto return1` pattern, `gCurrentEntityCtx` macro | **BEST ATTEMPT** — but reg alloc still off |
| 16 | — | 61 | NO | YES | no | — | 1,218 | $0.04 | Struct rewrite | Compile fail |
| 17 | 78 | 61 | YES | no | no | — | 48,453 | $1.62 | Tools (6 turns), targeted reg matching | Regression from 61 |
| 18 | 82 | 61 | YES | YES | no | — | 1,037 | $0.03 | Struct approach again | Regression |
| 19 | 130 | 61 | YES | YES | YES | — | 1,103 | $0.03 | Stall msg; complete rewrite | **Massive regression** to worst score |
| 20 | — | 61 | *empty* | YES | no | — | 0 | $0.00 | Empty response | Zero output |
| 21 | 74 | 61 | YES | no | no | FRESH | 58,866 | $2.00 | Fresh start, semantic names, `(ctx+0x0F)[world*8]` | Good but couldn't beat 61 |
| 22 | 96 | 61 | YES | YES | no | — | 1,065 | $0.03 | Struct rewrite | Regression to near-baseline |
| 23 | — | 61 | NO | YES | no | — | 1,400 | $0.04 | `visions[5][7][1]` array struct | Compile fail |
| 24 | 73 | 61 | YES | no | no | — | 52,844 | $1.83 | Tools (9 turns), context.h study | Close to best but still 73 |
| 25 | 80 | 61 | YES | YES | no | — | 1,171 | $0.03 | Padded struct | Regression |
| 26 | — | 61 | NO | YES | no | — | 1,106 | $0.03 | Struct variant | Compile fail |
| 27 | — | 61 | NO | YES | no | — | 998 | $0.03 | `visionFlags` struct | Compile fail |
| 28 | **64** | 61 | YES | no | no | — | 51,879 | $1.82 | Tools (9 turns), explicit offset calcs | Second best but couldn't beat 15 |
| 29 | — | 61 | NO | YES | no | — | 1,097 | $0.03 | Struct `visions` array | Compile fail |
| 30 | 70 | 61 | YES | no | no | — | 57,942 | $1.91 | Tools (6 turns), final analysis | Tied for third best |

---

## 3. Waste Analysis

Of 30 total attempts:

| Category | Count | Attempts |
|----------|-------|----------|
| **Compile failures** | 12 (40%) | 2, 3, 6, 7, 8, 13, 14, 16, 23, 26, 27, 29 |
| **Empty responses** | 2 (7%) | 10, 20 |
| **Soft timeout (forced early submit)** | 20 (67%) | 1–3, 5–8, 10, 12–14, 16, 18–20, 22–23, 25–27, 29 |
| **Stall-detected** | 2 | 6, 19 |
| **Duplicates** | 0 | — |
| **Regressions** (worse than previous best) | Multiple | 5, 9, 12, 17–19, 22, 25 |

**Net useful attempts** (compiled + not empty + actually used tools to iterate):
- Attempts 4, 9, 11, 15, 17, 21, 24, 28, 30 = **9 out of 30** (30%)
- Of these, only **attempts 11 and 15** actually improved the best score

**The real budget was 9 useful attempts, of which 2 moved the needle.** The other 21 attempts (70%) were wasted on compile failures, empty outputs, or soft-timeout-forced submissions that couldn't use tools.

### Soft timeout as the root destroyer

The soft timeout (1,000,000ms ≈ 16.7 min) was triggered on **20 of 30 attempts**. When it fires, Claude is forced to submit code immediately without using `compile_and_view_assembly`. The pattern is stark:

- **Attempts WITHOUT soft timeout** (10): average diff = **77.6** (of those that compiled), all used tools
- **Attempts WITH soft timeout** (20): average diff = **96.5** (of those that compiled), almost none used tools, 10 didn't compile

Soft timeout converts a tool-using iterative attempt into a single-shot "just output code" attempt, which is dramatically worse.

---

## 4. Pattern Analysis

### Progress trajectory

The trajectory oscillated without converging:

```
Attempt:  1   4   5   9  11  12  15  17  18  19  21  22  24  25  28  30
Diff:    95  89  96  95  70  88  61  78  82 130  74  96  73  80  64  70
         ↑       ↑       ↑↑      ↑↑↑     ↑↑↑      ↑       ↑   ↑↑
         base    reg    jump    BEST   worst    close      2nd  tie
```

**Three distinct phases:**
1. **Attempts 1–10**: Baseline struggle (89–96 range), mostly wasted on compile failures and soft timeouts
2. **Attempts 11–15**: Breakthrough phase — fresh start at 11 reached 70, then 15 reached the all-time best of 61
3. **Attempts 16–30**: Plateau/regression — never beat 61 again despite 15 more attempts

**The best attempt (15) came from**: a retry following attempt 14's compile failure, where Claude had access to the previous good code (attempt 11's structure) plus discovered the `gCurrentEntityCtx` macro from context.h, and added a `goto return1` pattern to place the `return 1` block mid-function (matching the target's layout).

### Why it plateaued at 61 differences

Comparing attempt 15's assembly to the target, the remaining 61 differences fall into two categories:

**Category A — Worlds 0–3 address computation (≈20 differences):**
Target does:
```asm
ldr r2, [r0, #0x0]      ; r2 = *gCurrentEntityCtx
mov r0, r9               ; r0 = world
lsl r1, r0, #0x3         ; r1 = world * 8
mov r0, r2               ; r0 = base
add r0, #0xf             ; r0 = base + 0x0F
add r0, r1               ; r0 = base + 0x0F + world*8
ldrb r1, [r0, #0x0]      ; load byte
```

Claude's code produces:
```asm
ldr r1, [r0, #0x0]      ; r1 = *gCurrentEntityCtx (wrong reg!)
mov r2, r9
lsl r0, r2, #0x3
add r2, r0, r1           ; combined add (different instruction)
ldrb r1, [r2, #0xf]     ; offset in ldrb (different encoding)
```

The target computes `base + 0x0F` then adds `world*8` with two separate `add` instructions, while GCC folds the constant into the `ldrb` offset when the C code uses array indexing like `data[0x0F + world * 8]`. To match the target, the C code likely needs something like:
```c
u8 *ptr = (u8 *)save;
ptr += 0x0F;
ptr += world * 8;
val = *ptr;
```

**Category B — Inner loop register swap (≈15 differences):**
Target has `j` in r2 and `byteOffset` in r3. Claude's code consistently produces the **reverse**: `j` in r3, `byteOffset` in r2. This is a pure GCC register allocator decision that's extremely hard to control from C code. The variable declaration order, loop structure, and expression order all influence it, but the relationship is non-obvious and compiler-version-specific.

**Category C — Initialization ordering (≈5 differences):**
Target initializes: `mov r1, #0x0; mov r8, r1; mov r12, r1; mov r6, #0x0; mov r10, r6; mov r0, #0x0; mov r4, r2; mov r7, #0x7f`
Claude's code produces a slightly different ordering (missing the `mov r0, #0x0` between r10 and r4).

### Failure patterns

1. **agbcc C89 requirement**: 12 compile failures (40%) were caused by Claude declaring variables mid-block (`u8 v30 = ...` inside a `{}`). agbcc requires C89-style declarations-before-statements. This is the single biggest waste factor.

2. **Soft timeout destroys tool usage**: When soft timeout fires, Claude outputs a raw code block without using `compile_and_view_assembly`. These attempts have ~0% chance of being better than tool-using attempts.

3. **Claude correctly identified the problem but couldn't fix it**: In attempt 28's response, Claude wrote: *"The remaining differences from the target are in register allocation (j and byteOffset are in swapped registers r2/r3 vs target's r2/r3) and some address computation differences. These are GCC register allocation decisions that are very difficult to control from C."* — This is an accurate diagnosis. Claude understood the problem is at the register allocator level.

4. **Stall detection didn't help**: Both stall-triggered attempts (6, 19) produced **worse** results. Attempt 19 regressed to 130 differences (the worst score), likely because the "try fundamentally different" instruction caused Claude to abandon the best-known approach.

5. **Context loss across conversation resets**: Fresh starts (attempts 11, 21) had to rediscover strategies. Attempt 11 happened to find a better approach; attempt 21 couldn't beat it.

---

## 5. Exact Assembly Patterns That Defeated Claude

### Pattern 1: Two-add address computation
**Target:**
```asm
mov r0, r2          ; r0 = base_ptr
add r0, #0xf        ; r0 = base_ptr + 15
add r0, r1          ; r0 = base_ptr + 15 + world*8
ldrb r1, [r0, #0x0] ; load from computed address
```
**Claude's code generates:**
```asm
add r2, r0, r1      ; r2 = world*8 + base_ptr (3-operand add)
ldrb r1, [r2, #0xf] ; offset baked into ldrb immediate
```

The target breaks the computation into `base + constant + variable` using two `add` instructions, while GCC folds the constant into the `ldrb` offset when the C code uses array indexing like `data[0x0F + world * 8]`. To match the target, the C code likely needs something like:
```c
u8 *ptr = (u8 *)save;
ptr += 0x0F;
ptr += world * 8;
val = *ptr;
```

### Pattern 2: Inner loop register allocation (r2/r3 swap)
**Target:** `j` → r2, `byteOffset` → r3
**All of Claude's attempts:** `j` → r3, `byteOffset` → r2

This is the **hardest** remaining issue. The permuter ran 560K+ iterations on this and also couldn't fix it (best score 1450, not 0). GCC's register allocator makes this decision based on:
- Variable declaration order
- Which variable appears first in the loop body
- Whether `j` or `byteOffset` is used in the increment expression first
- Possibly the expression `add r5, r0, #0x1` placement relative to the loop counter setup

### Pattern 3: Post-loop re-load pattern
**Target:**
```asm
ldr r0, [r4, #0x0]  ; r0 = *gCurrentEntityCtx
add r0, #0x8        ; r0 += 8
add r0, r3          ; r0 += byteOffset
ldrb r1, [r0, #0x0]
```
**Claude's code:**
```asm
ldr r0, [r4, #0x0]
add r0, r2, r0      ; r0 = byteOffset + base (3-operand)
ldrb r1, [r0, #0x8] ; 8 in ldrb immediate
```

Same root cause: GCC instruction selection for `base[0x08 + offset]` vs `*(base + 8 + offset)`.

---

## 6. Prompt and Context Evaluation

**Prompt**: Minimal — just "Decompile the function described in the system prompt." No examples, no hints about agbcc quirks, no similar functions.

**Context (8,655 chars)**: Contains type definitions, GBA register macros, and global variable macros. `gCurrentEntityCtx` is defined as `(*(u32 *)0x03004670)`. **No similar decompiled functions** are included as examples.

**Critical missing information:**
1. **No agbcc C89 requirement warning** — this caused 40% of attempts to fail on compilation. A single line "NOTE: agbcc requires C89 — all variable declarations must be at the top of their scope block" would have eliminated 12 wasted attempts.
2. **No matched function examples** — Claude had no reference for what agbcc-compatible decompiled code looks like in this project.
3. **No struct definition** for the save data — Claude had to guess the layout from raw offsets, and different guesses produced different code generation.

---

## 7. Pipeline Phase Contributions

**m2c**: Produced a decompilation that **failed to compile** (the programmatic phase reported `compiler: failure`). m2c's output used C99 features and raw casts. It was passed to Claude as context but never compiled successfully on its own.

**Permuter**: Ran 4 times, starting from different attempt codes:

| Run | Triggered By | Base Score | Best Score | Improvement | Iterations |
|-----|-------------|-----------|-----------|-------------|-----------|
| 1 | Attempt 1 | 3,335 | 1,450 | 56% better | 702,527 |
| 2 | Attempt 4 | 3,115 | 1,475 | 53% better | 560,854 |
| 3 | Attempt 11 | 18,700 | 1,525 | 92% better | 273,146 |
| 4 | Attempt 15 | 18,700 | 1,450 | 92% better | 211,314 |

The permuter achieved significant reductions (from 3000+ to ~1450) but plateaued there — same wall as Claude. The permuter's best code and Claude's best code converged to **essentially the same logical structure**, differing only in trivial variable naming. This confirms the remaining gap is in GCC register allocation, not logic.

The permuter's best results were **not fed back to Claude** during the run. This is a missed opportunity — the permuter found a score of 1,450 (roughly equivalent to ~50 objdiff differences) which is better than Claude's best of 61.

---

## 8. Token Consumption

| Metric | Value |
|--------|-------|
| Total output tokens | 451,239 |
| Total cache read tokens | 10,357,162 |
| Total cache creation tokens | 3,029,465 |
| **Total estimated cost** | **$15.52** (API reported) |
| Total wall time | 29,061 seconds ≈ **8.1 hours** |
| Average cost per attempt | $0.52 |
| Median cost per attempt | $0.04 |

The cost distribution is **bimodal**: soft-timeout attempts cost ~$0.03–0.04 each (forced single-shot), while tool-using attempts cost $1.00–2.00 each. The 9 tool-using attempts account for $13.52 (87%) of the total cost.

**Comparison to benchmark**: The benchmark averaged ~$1.70 per function for the 12-attempt SA3 runs. This 30-attempt run cost **9× more** ($15.52) for zero matches.

**Token growth over time**: Later attempts (21, 24, 28, 30) consumed 52K–59K output tokens each — significantly more than early tool-using attempts (25K–47K). This suggests growing conversation context made Claude work harder per turn without better results.

---

## 9. Ranked Proposals

### Rank 1: **Eliminate compile failures via C89 enforcement** (Expected: eliminate 40% waste)

**What it addresses:** 12 of 30 attempts (40%) failed because Claude emitted mid-block variable declarations incompatible with agbcc's C89 mode.

**How to implement:**
- Add to the system prompt: "CRITICAL: agbcc requires C89. ALL variable declarations MUST be at the top of their enclosing block. Never declare variables after a statement."
- Add a pre-compilation lint step that checks for mid-block declarations and returns a targeted error before attempting compilation
- This is a **one-line prompt change** with enormous impact

**Evidence:** Attempts 2, 3, 6, 7, 8, 13, 14, 16, 23, 26, 27, 29 all failed on this exact issue. If these 12 attempts had compiled, the function would have had 12 additional objdiff-evaluated submissions.

**Expected impact:** High. Would convert ~12 wasted attempts into actual submissions, roughly tripling the effective attempt budget.

### Rank 2: **Soft timeout recalibration or elimination** (Expected: double effective attempts)

**What it addresses:** Soft timeout fired on 20/30 attempts, forcing Claude to submit without using tools.

**How to implement:**
- Increase soft timeout to 25+ minutes (current: 16.7 min) for Sonnet-class models
- Or: make soft timeout dynamic — only trigger if Claude hasn't used any tools in the last N minutes (i.e., it's genuinely stalled, not working productively)
- Or: change the soft timeout message from "submit now" to "you have 3 minutes remaining, finish your current tool iteration and submit"

**Evidence:** Tool-using attempts averaged 77.6 differences; soft-timeout attempts averaged 96.5. The two best attempts (11, 15) were NOT soft-timeout-constrained.

**Expected impact:** High. Would roughly double the number of attempts where Claude can iterate with tools.

### Rank 3: **Include matched function examples in prompt** (Expected: better baselines)

**What it addresses:** Claude had zero examples of what correct agbcc-compiled decompiled code looks like.

**How to implement:**
- Include 1–2 already-matched functions from the same project as reference examples in the system prompt
- Prioritize functions that share structural features (global pointer dereference, nested loops, u8 truncation patterns)
- Use MizuchiDb's vector similarity to select structurally similar examples

**Evidence:** The context file contained only macros and type definitions. Claude had to independently discover patterns like the `goto return1` mid-function placement (only found in attempt 15), `(u8)(x + 1)` truncation, and `gCurrentEntityCtx` being a macro. Examples would front-load these learnings.

### Rank 4: **Add objdiff to MCP tools** (Expected: prevent false confidence)

**What it addresses:** Claude has no way to see the actual diff before submitting. It frequently claims "the code should match" when it doesn't.

**How to implement:** Add a `run_objdiff` MCP tool alongside `compile_and_view_assembly`.

**Evidence:** In attempt 28, Claude wrote "These are GCC register allocation decisions that are very difficult to control from C" — it **knew** it hadn't matched but submitted anyway because it had no precise diff. With objdiff, it could see exactly which instruction sequences differ and make targeted adjustments within the same turn.

### Rank 5: **Conversation reset with accumulated knowledge** (Expected: prevent regression)

**What it addresses:** After plateauing, continued attempts regress rather than improve. Fresh starts lose context.

**How to implement:** After N stalled attempts, reset the conversation but seed it with:
```
Previous best: 61 differences (attempt 15). Key findings:
- Use gCurrentEntityCtx macro, not raw 0x03004670 casts
- Use goto return1 pattern for mid-function return 1 block
- Inner loop: j and byteOffset in registers r2/r3 — target has j=r2, byteOffset=r3
- All variable declarations at top of scope (C89 required)
- Remaining mismatches: worlds 0-3 address computation pattern and inner loop register allocation
Best code: [insert attempt 15 code]
```

**Evidence:** Three fresh starts occurred (11, 21). Attempt 11 discovered the `gCurrentEntityCtx` pattern independently, but attempt 21 couldn't beat the existing best because it didn't receive the accumulated knowledge from attempts 11–15.

### Rank 6: **Smarter stall detection** (Expected: prevent negative impact)

**What it addresses:** Current stall detection triggers a generic "try fundamentally different" message that actively hurts: attempt 19 (stall-triggered) produced the **worst** score of 130 differences.

**How to implement:** Replace the stall message with:
- "Your last N attempts scored [X, Y, Z] differences. Your best was [B]. Focus on the specific mismatched assembly regions listed below rather than rewriting from scratch."
- Or: don't append a message at all — just trigger a conversation reset (Rank 5)

**Evidence:** Both stall-triggered attempts (6, 19) produced worse results. The generic "try fundamentally different" instruction caused Claude to abandon the best approach.

### Rank 7: **Feed permuter results back to Claude** (Expected: moderate, with caution)

**What it addresses:** The permuter found scores of 1,450 (better than Claude's best of ~61 objdiff differences) but these results were never shown to Claude.

**How to implement:** After permuter converges, provide its best code as a "reference implementation that scores better than your current attempt" and ask Claude to refine it.

**Caution (from the benchmark):** Permuter artifacts (no-op expressions, weird casts) can cause doom loops where Claude tries to "clean up" the code and makes it worse. Clearly label: "This code was found by random permutation — the logic may look unusual but the compiler output is closer to the target."

**Evidence:** The permuter's best code was structurally very similar to Claude's but had specific expression-level differences (e.g., `*((((u8 *) save) + 0x0F) + (world * 8))` instead of `data[0x0F + world * 8]`) that might produce different register allocation.

### Rank 8: **Early termination for register-allocation-bound functions** (Expected: cost savings)

**What it addresses:** Some functions are bottlenecked purely on GCC register allocation, which neither Claude nor the permuter can reliably control.

**How to implement:** After N attempts with a consistent "register allocation" signature in the diff (arguments differ but instructions match structurally), classify the function as "register-allocation-bound" and stop. This could be detected by checking if >70% of remaining differences are `ARGUMENT_MISMATCH` type.

**Evidence:** By attempt 11, the remaining differences were predominantly register swaps. The next 19 attempts (12–30) never solved this. Score was: 70 → best 61 → oscillating 64–96. The function should have been classified as "needs manual intervention or compiler flag experimentation" by attempt 15.

---

## 10. Answers to Final Questions

### If you could re-run this function with one change, what would it be?

**Add "agbcc requires C89 — no mid-block declarations" to the system prompt AND increase/remove the soft timeout.**

This single compound change would:
- Eliminate 12 compile failures (Rank 1)
- Allow all attempts to use tools (Rank 2)
- Convert from ~9 effective attempts to ~28 effective attempts
- Give Claude 3× the budget it actually had to work with

If forced to pick ONE: eliminate the compile failures. That's a prompt change requiring zero code.

### At what attempt should we have given up?

**Attempt 17.** Here's the reasoning:

- By attempt 15, the best score was 61 differences
- Attempts 16–17 both failed to improve (16 didn't compile, 17 got 78)
- The remaining differences were identified as register allocation issues by Claude itself
- The permuter (running since attempt 4) had converged to ~1,450 and also couldn't solve the register allocation
- The conditional probability of improvement after 3 stalled attempts past a local best is <10%

Stopping at attempt 17 would have saved attempts 18–30 (13 attempts × average $0.52 = ~$6.76, plus 4+ hours of wall time) with near-zero probability of matching.

**Optimal rule for hard functions**: Stop after 3 consecutive attempts that fail to improve on the best score, AND the best attempt's remaining differences are predominantly register-allocation mismatches. This would have triggered at attempt 17 (attempts 15→16→17 with no improvement) and saved 43% of the budget.
