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

C_SRCS := $(wildcard $(C_SUBDIR)/*.c)
C_OBJS := $(patsubst $(C_SUBDIR)/%.c,$(C_BUILDDIR)/%.o,$(C_SRCS))

DATA_SRCS := $(wildcard $(DATA_SUBDIR)/*.s)
DATA_OBJS := $(patsubst $(DATA_SUBDIR)/%.s,$(DATA_BUILDDIR)/%.o,$(DATA_SRCS))

OBJS     := $(C_OBJS) $(ASM_OBJS) $(DATA_OBJS)
OBJS_REL := $(patsubst $(OBJ_DIR)/%,%,$(OBJS))

### FLAGS ###

ASFLAGS  := -mcpu=arm7tdmi -mthumb-interwork
CPPFLAGS := -nostdinc -I tools/agbcc/include -iquote include
CC1FLAGS := -mthumb-interwork -Wimplicit -Wparentheses -O2 -fhex-asm -fprologue-bugfix

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
