# Mizuchi Improvements: Lessons from EntityDeathAnimation

## The Gap

Claude Runner got very close to matching `EntityDeathAnimation` (146/189 instructions matching in its best attempt), and the permuter closed the gap further (score 650). But the final match required 4 techniques that neither could apply:

| Technique | Why needed | Why Claude/permuter couldn't do it |
|-----------|-----------|-----------------------------------|
| `register int mask asm("r4") = 0xFF` | Forces `movs r4, #0xFF` scheduling | System prompt forbids `asm()` |
| `int val` + `int mask` types | Avoids extra `lsls` zero-extend after `ands` | Claude used `u8` (natural type); no feedback explained the codegen consequence |
| Block-scoped `register int ents asm("r3")` | Forces base address reload into r3 | System prompt forbids `asm()`; permuter can't emit register pins |
| `tmp + ents` operand order | Fixes `adds r4, r0, r3` vs `adds r4, r3, r0` | No feedback identified this as a commutative encoding issue |

## Root Causes

### 1. The `asm()` ban is too broad

The system prompt says:

> *"You should NEVER use the asm() construct."*

This was intended to prevent Claude from using inline assembly to implement logic (e.g., `asm("mov r0, #1")`), which would defeat the purpose of decompilation. But it also blocks two legitimate, widely-used GBA decomp techniques:

- **`register ... asm("rN")`** — a compiler hint for register allocation, not inline assembly. Used in pokeemerald, sa3, and every mature GBA decomp. This project already has 15 of them in matched functions.
- **`asm("" : "=r"(var) : "0"(val))`** — a compiler barrier for instruction scheduling. Used 42 times before the cleanup pass (now 0, all converted to direct init).

**Proposed fix**: Replace the blanket ban with a nuanced rule:

> *"Do NOT use `asm()` to write assembly instructions. However, you MAY use these compiler hint patterns when needed to fix register allocation or instruction scheduling:*
> - *`register type var asm("rN")` — pin a variable to a specific register*
> - *`asm("" : "=r"(var) : "0"(val))` — force the compiler to emit a load at this point*
> - *Block-scoped `{ register type var asm("rN") = val; ... }` — control both register and timing*
>
> *Use these only after confirming via `compile_and_view_assembly` that normal C produces wrong register allocation."*

### 2. No agbcc codegen knowledge in the prompt

Claude doesn't know how agbcc's register allocator and type system interact. The critical patterns:

**Type choice affects zero-extend emission:**
- `u8 & u8` → extra `lsls` after `ands` (agbcc zero-extends the u8 result)
- `int & int` → no extra instruction (result is already 32-bit)
- `u8 & int` → extra `ldrb` re-read from memory

Claude used `u8 mask = 0xFF` and `u8 timer` — the natural types from reading the assembly. It had no way to know that `int` types would eliminate the zero-extend instructions.

**`int base = 0x03002920` keeps a constant in a register:**
- Writing `((EntityDeathStruct *)0x03002920)[slot]` everywhere causes the compiler to reload the address from the literal pool each time.
- Writing `int base = 0x03002920; ((EntityDeathStruct *)base)[slot]` causes the compiler to load it once into a register and reuse it.
- But `gEntityArray` macro (`((u8 *)0x03002920)`) behaves like the first case due to the pointer cast.

Claude used `#define gEntityArray ((EntityDeathStruct *)0x03002920)` and accessed `gEntityArray[slot]` everywhere — a clean, readable approach that produces different (worse) register allocation than `int base`.

**Proposed fix**: Add an "agbcc codegen tips" section to the system prompt or include the project's learnings docs:

> *"agbcc quirks:*
> - *Use `int` not `u8` for mask variables used in `& mask` expressions to avoid extra zero-extends*
> - *Use `int base = 0x03002920` to keep an address in a register across the function*
> - *Block scope `{ }` controls register lifetime — variables declared in blocks are allocated later*
> - *In Thumb `adds Rd, Rn, Rm`, the left operand of `+` in C maps to Rn — use `offset + base` not `base + offset` to control encoding"*

