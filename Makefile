include config.mk

MAKEFLAGS += --no-print-directory

.SUFFIXES:
.SECONDARY:
.DELETE_ON_ERROR:
.SECONDEXPANSION:

ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

ifeq ($(OS),Windows_NT)
EXE := .exe
else
EXE :=
endif

SHELL  := /bin/bash -o pipefail
SHA1   := $(shell { command -v sha1sum || command -v shasum; } 2>/dev/null) -c

### TOOLCHAIN ###

PREFIX  := arm-none-eabi-
CC1     := tools/agbcc/bin/agbcc$(EXE)
CC1_OLD := tools/agbcc/bin/old_agbcc$(EXE)
CPP     := $(PREFIX)cpp
LD      := $(PREFIX)ld
OBJCOPY := $(PREFIX)objcopy
AS      := $(PREFIX)as

### FILES ###

OBJ_DIR := build
ROM     := $(BUILD_NAME).gba
ELF     := $(BUILD_NAME).elf
MAP     := $(BUILD_NAME).map

ASM_SUBDIR   := asm
ASM_BUILDDIR := $(OBJ_DIR)/$(ASM_SUBDIR)

C_SUBDIR   := src
C_BUILDDIR := $(OBJ_DIR)/$(C_SUBDIR)

DATA_SUBDIR   := data
DATA_BUILDDIR := $(OBJ_DIR)/$(DATA_SUBDIR)

