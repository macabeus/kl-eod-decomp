# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Matching decompilation of **Klonoa: Empire of Dreams** (GBA, USA). The goal is to produce C source that compiles to a byte-for-byte identical ROM using `agbcc` (GCC 2.95 fork for GBA). Currently 2 of 690 functions are decompiled.

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
- **Assembler/Linker**: `arm-none-eabi-as` / `arm-none-eabi-ld`
- **Disassembler**: Luvdis (tools/luvdis/) — generates asm/ from baserom.gba

## Architecture

Each source file in `src/` represents a module defined in `klonoa-eod-decomp.toml` with a start address. Non-decompiled functions use the `INCLUDE_ASM()` macro to inline generated assembly from `asm/nonmatchings/`. The link order in `ldscript.txt` is: rom_header → crt0 → system → math → engine → code_0 → code_1 → code_3 → gfx → m4a → syscalls → util → libgcc → data.

**Modules** (src/): `system.c` (14 funcs), `math.c` (4), `engine.c` (25), `code_0.c` (51), `code_1.c` (121), `code_3.c` (142), `gfx.c` (137), `m4a.c` (38), `syscalls.c` (2), `util.c` (5).

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

- **Always use a Python venv.** Never install packages globally. Activate before any `python3` or `pip` command:
  ```bash
  source venv/bin/activate
  ```
  If the venv doesn't exist yet, create it with `python3 -m venv venv` and install deps: `pip install -r requirements.txt`.

## Key Configuration Files

- **klonoa-eod-decomp.toml**: Module start addresses and function renames
- **functions_merged.cfg**: Function boundary addresses for Luvdis disassembly
- **ldscript.txt**: Linker script defining ROM memory layout at 0x8000000
- **mizuchi.yaml**: Config for Mizuchi AI-assisted decompilation tool
- **config.mk**: ROM metadata (title: KLONOA, game code: AKLE, maker: AF)
