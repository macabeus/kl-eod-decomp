#!/bin/bash
# test-ftst-ply-fine.sh — Demonstrate that -ftst enables C decompilation of ply_fine
#
# Usage: ./scripts/test-ftst-ply-fine.sh /path/to/patched/old_agbcc
#
# Requires: arm-none-eabi-cpp, arm-none-eabi-as, arm-none-eabi-objdump
#
# This script:
# 1. Compiles the same C code with and without -ftst
# 2. Assembles the pokeemerald target from source
# 3. Shows the byte-level diff
set -euo pipefail

AGBCC="${1:?Usage: $0 /path/to/patched/old_agbcc}"
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

echo "=== Step 1: Generate C source ==="

cat > "$TMPDIR/ply_fine.c" << 'CEOF'
typedef unsigned char u8;
typedef unsigned int u32;

struct SoundChannel {
    u8 statusFlags;       /* 0x00 */
    u8 _pad1[0x33];
    struct SoundChannel *nextChannelPointer;  /* 0x34 */
};

struct MusicPlayerTrack {
    u8 flags;             /* 0x00 */
    u8 _pad1[0x1F];
    struct SoundChannel *chan;  /* 0x20 */
};

void RealClearChain(void *x);

void ply_fine(u32 unused, struct MusicPlayerTrack *track)
{
    struct SoundChannel *chan;

    chan = track->chan;
    while (chan != 0)
    {
        u8 sf;
        sf = chan->statusFlags;
        if (sf & 0xC7)
        {
            chan->statusFlags = sf | 0x40;
        }
        RealClearChain(chan);
        chan = chan->nextChannelPointer;
    }
    track->flags = 0;
}
CEOF

echo "Source: $TMPDIR/ply_fine.c"
cat "$TMPDIR/ply_fine.c"

echo ""
echo "=== Step 2: Preprocess ==="
arm-none-eabi-cpp -nostdinc -P "$TMPDIR/ply_fine.c" -o "$TMPDIR/ply_fine.i"

echo ""
echo "=== Step 3: Compile WITHOUT -ftst ==="
"$AGBCC" "$TMPDIR/ply_fine.i" -o "$TMPDIR/no_tst.s" -mthumb-interwork -O2 -fhex-asm
sed '/\.size/d' "$TMPDIR/no_tst.s" > "$TMPDIR/no_tst_stripped.s"
printf ".text\n\t.align\t2, 0\n" >> "$TMPDIR/no_tst_stripped.s"
arm-none-eabi-as -mcpu=arm7tdmi -mthumb-interwork "$TMPDIR/no_tst_stripped.s" -o "$TMPDIR/no_tst.o"
arm-none-eabi-objcopy -O binary -j .text "$TMPDIR/no_tst.o" "$TMPDIR/no_tst.bin"

echo "Assembly (without -ftst):"
grep -A30 'ply_fine:' "$TMPDIR/no_tst.s" | head -30
echo ""
echo "Disassembly:"
arm-none-eabi-objdump -d "$TMPDIR/no_tst.o" | grep -A30 '<ply_fine>'

echo ""
echo "=== Step 4: Compile WITH -ftst ==="
"$AGBCC" "$TMPDIR/ply_fine.i" -o "$TMPDIR/tst.s" -mthumb-interwork -O2 -fhex-asm -ftst
sed '/\.size/d' "$TMPDIR/tst.s" > "$TMPDIR/tst_stripped.s"
printf ".text\n\t.align\t2, 0\n" >> "$TMPDIR/tst_stripped.s"
arm-none-eabi-as -mcpu=arm7tdmi -mthumb-interwork "$TMPDIR/tst_stripped.s" -o "$TMPDIR/tst.o"
arm-none-eabi-objcopy -O binary -j .text "$TMPDIR/tst.o" "$TMPDIR/tst.bin"

echo "Assembly (with -ftst):"
grep -A30 'ply_fine:' "$TMPDIR/tst.s" | head -30
echo ""
echo "Disassembly:"
arm-none-eabi-objdump -d "$TMPDIR/tst.o" | grep -A30 '<ply_fine>'

echo ""
echo "=== Step 5: Assemble reference target ==="
cat > "$TMPDIR/target.s" << 'TEOF'
.syntax unified
.thumb
.global ply_fine
.thumb_func
ply_fine:
	push {r4,r5,lr}
	adds r5, r1, #0
	ldr r4, [r5, #0x20]
	cmp r4, #0
	beq 1f
2:
	ldrb r1, [r4, #0x00]
	movs r0, #0xC7
	tst r0, r1
	beq 3f
	movs r0, #0x40
	orrs r1, r0
	strb r1, [r4, #0x00]
3:
	adds r0, r4, #0
	bl RealClearChain
	ldr r4, [r4, #0x34]
	cmp r4, #0
	bne 2b
1:
	movs r0, #0
	strb r0, [r5, #0x00]
	pop {r4,r5}
	pop {r0}
	bx r0
TEOF
arm-none-eabi-as -mcpu=arm7tdmi -mthumb-interwork "$TMPDIR/target.s" -o "$TMPDIR/target.o"
arm-none-eabi-objcopy -O binary -j .text "$TMPDIR/target.o" "$TMPDIR/target.bin"

echo ""
echo "=== Step 6: Binary comparison ==="

echo "Bytes WITHOUT -ftst:"
xxd "$TMPDIR/no_tst.bin" | head -5
echo ""
echo "Bytes WITH -ftst:"
xxd "$TMPDIR/tst.bin" | head -5
echo ""
echo "Bytes TARGET:"
xxd "$TMPDIR/target.bin" | head -5

# Compare only up to the target's length (ignore trailing alignment padding)
TARGET_SIZE=$(wc -c < "$TMPDIR/target.bin" | tr -d ' ')

# Truncate compiled binaries to target size for comparison
dd if="$TMPDIR/tst.bin" bs=1 count="$TARGET_SIZE" of="$TMPDIR/tst_trimmed.bin" 2>/dev/null
dd if="$TMPDIR/no_tst.bin" bs=1 count="$TARGET_SIZE" of="$TMPDIR/no_tst_trimmed.bin" 2>/dev/null

echo ""
if diff -q "$TMPDIR/tst_trimmed.bin" "$TMPDIR/target.bin" >/dev/null 2>&1; then
    echo "✅ WITH -ftst: BYTE-IDENTICAL to target ($TARGET_SIZE instruction bytes match)"
else
    echo "❌ WITH -ftst: does NOT match target"
    diff <(xxd "$TMPDIR/tst_trimmed.bin") <(xxd "$TMPDIR/target.bin") || true
fi

echo ""
if diff -q "$TMPDIR/no_tst_trimmed.bin" "$TMPDIR/target.bin" >/dev/null 2>&1; then
    echo "✅ WITHOUT -ftst: BYTE-IDENTICAL to target"
else
    echo "❌ WITHOUT -ftst: does NOT match target (expected)"
    echo "   Key difference: ands (0x4008) + cmp (0x2800) vs tst (0x4208)"
fi

echo ""
echo "=== Summary ==="
echo "The -ftst flag converts:"
echo "  and r0, r0, r1  (0x4008)  →  tst r0, r1  (0x4208)"
echo "  cmp r0, #0      (0x2800)  →  (removed — tst already sets flags)"
echo ""
echo "This saves 1 instruction (2 bytes) per flag check where the AND result is unused."
echo "Without -ftst, this function CANNOT be matched from C code."