ASM_SRCS := $(wildcard $(ASM_SUBDIR)/*.s)
ASM_OBJS := $(patsubst $(ASM_SUBDIR)/%.s,$(ASM_BUILDDIR)/%.o,$(ASM_SRCS))

C_SRCS := $(filter-out $(C_SUBDIR)/m4a_1.c $(wildcard $(C_SUBDIR)/m4a_nopush_*.c) $(wildcard $(C_SUBDIR)/m4a_tst_*.c),$(wildcard $(C_SUBDIR)/*.c))
C_OBJS := $(patsubst $(C_SUBDIR)/%.c,$(C_BUILDDIR)/%.o,$(C_SRCS))

DATA_SRCS := $(wildcard $(DATA_SUBDIR)/*.s)
DATA_OBJS := $(patsubst $(DATA_SUBDIR)/%.s,$(DATA_BUILDDIR)/%.o,$(DATA_SRCS))

OBJS     := $(C_OBJS) $(ASM_OBJS) $(DATA_OBJS)
OBJS_REL := $(patsubst $(OBJ_DIR)/%,%,$(OBJS))

### FLAGS ###

ASFLAGS  := -mcpu=arm7tdmi -mthumb-interwork
CPPFLAGS := -nostdinc -I tools/agbcc/include -iquote include
CC1FLAGS := -mthumb-interwork -Wimplicit -Wparentheses -O2 -fhex-asm -fprologue-bugfix

# TST compilation unit: m4a_1.c is pre-compiled with old_agbcc -ftst into
# build/m4a_1_funcs.s, then included as assembly in m4a.c via asm(".include ...").
# This keeps all m4a functions in one .text section (required by shared literal
# pools in the ROM assembly). Uses old_agbcc (not agbcc) to match m4a's register
# allocation behavior.
TST_CC1FLAGS := -mthumb-interwork -O2 -ftst

DECOMP_TOML := klonoa-eod-decomp.toml
LDSCRIPT    := ldscript.txt
LDSCRIPT_IN := ldscript.in.txt

LIBS := tools/agbcc/lib/libgcc.a tools/agbcc/lib/libc.a

### FORMATTER ###

FORMAT := clang-format
FORMAT_SRCS := $(shell find src include -name "*.c" -o -name "*.h")

### CONTEXT ###

C_HEADERS := $(shell find include -name "*.h" -not -name "include_asm.h")
CONTEXT_FLAGS := -DM2C -DPLATFORM_GBA=1 -Dsize_t=int

### TARGETS ###

.PHONY: all rom compare clean tidy format check_format ctx

$(shell mkdir -p $(ASM_BUILDDIR) $(C_BUILDDIR) $(DATA_BUILDDIR))

all: compare

compare: rom
	$(SHA1) $(BUILD_NAME).sha1

rom: $(ROM)

$(ELF): $(OBJS) $(LDSCRIPT)
	@echo "$(LD) -T $(LDSCRIPT) -Map $(MAP) <objects> -o $@"
	@cd $(OBJ_DIR) && $(LD) -T ../$(LDSCRIPT) -Map ../$(MAP) $(OBJS_REL) -o ../$(ELF)

$(ROM): $(ELF)
	$(OBJCOPY) -O binary $< $@

# Generate ldscript.txt by prepending symbol aliases from the TOML [renames] section
$(LDSCRIPT): $(LDSCRIPT_IN) $(DECOMP_TOML)
	@python3 scripts/generate_ldscript.py $(DECOMP_TOML) $(LDSCRIPT_IN) $(LDSCRIPT)

### RECIPES ###

# Assemble standalone .s files (crt0, libgcc, rom_header)
$(ASM_BUILDDIR)/%.o: $(ASM_SUBDIR)/%.s
	@echo "$(AS) <flags> -o $@ $<"
	@$(AS) $(ASFLAGS) -o $@ $<

# Pre-compile TST functions (m4a_1.c → build/m4a_1_funcs.s)
$(OBJ_DIR)/m4a_1_funcs.s: $(C_SUBDIR)/m4a_1.c
	@echo "$(CC1) <flags> -ftst -o $@ $<"
	@$(CPP) $(CPPFLAGS) $< -o $(OBJ_DIR)/m4a_1.i
	@$(CC1_OLD) $(TST_CC1FLAGS) -o $(OBJ_DIR)/m4a_1_raw.s $(OBJ_DIR)/m4a_1.i
	@sed '/^@/d;/^\.code/d;/^\.gcc2_compiled/d;/^\.text$$/d;/^\.Lfe/d;/^[[:space:]]*\.size/d;/macros\.inc/d;s/\.L\([0-9]\)/.Lm4a1_\1/g' $(OBJ_DIR)/m4a_1_raw.s > $@

# Pre-compile nopush functions (leaf functions that need -fprologue-bugfix)
# Each produces a .s snippet .include'd at the correct position in m4a.c.
NOPUSH_SRCS := $(wildcard $(C_SUBDIR)/m4a_nopush_*.c)
NOPUSH_ASM  := $(patsubst $(C_SUBDIR)/m4a_nopush_%.c,$(OBJ_DIR)/m4a_nopush_%.s,$(NOPUSH_SRCS))

$(OBJ_DIR)/m4a_nopush_%.s: $(C_SUBDIR)/m4a_nopush_%.c
	@echo "$(CC1_OLD) <nopush flags> -o $@ $<"
	@$(CPP) $(CPPFLAGS) $< -o $(OBJ_DIR)/m4a_nopush_$*.i
	@$(CC1_OLD) -mthumb-interwork -O2 -fprologue-bugfix -o $(OBJ_DIR)/m4a_nopush_$*_raw.s $(OBJ_DIR)/m4a_nopush_$*.i
	@sed '/^@/d;/^\.code/d;/^\.gcc2_compiled/d;/^\.text$$/d;/^\.Lfe/d;/^[[:space:]]*\.size/d;/macros\.inc/d;s/\.L\([0-9]\)/.Lnp$*_\1/g' $(OBJ_DIR)/m4a_nopush_$*_raw.s > $@

# Pre-compile per-function TST units (functions needing -ftst individually)
TST_FUNC_SRCS := $(wildcard $(C_SUBDIR)/m4a_tst_*.c)
TST_FUNC_ASM  := $(patsubst $(C_SUBDIR)/m4a_tst_%.c,$(OBJ_DIR)/m4a_tst_%.s,$(TST_FUNC_SRCS))

$(OBJ_DIR)/m4a_tst_%.s: $(C_SUBDIR)/m4a_tst_%.c
	@echo "$(CC1_OLD) <flags> -ftst -o $@ $<"
	@$(CPP) $(CPPFLAGS) $< -o $(OBJ_DIR)/m4a_tst_$*.i
	@$(CC1_OLD) $(TST_CC1FLAGS) -o $(OBJ_DIR)/m4a_tst_$*_raw.s $(OBJ_DIR)/m4a_tst_$*.i
	@sed '/^@/d;/^\.code/d;/^\.gcc2_compiled/d;/^\.text$$/d;/^\.Lfe/d;/^[[:space:]]*\.size/d;/macros\.inc/d;s/\.L\([0-9]\)/.Lm4atst$*_\1/g' $(OBJ_DIR)/m4a_tst_$*_raw.s > $@

# Compile m4a with old_agbcc — Nintendo's MusicPlayer2000 was prebuilt
# with an older GCC as part of the GBA SDK.
$(C_BUILDDIR)/m4a.o: $(C_SUBDIR)/m4a.c $(OBJ_DIR)/m4a_1_funcs.s $(NOPUSH_ASM) $(TST_FUNC_ASM)
	@echo "$(CC1_OLD) <m4a flags> -o $@ $<"
	@$(CPP) $(CPPFLAGS) $< -o $(C_BUILDDIR)/m4a.i
	@$(CC1_OLD) -mthumb-interwork -O2 -o $(C_BUILDDIR)/m4a.s $(C_BUILDDIR)/m4a.i
	@printf ".text\n\t.align\t2, 0\n" >> $(C_BUILDDIR)/m4a.s
	@$(AS) $(ASFLAGS) -o $@ $(C_BUILDDIR)/m4a.s

# Compile C files (with INCLUDE_ASM support)
$(C_BUILDDIR)/%.o: $(C_SUBDIR)/%.c
	@echo "$(CC1) <flags> -o $@ $<"
	@$(CPP) $(CPPFLAGS) $< -o $(C_BUILDDIR)/$*.i
	@$(CC1) $(CC1FLAGS) -o $(C_BUILDDIR)/$*.s $(C_BUILDDIR)/$*.i
	@printf ".text\n\t.align\t2, 0\n" >> $(C_BUILDDIR)/$*.s
	@$(AS) $(ASFLAGS) -o $@ $(C_BUILDDIR)/$*.s

# Assemble data files
$(DATA_BUILDDIR)/%.o: $(DATA_SUBDIR)/%.s
	@echo "$(AS) <flags> -o $@ $<"
	@$(AS) $(ASFLAGS) -o $@ $<

ctx.c: $(C_HEADERS)
	@for header in $(C_HEADERS); do echo "#include \"$$header\""; done > ctx.h
	@gcc -P -E -dD -undef -I tools/agbcc/include -I include $(CONTEXT_FLAGS) ctx.h \
		| sed '/#undef/d' \
		| sed '/typedef unsigned long int int;/d' \
		| sed 's/__attribute__((.*))//' \
		| sed '/^#define __STDC/d' \
		| sed '/^#define __GCC/d' \
		| sed '/^#define GUARD_/d' \
		> ctx.c
	@rm ctx.h
	@echo "Generated ctx.c ($$(wc -l < ctx.c) lines)"

ctx: ctx.c

format:
	$(FORMAT) -i -style=file $(FORMAT_SRCS)

check_format:
	$(FORMAT) -style=file --dry-run --Werror $(FORMAT_SRCS)

clean: tidy

tidy:
	$(RM) -r build
	$(RM) $(BUILD_NAME).gba $(BUILD_NAME).elf $(BUILD_NAME).map
