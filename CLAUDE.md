# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Matching decompilation of **Klonoa: Empire of Dreams** (GBA, USA). The goal is to produce C source that compiles to a byte-for-byte identical ROM using `agbcc` (GCC 2.95 fork for GBA).

## Build Commands

```bash
make                # Build ROM and verify SHA1 match (same as `make compare`)
make tidy           # Clean build artifacts
make format         # Auto-format C/H files with clang-format
make check_format   # Check formatting without modifying (used in CI)
make ctx            # Generate ctx.c for decomp.me/m2c/mizuchi
```

Build requires `arm-none-eabi` toolchain, Python 3.13+, and a legally obtained `baserom.gba` placed at the project root. First-time setup: run `./setup.sh` after cloning with `--recurse-submodules`.

## Toolchain

- **Compiler**: `agbcc` (tools/agbcc/) — GCC 2.95 targeting ARM7TDMI, Thumb interwork, `-O2`
  - **`-ftst` flag**: Patched to emit `tst` instruction for bitwise flag tests. Required for 7 core MP2000 functions. m4a.o is compiled with this flag.
  - **Per-compilation-unit TST**: The original m4a module was compiled from multiple source files — some with TST, some without. Functions from the TST compilation unit must use `-ftst`; others must not.
- **Assembler/Linker**: `arm-none-eabi-as` / `arm-none-eabi-ld`
- **Disassembler**: Luvdis (tools/luvdis/) — generates asm/ from baserom.gba

## Architecture

Each source file in `src/` represents a module defined in `klonoa-eod-decomp.toml` with a start address. Non-decompiled functions use the `INCLUDE_ASM()` macro to inline generated assembly from `asm/nonmatchings/`. The link order in `ldscript.txt` is: rom_header → crt0 → system → math → engine → code_0 → code_1 → code_3 → gfx → m4a → syscalls → util → libgcc → data.

**Modules** (src/): `system.c` (10 funcs), `math.c` (7 funcs), `engine.c` (27 funcs), `code_0.c` (23 funcs), `code_1.c` (101 funcs), `code_3.c` (88 funcs), `gfx.c` (113 funcs), `m4a.c` (93 funcs), `syscalls.c` (5 funcs), `util.c` (9 funcs).

## Decompilation Workflow

1. Pick an `INCLUDE_ASM("asm/nonmatchings/...", FUN_XXXXXXXX)` call in a `src/*.c` file
2. Replace it with equivalent C code that compiles to matching assembly
3. Add a rename entry in `klonoa-eod-decomp.toml` under `[renames]`: `FUN_XXXXXXXX = "MeaningfulName"`
4. Run `make compare` to verify byte-exact match
5. Rerun `python3 scripts/generate_asm.py` to update assembly labels

## Code Style

- 4-space indentation, 120-char line limit, K&R braces
- Right-aligned pointers (`u8 *ptr`)
- Types from `include/global.h`: `u8/u16/u32`, `s8/s16/s32`, volatile variants (`vu8`, etc.)
- `TRUE`/`FALSE`/`NULL` defined as 1/0/0
- Include order must never be reordered (affects matching)
- Format with `make format` before committing

## Policies

- **Always use the Python venv.** Never install packages globally. Activate before any `python3` or `pip` command:
  ```bash
  source .venv/bin/activate
  ```
  If `.venv` doesn't exist yet, run `./setup.sh` which creates it and installs dependencies.

- **Run `make format` before every commit** that touches C or header files. CI enforces `make check_format`.

- **One commit per matched function.** Each successfully decompiled function gets its own commit with a descriptive message explaining the matching technique used.

- **Every decompiled function must have a semantic name and docstring.** No `FUN_XXXXXXXX` names in committed C code. Add a rename in `klonoa-eod-decomp.toml` and a `/** docstring */` above the function.

- **Check GitHub issues before decompiling.** Reference issues in commits/PRs. Post findings (techniques, blockers) on relevant issues when a match succeeds or fails.

- **PRs use feature branches.** Create a branch from `main`, push, open PR against upstream. Never push work directly to `main`. Delete branches after merge.

- **Update the website when learning about architecture.** When decompilation reveals how a subsystem works, update the gh-pages documentation (graphics-engine.html, game-engine.html, sound.html, matching.html, etc.).

- **"decomp more" means:** look for related functions near already-matched ones, assign semantic symbols, write docstrings, try matching with known techniques.

- **"more symbols" means:** name ALL addressable things — functions, sub-function entry points, IWRAM globals, ROM data tables, struct fields, constants.

- **Keep the terminal title updated.** Use `printf '\033]0;DESCRIPTION\007'` to show the current task in the gnome-terminal title bar.

- **All policies must be public.** Every strict policy must be added to this CLAUDE.md file so that all collaborators can see and follow them. Never store policies only in Claude's memory.

## Key Configuration Files

- **klonoa-eod-decomp.toml**: Module start addresses and function renames
- **functions_merged.cfg**: Function boundary addresses for Luvdis disassembly
- **ldscript.txt**: Linker script defining ROM memory layout at 0x8000000
- **mizuchi.yaml**: Config for Mizuchi AI-assisted decompilation tool
- **config.mk**: ROM metadata (title: KLONOA, game code: AKLE, maker: AF)
