# Improving Code Quality in Matched Decompiled Functions

## Context

After a function is matched (compiles to byte-identical assembly), the C source often contains mechanical artifacts from the matching process: opaque variable names, inline assembly hacks, and unnecessary scope blocks. This document describes safe improvements that preserve the match.

## Safe Improvements (always preserve match)

### 1. Rename opaque variables

Register-derived names like `a0`, `a1`, `r0`, `r1`, `v1` can be renamed freely — variable names have no effect on compiled output.

```c
// Before:
u32 a0 = 0x0300081C;
u32 *infoRef;
asm("" : "=r"(infoRef) : "0"(a0));

// After:
u32 soundInfoAddr = 0x0300081C;
u32 *soundInfoRef;
asm("" : "=r"(soundInfoRef) : "0"(soundInfoAddr));
```

**Naming conventions used in this project**:
- `xxxAddr` for raw address constants (e.g., `soundInfoAddr`, `fadeCounterAddr`)
- Descriptive names for pointers (e.g., `soundInfoRef`, `fadeCounter`, `callbackState`)
- Context-specific names (e.g., `fadeTimer`, `channelIdx`, `songEntry`)

### 2. Replace magic number addresses with globals.h defines

When `globals.h` already defines a name for an address, use it:

```c
// Before:
u32 addr = 0x03005498;
u8 *fadeCounter;
asm("" : "=r"(fadeCounter) : "0"(addr));
*fadeCounter += 1;

// After (if barrier can be removed):
gFrameCounter += 1;

// After (if barrier must stay, still improve readability):
// Use the define name in comments or variable names
```

### 3. Replace `#define` addresses with extern linker symbols

For ROM data table addresses, define them as extern symbols. This often eliminates `asm` barriers entirely:

```c
// Before (globals.h):
#define ROM_MUSIC_TABLE 0x08118AB4

// After (globals.h):
extern const struct MusicPlayer gMPlayTable[];
#define ROM_MUSIC_TABLE 0x08118AB4  // keep for reference

// ldscript.txt:
gMPlayTable = 0x08118AB4;
```

See `agbcc-asm-barriers.md` for the full technique.

### 4. Add or improve docstrings

Every decompiled function must have a `/** docstring */`. Write docstrings that explain what the function does, not how it does it.

### 5. Fix parameter types by comparing with other decomps

The initial decompilation often infers `u32` for all parameters. Checking pokeemerald/sa3 reveals the correct narrower types, which can eliminate shift-folding barriers:

```c
// Before (u32 causes <<16 >>13 shift pattern needing asm barrier):
void m4aSongNumStart(u32 idx) { ... }

// After (u16 is the correct type — shifts generated naturally):
void m4aSongNumStart(u16 idx) { ... }
```

Cross-reference with: pokeemerald `include/m4a.h`, sa3 `include/lib/m4a/m4a.h`.

### 6. Use volatile stores for ordering

When a store must happen before a subsequent load, use `vu8`/`vu16`/`vu32` cast on the store:

```c
// Before (asm barrier forces store-before-load ordering):
u32 pauseFlagAddr = 0x030034E4;
u8 *pauseFlag;
asm("" : "=r"(pauseFlag) : "0"(pauseFlagAddr));
*pauseFlag = 1;

// After (volatile store is a compiler barrier):
*(vu8 *)0x030034E4 = 1;
```

## Risky Improvements (test with `make compare` after each change)

### 7. Remove `asm("")` barriers

See `agbcc-asm-barriers.md` for the decision tree. Always:
1. Make the change
2. Run `make compare`
3. If it fails, use `arm-none-eabi-objdump -d build/src/FILE.o` to compare the generated assembly against the target in `asm/matchings/`
4. Revert if it doesn't match

### 8. Flatten orphan `{ }` blocks

Move variable declarations to function top and remove the block. This changes register lifetimes and often breaks the match.

```c
// Before:
void func(void) {
    // code A
    {
        u32 x = compute();
        use(x);
    }
}

// After (if it matches):
void func(void) {
    u32 x;
    // code A
    x = compute();
    use(x);
}
```

