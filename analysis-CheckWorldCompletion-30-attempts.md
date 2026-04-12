# Analysis: CheckWorldCompletion — 30 Failed Attempts

Report file: `partial-run-results-2026-03-17T02-34-13.json`

## Summary

30 attempts, $9.67 total cost, 0 matches. Best result: 61% match (attempt 8). The model plateaus around 59-60% when using the MCP tool and oscillates between 33-54% without it.

## Key Finding: MCP Tool Usage Correlates Strongly with Quality

| Category | Attempts | Avg Match % | Range |
|---|---|---|---|
| Used MCP tool | 6 (attempts 2,5,8,10,13,28) | **59.0%** | 58-60% |
| No MCP tool (soft timeout hit) | 13 | **42.6%** | 33-54% |
| Compiler failure / no code | 11 | N/A | N/A |

The MCP tool (`mcp__mizuchi__compile_and_view_assembly`) was used in 6 of 30 attempts. These attempts ran 200-1100 seconds and consistently achieved ~59% match. The remaining 13 compilable attempts hit the soft timeout and performed significantly worse.

**However, the tool-using attempts plateau at 59% and never improve beyond that.** All 6 converge to essentially the same code — the same 78 differences, the same 115 matching instructions.

## Soft Timeout Behavior: Not Broken, But Misreported

### What appeared broken

20 of 30 attempts triggered `softTimeoutTriggered: true` with `durationMs` of only 14-18 seconds, despite `softTimeoutMs` being configured at 1,000,000ms (1000s). This initially appeared as if the soft timeout was firing prematurely.

### What actually happens

The soft timeout fires correctly after 1000s. The `durationMs` in the report was misleading due to **a bug in timing accumulation for aborted queries** (now fixed in Mizuchi).

When the soft timeout fires and aborts the initial query, the SDK's `result` message (which carries `duration_ms` / `duration_api_ms`) is never emitted. The report's `durationMs` only captured the ~15s recovery query, silently dropping the ~1000s initial query.

Evidence from token usage confirms the initial queries ran for their full budget:

| Category | Avg output tokens | Avg cost | Reported `durationMs` | Actual wall time |
|---|---|---|---|---|
| Non-timeout (tool use) | 42,570 | $1.48 | 649s | ~649s |
| Timeout (recovery only) | 1,230 | $0.04 | 15s | **~1015-1273s** |

The actual wall time per timeout attempt is: TTFT (5-258s) + softTimeoutMs (1000s) + recovery (~15s).

### The fix

A bug fix was applied to Mizuchi's `#collectResponse` method: when a query is aborted without a `result` message, elapsed wall time (`Date.now() - #queryStartTime`) is now added to `#queryTiming.durationMs` and `durationApiMs`. This ensures reports reflect the actual time spent, not just the recovery query's API time.

### Impact on the analysis

The soft timeout is NOT preventing tool usage — the model gets a full 1000s budget to iterate. The 20 timeout attempts ran for ~17 minutes each before the recovery prompt fired. The low quality of timeout attempts (42.6% vs 59%) is because the recovery prompt says "do not use tools, just output the code" — the model submits a guess from memory without a final `compile_and_view_assembly` check.

## The 59% Ceiling: Root Cause Analysis

The 96 differences in the best attempt break down as:

| Category | Count | Description |
|---|---|---|
| ARGUMENT_MISMATCH | 58 | Same opcode, wrong registers |
| INSERTION | 16 | Target has instructions the compiled code doesn't |
| DELETION | 14 | Compiled code has extra instructions |
| REPLACEMENT | 7 | Different instruction entirely |
| OPCODE_MISMATCH | 1 | `bgt` vs `ble` (inverted condition) |

### Problem 1: Memory access idiom — `ptr[base + offset]` vs `(ptr + base) + offset`

This is the **single largest source of differences** (causes ~30 of the 96 diffs). It repeats in the inner loop 3 times per iteration.

