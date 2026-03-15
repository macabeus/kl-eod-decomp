# Klonoa: Empire of Dreams Decompilation

<img src="./docs/media/logo.png" align="right" height="130px" />

> 🌙 Turning the dreams into C code, one function at a time.

A work-in-progress matching decompilation of **Klonoa: Empire of Dreams** (GBA, 2001).

## What is Matching Decompilation?

Matching decompilation is the art of converting assembly back into C source code that, when compiled, produces byte-for-byte identical machine code. It’s popular in the retro gaming community for recreating the source code of classic games. For example, [Super Mario 64](https://github.com/n64decomp/sm64) and [The Legend of Zelda: Ocarina of Time](https://github.com/zeldaret/oot) have been fully match-decompiled.

> [Learn more by watching my talk.](https://www.youtube.com/watch?v=sF_Yk0udbZw)

## Setup

### Prerequisites

- [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (`arm-none-eabi-as`, `arm-none-eabi-ld`, `arm-none-eabi-objcopy`)
- GNU Make
- A legally obtained copy of the USA ROM

### Building

1. Clone this repository including the submodules:

```
git clone --recurse-submodules git@github.com:macabeus/kl-eod-decomp.git
```

2. Place your ROM as `baserom.gba` in the project root.

3. Run the setup script:

```bash
./setup.sh
```

4. Build and verify:

```bash
make
```

A successful build prints `klonoa-eod.gba: OK`, confirming a byte-identical match against the original ROM.

### Rebuilding

After setup, just run `make`. Use `make tidy` to clean build artifacts.

### Mizuchi

It's optional but highly recommended to install [Mizuchi](https://github.com/macabeus/mizuchi) and use it to browse the functions and decompile them automatically.

### Useful Commands

| Command | Description |
|---------|-------------|
| `make` | Build and verify matching |
| `make tidy` | Clean build artifacts |
| `make format` | Auto-format C/H files |
| `make check_format` | Check formatting (used by CI) |
| `make ctx` | Generate `ctx.c` — the context file used by [decomp.me](https://decomp.me), [m2c](https://github.com/matt-kempster/m2c), and [mizuchi](https://github.com/macabeus/mizuchi) for decompiling individual functions |

## Project Structure

```
├── asm/
│   ├── crt0.s                  ARM init code (hand-written, in git)
│   ├── macros.inc              Assembly macros (in git)
│   └── nonmatchings/           Individual function .s files (generated)
│   |   ├── system/
│   |   ├── engine/
│   |   ├── gfx/
│   |   └── ...
│   └── matchings/              Individual function .s files (generated)
│       ├── system/
│       ├── engine/
│       ├── gfx/
│       └── ...
├── src/                        Decompiled C source (grows over time)
├── include/                    C headers
├── data/                       Game data
├── constants/                  GBA and game constants
├── tools/
│   ├── agbcc/                  agbcc compiler (git submodule)
│   └── luvdis/                 GBA disassembler (git submodule)
├── scripts/
│   └── generate_asm.py         Generates asm/ from baserom.gba
├── klonoa-eod-decomp.toml      Module definitions and function renames
├── functions_merged.cfg        Function address list for disassembly
├── Makefile                    Build system
├── ldscript.txt                Linker script
└── klonoa-eod.sha1             SHA1 checksum for verification
```

The `asm/nonmatchings/` and `asm/matchings/` directories are **not checked into git** — they are generated from the ROM by `scripts/generate_asm.py` (called automatically by `setup.sh`).

## Decompiling a Function

Each `src/*.c` file contains `INCLUDE_ASM(...)` macros that inline assembly for functions not yet decompiled. To decompile a function:

1. **Find the function** in the corresponding `src/*.c` file (e.g., `src/gfx.c`).

2. **Replace** its `INCLUDE_ASM(...)` line with C code:

    ```c
    // Before:
    INCLUDE_ASM("asm/nonmatchings/gfx", FUN_0804b254);

    // After:
    u16 ReadUnalignedU16(u8 *ptr)
    {
        return ptr[0] | (ptr[1] << 8);
    }
    ```

3. **Rename the function** — add an entry to `klonoa-eod-decomp.toml` under `[renames]`:

    ```toml
    [renames]
    FUN_0804b254 = "ReadUnalignedU16"
    ```

    This ensures the assembly label is updated everywhere when `generate_asm.py` runs. The key is the original Ghidra/Luvdis name (`FUN_XXXXXXXX`) and the value is the new descriptive name.

4. **Verify** the build still matches:

    ```bash
    make compare
    ```

    A successful match prints `klonoa-eod.gba: OK`.

## Discord

Talk with us in the Discord servers linked below:

- [decomp.me Discord](https://discord.gg/sutqNShRRs)
- [Klonoa Mega Chat](https://discord.com/invite/klonoa-mega-chat-103975433493581824) at the channel `#klonoa-modifications`

## CI

The GitHub Actions workflow (`.github/workflows/build.yml`) builds the project and verifies the ROM match on every push and pull request.

The ROM is **not** stored in the repository. It is fetched at CI time from a URL stored as a GitHub secret and verified against the expected SHA1 checksum.

To set up CI, add this [repository secret](https://docs.github.com/en/actions/security-for-github-actions/security-guides/using-secrets-in-github-actions):

| Secret        | Description                             |
|---------------|-----------------------------------------|
| `STORAGE_URL` | URL to a `.zip` file containing the ROM |

## Status

- **Compiler**: agbcc (GCC 2.95) — confirmed via function epilogue analysis
- **Build**: Matching (`klonoa-eod.gba: OK`)
- **Functions**: 463 total, 2 decompiled to C