#### Flattening blocks with `register asm` + direct init

Blocks containing `register type var asm("rN") = val` exist to control both register assignment AND initialization timing. The block entry point is what schedules the `movs rN, #val` instruction.

To flatten these, **separate declaration from assignment**:

```c
// Before (block controls timing):
entity->timer -= 1;
{
    register int mask asm("r4") = 0xFF;
    if ((u8)entity->timer == 0xFF) {
        /* ... uses mask ... */
    }
}

// After (declaration hoisted, assignment at original point):
register int mask asm("r4");
/* ... other declarations ... */

entity->timer -= 1;
mask = 0xFF;
if ((u8)entity->timer == 0xFF) {
    /* ... uses mask ... */
}
```

This works because `mask = 0xFF` (a plain assignment to a register-pinned variable) emits `movs r4, #0xFF` at the assignment point, just like the block-scoped init did. The declaration at function top allocates r4 for `mask` but doesn't emit any instruction.

**When this fails**: When the compiler reorders the plain assignment. The block-scoped direct init is a stronger scheduling guarantee than a plain assignment. If `make compare` fails after flattening, the block is load-bearing — keep it.

**Contrast with `register asm = val` at function scope**: Declaring `register int mask asm("r4") = 0xFF` at function scope emits the load at function entry (too early for mid-function timing). Separating into declaration (function top) + assignment (mid-function) gives the right timing without the block.

### 9. Remove `register asm("rN")` pins

These are almost never removable. Test anyway — sometimes agbcc naturally picks the same register.

## Using objdiff for Diagnosis

When a change breaks the match, compare disassembly:

```bash
# Disassemble a function from the compiled object:
arm-none-eabi-objdump -d -j .text build/src/gfx.o | sed -n '/<FunctionName>:/,/^$/p'

# Compare with the target assembly:
cat asm/matchings/gfx/FunctionName.s

# Find exact byte differences:
cmp -l klonoa-eod-baseline.gba klonoa-eod.gba | head -20
```

Common differences and their causes:
- **Swapped `ldr` order**: Address load ordering issue → need `asm` barrier or orphan block
- **Different register numbers**: Register allocation changed → need `register asm("rN")` or scope block
- **Missing/extra instruction**: Shift folding or constant propagation → need `asm("" : "+r")` barrier
- **Same instructions, different literal pool offsets**: The function body is identical but the pool layout differs → usually caused by the position of `ldr rN, [pc, #X]` changing

## Workflow

1. Pick a function with `asm` barriers or orphan blocks
2. Read the target assembly in `asm/matchings/MODULE/FUNCTION.s`
3. Understand what each `asm` barrier is preventing (see categories above)
4. Check reference projects (pokeemerald, sa3) for the same function — compare parameter types, struct usage, and extern declarations
5. Try the most aggressive removal first (often: correct parameter type + extern symbols eliminates everything)
6. If it fails, use objdiff to understand the gap — compare instruction-by-instruction
7. Try progressively less aggressive approaches: volatile store, declaration reorder, partial barrier removal
8. If nothing works, the barrier is load-bearing — keep it with improved variable names
9. Run `make format` before committing

## Effectiveness Ranking (from most to least impactful)

1. **Extern linker symbols** — eliminated 11 barriers across 6 functions
2. **Parameter type narrowing** (`u32` → `u16`/`u8`) — eliminated 4 shift barriers
3. **Volatile stores** (`*(vu8 *)addr = val`) — eliminated 4 ordering barriers
4. **Direct pointer initialization** — eliminated 3 barriers
5. **Using existing `#define` globals** — eliminated 3 barriers
6. **Flattening orphan blocks** — removed 13+ blocks (no barrier removal, but cleaner code)
7. **Separating `register asm` declaration from assignment** — flattens blocks that use `register asm("rN") = val` for timing control; hoist declaration to function top, keep assignment at original point
8. **Declaration order changes** — critical for register assignment when using externs