**Target assembly pattern (original compiler):**
```asm
ldr r0, [r4, #0x0]    ; r0 = *saveDataPtr
add r0, #0x8           ; r0 = saveData + 8
add r0, r3             ; r0 = saveData + 8 + slot_offset
ldrb r1, [r0, #0x0]   ; r1 = *(saveData + 8 + slot_offset)
```

**AI-generated code compiles to:**
```asm
ldr r1, [r4, #0x0]    ; r1 = *saveDataPtr
add r0, r2, r1        ; r0 = slot_offset + saveData
ldrb r0, [r0, #0x8]   ; r0 = *(saveData + slot_offset + 8)
```

Same semantics, different instruction sequence. The original code does `(base + 8) + offset` while agbcc generates `(base + offset) + 8` from `saveData[0x08 + r3]`.

**Fix for the prompt:** The AI needs to write this as:
```c
u8 *ptr = saveData + 8;   // separate the base+8 computation
ptr += slot_offset;
val = *ptr;
```

### Problem 2: Inner loop counter types cause extra truncation

Target uses raw `add r3, #0x1; add r2, #0x1` (no truncation). AI code uses `u8` counters which generates `lsl/lsr` byte truncation after every increment — 6 extra instructions per iteration across 35 iterations = ~12 extra instructions.

**Fix for the prompt:** Inner loop counters `r2` (slot) and `r3` (offset) should be `u32` or `int`, not `u8`.

### Problem 3: The `world <= 3` branch has different pointer decomposition

Target computes the save data offset as:
```asm
mov r0, r9           ; r0 = world
lsl r1, r0, #0x3    ; r1 = world * 8
mov r0, r2           ; r0 = saveData
add r0, #0xf        ; r0 = saveData + 0xF
add r0, r1           ; r0 = saveData + 0xF + world*8
```

AI code generates `saveData[0x0F + world * 8]` which agbcc compiles differently — it computes the array index first, then adds it to the base.

### Problem 4: The `bgt` vs `ble` inversion

Attempt 0 generates `bgt .L15e188` (branch if greater than) but target has `ble .L146171` (branch if less or equal). This is from writing `if (x > 0x18)` instead of `if (x <= 0x18)` with swapped branch targets. An inverted condition that agbcc handles differently.

## What the Retry Prompt Provides

- **After compilation failure:** The model receives only its previous code. No assembly diff — just "syntax error at line X".
- **After objdiff failure:** The model receives the full assembly diff (both sides + 96 differences). This is 26KB of text. Only attempt 10 received this full context and it still plateaued at 58%.

**Problem:** The 96-difference list is too much information. The model can't extract the actionable pattern (pointer arithmetic idiom) from a wall of register-level diffs.

## Recommendations

### 1. Add agbcc pointer arithmetic guidance to the prompt
The most impactful single change. Add something like:

```markdown
## Critical: agbcc pointer arithmetic

When the target assembly shows:
    add r0, #0x8
    add r0, r3
    ldrb r1, [r0, #0x0]

Write the C code as separate pointer additions, NOT as a single array index:
    u8 *ptr = base + 8;
    ptr += offset;
    val = *ptr;

Do NOT write: val = base[8 + offset]  — this produces different codegen.
```

### 2. Summarize the diff instead of dumping 96 differences
Instead of showing all 96 raw differences, summarize the patterns:
- "Pointer arithmetic pattern differs in 30 places — you're generating `ldrb rX, [rY, #0x8]` but the target has `add rX, #0x8; ldrb rY, [rX, #0x0]`"
- "Inner loop uses byte truncation (lsl/lsr) — use u32 counters instead of u8"

### 3. Use `u32` loop counters in examples
The examples in the prompt use `u8` for loop counters. Since the target assembly increments without byte truncation, the inner loop variables should be `u32`.

### 4. Add a structurally similar decompiled example
The current examples are trivial (5-10 lines). Add a matched function that has nested loops, multi-register save/restore, and pointer arithmetic to teach the model the agbcc idioms.

### 5. Consider providing the `matchPercent` in retry feedback
The `matchPercent` field is always `None` in the report despite `matchingCount`/`differenceCount` being populated. Fix this so the model knows its progress quantitatively (e.g., "Your code matches 59% of instructions").
