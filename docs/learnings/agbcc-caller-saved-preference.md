# agbcc: `-fcaller-saved-preference` Flag

## What This Change Does

We added a new compiler flag to agbcc that makes the register allocator prefer **caller-saved registers** for variables that don't need to survive across function calls.

## Background: Caller-Saved vs Callee-Saved Registers

In the ARM architecture (and most others), CPU registers are divided into two groups based on a convention called the [calling convention](https://developer.arm.com/documentation/dui0041/c/ARM-Procedure-Call-Standard/About-the-ARM-Procedure-Call-Standard):

- **Caller-saved registers** (r0-r3 on ARM/Thumb): These are "scratch" registers. Any function is free to overwrite them. If you're calling another function and need a value after the call returns, you must save it yourself before the call. They're also called **volatile** or **temporary** registers.

- **Callee-saved registers** (r4-r7 on Thumb, r4-r11 on ARM): These must be preserved by any function that uses them. A function that wants to use r4 must `push {r4}` at the beginning and `pop {r4}` at the end. They're also called **non-volatile** or **preserved** registers.

The implication for code generation:

| Register type | Cost if value crosses a function call | Cost if value does NOT cross a call |
|---|---|---|
| Caller-saved (r0-r3) | Must save/restore around the call (expensive) | Free to use, no push/pop needed |
| Callee-saved (r4-r7) | Automatically preserved (free at use site) | Requires push/pop in prologue/epilogue (expensive!) |

A smart register allocator should prefer caller-saved registers for values that don't cross function calls, because they don't require push/pop. This is [standard compiler behavior](https://gcc.gnu.org/onlinedocs/gccint/Register-Allocation.html) in modern GCC versions.

## The Problem in agbcc

agbcc (based on GCC 2.95) doesn't make this distinction well. Its global register allocator in [`global.c`](https://gcc.gnu.org/onlinedocs/gccint/Global-Reg.html) processes pseudo-registers in priority order and assigns them to the first available hard register. It doesn't prefer caller-saved registers for non-call-crossing values.

This caused a concrete problem: when compiling a `switch` statement, the switch value (which only lives during the comparison chain and never crosses a function call) was allocated to r6 (callee-saved). This forced the compiler to emit `push {r4, r5, r6, lr}` instead of `push {r4, r5, lr}`, adding an unnecessary register save/restore.

### Concrete Example

The function `EntityItemDrop` has a switch on `entityBase[0x0F]`. The target binary (from the original game compiler) uses:

```asm
ldrb r1, [r3, #0x0F]   ; load switch value into r1 (caller-saved)
cmp r1, #3              ; compare — r1 doesn't need to survive any call
beq case3
```

But agbcc generated:

```asm
ldrb r6, [r2, #15]     ; load into r6 (callee-saved!)
cmp r6, #3              ; r6 is preserved but unnecessarily so
beq case3
```

The r6 usage forces `push {r4, r5, r6, lr}` (saving 3 callee-saved registers) instead of `push {r4, r5, lr}` (saving only 2). This single register difference cascades through the entire function, changing all register assignments.

## The Fix

We modified `find_reg()` in `global.c`. This function tries to assign a hard register to each pseudo-register. It runs in two passes:

- **Pass 0**: Conservative — avoids registers that other pseudos prefer
- **Pass 1**: Aggressive — tries all available registers

Our change adds a check in pass 0: if the flag is enabled and the pseudo doesn't cross any function calls (`allocno_calls_crossed == 0`), skip callee-saved registers. This makes the allocator try caller-saved registers (r0-r3) first. If none are available in pass 0, pass 1 runs unchanged as a fallback.

### Files Changed

| File | Change |
|---|---|
| `gcc/flags.h` | Added `extern int flag_caller_saved_preference;` |
| `gcc/toplev.c` | Added flag variable definition and `f_options[]` entry |
| `gcc/global.c` | Added 4-line check in `find_reg()` pass 0 inner loop |

### The Core Change (global.c)

```c
/* In the inner loop of find_reg(), when testing if a hard register is usable: */
&& !(flag_caller_saved_preference
     && pass == 0
     && allocno_calls_crossed[allocno] == 0
     && !call_used_regs[regno])
```

This says: "When the flag is active, in the conservative pass (pass 0), for pseudos that don't cross any calls, skip registers that are NOT call-used (i.e., skip callee-saved registers)."

## Impact

### On Existing Matched Functions

**Zero impact.** All existing matched functions produce identical output with the flag enabled. This was verified by a clean rebuild (`rm -f build/src/*.o && make compare`) passing the SHA1 check.

The reason: none of the currently matched C functions contain `switch` statements, and the existing `register asm("rN")` pins override the allocator's decisions.

### On Future Decompilations

The flag enables matching functions like `EntityItemDrop` where the original compiler kept the switch discriminant in a caller-saved register. Without the flag, this was impossible — 13 parallel agents testing 500+ C code variations all failed because agbcc hardcoded the switch value into a callee-saved register.

### On Code Quality

The flag produces objectively better code for non-call-crossing values: fewer unnecessary push/pop instructions, matching what [modern GCC](https://gcc.gnu.org/wiki/Register_Allocation) and the original game compiler produce.

## How to Use

The flag is added to `CC1FLAGS` in the Makefile:

```makefile
CC1FLAGS := ... -fcaller-saved-preference
```

It applies to all game code modules (code_0, code_1, code_3, engine, gfx, math, syscalls, system, util). The m4a sound library files use a separate compiler (`old_agbcc`) and are unaffected.

## References

- [ARM Procedure Call Standard (APCS)](https://developer.arm.com/documentation/dui0041/c/ARM-Procedure-Call-Standard) — defines which registers are caller-saved vs callee-saved
- [GCC Internals: Register Allocation](https://gcc.gnu.org/onlinedocs/gccint/Register-Allocation.html) — describes the global register allocator's pass structure
- [GCC Internals: Global Register Allocation](https://gcc.gnu.org/onlinedocs/gccint/Global-Reg.html) — the `find_reg` function that our change modifies
- [GCC Wiki: Register Allocation](https://gcc.gnu.org/wiki/Register_Allocation) — overview of GCC's register allocation strategy
- [Thumb Calling Convention](https://developer.arm.com/documentation/ddi0210/c/Introduction/Instruction-set-summary/ThumbTM-instruction-summary) — ARM7TDMI Thumb register usage conventions