### 3. The diff feedback doesn't explain WHY instructions differ

When Claude sees objdiff output like:

```
Difference 1 (INSERTION):
- Current: `(empty)` [insert]
- Target:  `ldr r7, [pc, #0x70]` [insert]

Difference 3 (DELETION):
- Current: `lsl r0, #0x18` [delete]
- Target:  `(empty)` [delete]
```

It knows WHAT is wrong but not WHY. The missing `ldr r7` means the address isn't being loaded at the right point. The extra `lsl` means there's an unwanted zero-extend. But Claude can't map these to the C-level fixes.

**Proposed fix**: Add a diff analysis layer that categorizes mismatches:

| Pattern | Likely cause | Suggested C fix |
|---------|-------------|----------------|
| Different register numbers, same instructions | Register allocation mismatch | Try `register ... asm("rN")` or change variable declaration order |
| Extra `lsls/lsrs` pair | Unnecessary zero-extend | Change variable type from `u8` to `int` |
| `ldr` at wrong position | Address scheduling | Use `int var = ADDR` to pin; or block scope |
| Missing `ldr` reload | Compiler reusing a register | Force reload with new block-scoped variable |
| Swapped operands in `adds` | Commutative encoding | Swap left/right operands of `+` in C |

This could be a post-processing step in the objdiff plugin or a separate analysis phase.

### 4. No permuter-to-Claude feedback loop

The permuter reached score 650 — it found the right logic structure but got stuck on register allocation. Both permuter runs converged to nearly identical code, suggesting the logic was correct. But there was no mechanism to feed the permuter's best code back to Claude with instructions like: "This code has the right logic. Fix the register allocation using `register asm` pins."

**Proposed fix**: Add a pipeline phase after the permuter that:
1. Takes the permuter's best code
2. Identifies remaining diffs (register assignment, zero-extends, instruction scheduling)
3. Feeds both the code and the categorized diffs to Claude with register-allocation-specific guidance
4. Allows Claude to apply `register asm` pins and type changes

### 5. Stall detection could trigger strategy changes

The run showed Claude's match percentage degrading over attempts (146→122→76→73→71 matching instructions). The stall detector eventually fires, but it just says "try a fundamentally different strategy." It could instead suggest specific technical approaches:

- "Your zero-extends don't match. Try using `int` instead of `u8` for intermediate values."
- "Register allocation is wrong. Try `int base = 0x03002920` and access through `((Struct *)base)[slot]`."
- "The second half reloads the base address. Try a block scope with a new variable."

## Prioritized Improvements

### High impact, low effort
1. **Relax the `asm()` ban** — Allow `register asm` pins and `asm("" : "=r" : "0")` barriers. This is a system prompt text change. Every mature GBA decomp uses these.
2. **Add agbcc codegen tips to system prompt** — Document `int` vs `u8` behavior, `int base` trick, block scoping. This is a prompt text change.
3. **Feed learnings docs to Claude** — The `docs/learnings/agbcc-asm-barriers.md` and `code-quality-improvements.md` files contain exactly the knowledge Claude needs. Include them as context.

### Medium impact, medium effort
4. **Diff categorization in objdiff** — Classify mismatches as register-allocation, zero-extend, scheduling, or operand-order issues. Suggest C-level fixes.
5. **Permuter-to-Claude handoff** — When permuter converges but doesn't reach score 0, feed its best code + categorized diffs to Claude as a new retry with register-allocation focus.

### Lower impact, higher effort
6. **Two-phase decompilation mode** — Phase 1: match logic (no `asm` allowed, focus on structure). Phase 2: match registers (enable `asm` pins, focus on encoding). This separates concerns and lets each phase use the right tools.
7. **agbcc codegen database** — Build a database of C patterns → assembly patterns (e.g., "`u8 & u8` produces extra `lsls`") and use it to automatically suggest fixes during retries.
