#ifndef GUARD_GLOBALS_H
#define GUARD_GLOBALS_H

#include "global.h"

/* ── Graphics Stream ── */

/* Pointer to the current position in the graphics/music data stream.
 * Double indirection: the u32 at this address holds a u8* into the stream.
 * Nearly all gfx stream command functions read/advance this pointer. */
#define gStreamPtr               (*(u8 **)0x03004D84)

/* Pointer to the graphics buffer control struct.
 * Dereferenced for palette state, flags, and buffer management. */
#define gGfxBufferPtr            (*(u32 *)0x030034A0)

/* Secondary stream output: palette/color value mirror. */
#define gStreamColorOut          (*(u16 *)0x03005420)

/* Stream color mirror (written alongside gStreamColorOut). */
#define gStreamColorMirror       (*(u16 *)0x030034AC)

/* Decompressed data buffer pointer (allocated by LoadAndDecompress functions). */
#define gDecompBuffer            (*(u32 *)0x030007D0)

/* Graphics buffer freed by ShutdownGfxStream. */
#define gGfxStreamBuffer         (*(u32 *)0x030007C8)

/* Buffer freed by FreeBuffer_52A4. */
#define gBuffer_52A4             (*(u32 *)0x030052A4)

/* ── Sound / Music (m4a / MusicPlayer2000 / Sappy engine) ── */

/* Main sound info struct pointer. Contains channel state, mixer config,
 * and pointers to the currently playing tracks. */
#define gSoundInfo               (*(u32 *)0x0300081C)

/* Music player context pointers. Each MusicPlayer instance has its own
 * context for independent track playback. */
#define gMPlayInfo_BGM           (*(u32 *)0x030064D8)
#define gMPlayInfo_SE            (*(u32 *)0x030064DC)

/* Sound command dispatch table pointer. */
#define gSoundTablePtr           (*(u32 *)0x03006450)

/* Sound command dispatch secondary table pointer. */
#define gSoundCmdTablePtr        (*(u32 *)0x03006454)

/* Sound engine state/event buffer. */
#define gSoundEventBuffer        ((u8 *)0x030054A0)

/* Sappy engine magic marker: "Smsh" (0x68736D53) in little-endian.
 * Used to verify the sound engine is properly initialized. */
#define SAPPY_MAGIC              0x68736D53

/* ── Game State ── */

/* Pause flag: when non-zero, GameUpdate skips the main update loop. */
#define gPauseFlag               (*(u8 *)0x030034E4)

/* Frame/tick counter — decremented each frame, byte-sized. */
#define gFrameCounter            (*(u8 *)0x03005498)

/* ── Entity / Object System ── */

/* Base of the main entity/sprite struct array (~36 bytes per element).
 * Most-referenced address in the entire codebase (1634 refs). */
#define gEntityArray             ((u8 *)0x03002920)

/* Global control/state block — flags and status at word/byte offsets. */
#define gControlBlock            ((u8 *)0x03004C20)

/* Game state struct array (byte-field access, ~200+ bytes per entry). */
#define gGameStateArray          ((u8 *)0x03005220)

/* Game state flags struct (byte fields at various offsets). */
#define gGameFlags               ((u8 *)0x03005400)

/* Configuration/output buffer struct (16-bit fields, dimensions/coords). */
#define gConfigBuffer            ((u8 *)0x03003430)

/* Pointer/handle array for buffers. */
#define gHandleArray             ((u8 *)0x03004790)

/* Entity/object pointer (double indirection). */
#define gEntityPtr               (*(u32 *)0x03004654)

/* OAM source data / lookup table (halfword-indexed). */
#define gOamSourceTable          ((u16 *)0x03004680)

/* Status byte lookup table. */
#define gStatusTable             ((u8 *)0x03000830)

/* Mixed-mode struct (byte + halfword fields). */
#define gMixedState              ((u8 *)0x03003510)

/* DMA buffer source address. */
#define gDmaBufferAddr           (*(u32 *)0x030007DC)

/* ── UI / Text Rendering ── */

/* Sprite attribute table for HUD/dialog rendering.
 * Array of 8-byte OAM-like entries at 0x03004800. */
#define gOamBuffer               ((u8 *)0x03004800)

/* Current sprite/OAM slot index for HUD. */
#define gSpriteSlotIndex         (*(u16 *)0x0300466C)

/* Sprite draw count / limit. */
#define gSpriteDrawCount         (*(u16 *)0x030051DC)

/* Display configuration flags (byte). */
#define gDisplayMode             ((u8 *)0x03000810)

/* Scroll/position state for text rendering. */
#define gTextScrollState         (*(u32 *)0x030034DC)

/* UI sub-state struct (for dialog, menus, transitions). */
#define gUIState                 ((u8 *)0x03004DA0)

/* Rendering scratch / output flags. */
#define gRenderFlags             (*(u32 *)0x03005428)

/* Secondary display state. */
#define gDisplayState2           ((u8 *)0x03003410)

/* Viewport/layer state. */
#define gViewportState           ((u8 *)0x03005284)

/* ── ROM Data Tables ── */

/* Sprite frame/animation data table.
 * Array of {u32 count, u32 dataPtr} pairs for sprite animations.
 * Referenced by RenderMenuUI, RenderDialogBox, RenderHUD*, etc. */
#define ROM_SPRITE_FRAME_TABLE   0x08078FC8

/* Display configuration / sprite mapping table.
 * Used by SetupDisplayConfig to select rendering modes. */
#define ROM_DISPLAY_CONFIG_TABLE 0x080D821C

/* OAM template data (initial OAM attribute values).
 * Used by InitOamEntries (FUN_0800a468). */
#define ROM_OAM_TEMPLATE         0x080E2A7C

/* Graphics asset / tileset tables used by InitGraphicsSystem. */
#define ROM_GFX_ASSET_TABLE      0x0818B7AC
#define ROM_TILESET_TABLE        0x0818B8E0

/* ── Sound ROM Data Tables ── */

/* Music track table: array of {u32 count, u32 trackDataPtr} entries.
 * Indexed by track ID in PlayMusicTrack. */
#define ROM_MUSIC_TABLE          0x08118AB4

/* Music track metadata table (offsets, lengths, loop points).
 * Paired with ROM_MUSIC_TABLE. */
#define ROM_MUSIC_META_TABLE     0x08118AE4

/* Sound command dispatch table.
 * Array of function pointers indexed by command byte. */
#define ROM_SOUND_CMD_TABLE      0x08117C8C

/* Instrument/voice table.
 * Contains waveform, envelope, and pitch data for each instrument. */
#define ROM_INSTRUMENT_TABLE     0x081179E4

/* Pitch/frequency lookup tables for MIDI note-to-frequency conversion. */
#define ROM_FREQ_TABLE_1         0x08117A74
#define ROM_FREQ_TABLE_2         0x08117B28
#define ROM_PITCH_TABLE          0x08117B70
#define ROM_WAVE_DUTY_TABLE      0x08117BF4
#define ROM_NOISE_TABLE          0x08117C0C
#define ROM_ENVELOPE_TABLE       0x08117C48
#define ROM_SWEEP_TABLE          0x08117C58

/* Sound configuration init data. */
#define ROM_SOUND_INIT_DATA      0x081177E4

#endif /* GUARD_GLOBALS_H */
