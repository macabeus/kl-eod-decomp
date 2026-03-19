/* CLOSE: r3/r5 swap in callee-saved register allocation. Needs permuter. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
void InitOamEntries(void) {
    u16 *src = (u16 *)0x03004680;
    u32 *tmpl = (u32 *)0x080E2A7C;
    u32 tmpl_lo = tmpl[0];
    u32 tmpl_hi = tmpl[1];
    u32 *dst = (u32 *)0x03004800;
    s32 i;
    for (i = 127; i >= 0; i--) {
        dst[0] = tmpl_lo;
        dst[1] = tmpl_hi;
        *(u16 *)((u8 *)dst + 6) = *src;
        src++;
        dst += 2;
    }
}
