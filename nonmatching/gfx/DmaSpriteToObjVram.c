/* CLOSE: r0/r2 swap in register allocation. Needs permuter. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef volatile unsigned int vu32;
struct SpriteEntry { u32 data; u16 oamIndex; u16 frameSize; };
void DmaSpriteToObjVram(s32 spriteIdx, s32 frame) {
    vu32 *dma = (vu32 *)0x040000D4;
    struct SpriteEntry *table = *(struct SpriteEntry **)0x030007C8;
    struct SpriteEntry *entry = table + spriteIdx;
    u16 frameSize = entry->frameSize;
    u32 src = entry->data + (u32)(frameSize * 32) * frame;
    dma[0] = src;
    dma[1] = 0x06010000 + (u32)(entry->oamIndex * 32);
    dma[2] = 0x80000000 | (u32)(entry->frameSize * 16);
    dma[2];
}
