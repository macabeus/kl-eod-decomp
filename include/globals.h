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

/* BG layer configuration struct array (3 entries, 0x1C=28 bytes each).
 * Each entry controls one hardware BG layer:
 *   +0x00: u32 tileVramDest   - VRAM charblock address for tile DMA
 *   +0x04: u32 tmapVramDest   - VRAM screenbase address for tilemap DMA
 *   +0x08: u16 scrollX        - horizontal scroll position (subpixel)
 *   +0x0A: u16 scrollY        - vertical scroll position (subpixel)
 *   +0x10: u16 mapWidth       - tilemap width in tiles
 *   +0x12: u16 mapHeight      - tilemap height in tiles
 *   +0x14: u16 flags
 *   +0x16: u16 dmaTileCount   - number of tile rows for DMA
 *   +0x18: u8  dmaRowSize     - bytes per DMA row
 * Initialized by InitLevelBG with charblock assignments:
 *   Entry 0: tileVram=0x06000000, tmapVram=0x0600E000
 *   Entry 1: tileVram=0x06004000, tmapVram=0x0600E800
 *   Entry 2: tileVram=0x06008000, tmapVram=0x0600F000 */
#define gBGLayerState            ((u8 *)0x03003430)

/* Decompress/DMA buffer control struct.
 * Holds pointers to allocated decompression buffers during scene setup.
 *   +0x00: buf0  (tile data A / tilemap layer 0)
 *   +0x04: buf1  (tile data B / tilemap layer 1)
 *   +0x08: buf2  (tile data C)
 *   +0x0C: buf3  (tile data D)
 *   +0x14: buf5  (tilemap layer 2) */
#define gDecompBufferCtrl        ((u8 *)0x03004790)

/* Entity/object pointer (double indirection). */
#define gEntityPtr               (*(u32 *)0x03004654)

/* OAM source data / lookup table (halfword-indexed). */
#define gOamSourceTable          ((u16 *)0x03004680)

/* Status byte lookup table. */
#define gStatusTable             ((u8 *)0x03000830)

/* Mixed-mode struct (byte + halfword fields). */
#define gMixedState              ((u8 *)0x03003510)

/* VRAM write cursor: current palette RAM destination for DMA transfers.
 * Advanced by 0x20 after each 32-byte palette DMA during scene setup. */
#define gVramWriteCursor         (*(u32 *)0x030007DC)

/* Palette VRAM write cursor: tracks current VRAM destination during
 * sequential tile/palette DMA transfers in scene setup. */
#define gPaletteVramCursor       (*(u32 *)0x03005490)

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

/* ── Background Layer Tables ── */

/* BG tile data pointer table: 162 u32 ROM pointers to compressed tile charblock data.
 * Indexed as: (vision-1)*27 + world*3 + layer, where vision=1-6, world=0-8, layer=0-2.
 * Each entry points to a compressed asset with a 4-byte sub-header (BGCNT template)
 * followed by 4bpp tile character data. */
#define ROM_BG_TILE_TABLE        0x08189034

/* BG tilemap pointer table: 162 u32 ROM pointers to compressed screenblock data.
 * Same indexing as ROM_BG_TILE_TABLE.
 * Each entry points to compressed tilemap data (u16 per cell:
 * bits 0-9=tileID, bit10=hflip, bit11=vflip, bits12-15=palBank). */
#define ROM_BG_TILEMAP_TABLE     0x081892BC

/* BG palette pointer table: 54 u32 ROM pointers to compressed palette data.
 * Indexed as: (vision-1)*9 + world.
 * Each entry points to 512 bytes of GBA RGB555 palette (16 banks x 16 colors). */
#define ROM_BG_PALETTE_TABLE     0x08188F5C

/* BG tile/tilemap configuration lookup table.
 * Used by LoadBGTileData and LoadBGTilemapData to map (sceneIdx, layerIdx)
 * to an entry index into the BG layer struct and ROM data tables. */
#define ROM_BG_LOOKUP_TABLE      0x08057ACC

/* BG tile ROM pointer sub-table.
 * Indexed by entry from ROM_BG_LOOKUP_TABLE to select compressed tile data. */
#define ROM_BG_TILE_SUBTABLE     0x08189BCC

/* BG tilemap ROM pointer sub-table.
 * Used by LoadBGTilemapData for per-entry tilemap data selection. */
#define ROM_BG_TILEMAP_SUBTABLE  0x08189CCC

/* BG dimension lookup tables: map (vision, world) to tilemap width/height.
 * Each is a 54-entry table of u16 values (6 visions x 9 worlds). */
#define ROM_BG_WIDTH_TABLE       0x08051C76
#define ROM_BG_HEIGHT_TABLE      0x08051DBA
#define ROM_BG_TILECOUNT_TABLE   0x08051EFE
#define ROM_BG_STRIDE_TABLE      0x08052042

/* ── Scene-Specific Shared Tilesets ── */

/* These compressed tilesets are loaded into VRAM charblocks during scene init
 * (SetupSceneGfx / FUN_0804886c). They provide shared tiles (HUD, items, etc.)
 * that are referenced by per-level BG tilemaps via absolute tile IDs. */
#define ROM_SCENE_TILESET_A      0x08366214  /* -> charblocks 0-3 via palettePtr */
#define ROM_SCENE_TILESET_B      0x08367468  /* -> small OBJ tiles */
#define ROM_SCENE_TILES_CB0      0x082F4D3C  /* -> VRAM 0x06000000 (charblock 0) */
#define ROM_SCENE_TILES_CB1      0x082F518C  /* -> VRAM 0x06004000 (charblock 1) */
#define ROM_SCENE_TILES_CB2      0x082F5D0C  /* -> VRAM 0x06008000 (charblock 2) */
#define ROM_SCENE_TILES_CB3      0x082F7D64  /* -> VRAM 0x0600C000 (charblock 3) */
#define ROM_SCENE_TILEMAP_DATA   0x082F5920  /* -> IWRAM tilemap buffers */

/* Scene-specific palette data loaded during SetupSceneGfx. */
#define ROM_SCENE_PALETTE_A      0x08078F88
#define ROM_SCENE_PALETTE_B      0x08078FA8

/* OBJ (sprite) tileset for scene overlay. */
#define ROM_SCENE_OBJ_TILES      0x082F4934

/* Sprite layout table for HUD/scene overlay objects.
 * 12-byte entries terminated by 0xFFFF. */
#define ROM_SCENE_SPRITE_TABLE   0x08116590

/* World map BG tile data. */
#define ROM_WORLDMAP_TILES       0x082EA584
#define ROM_WORLDMAP_TILEMAP     0x082EA730
#define ROM_WORLDMAP_PALETTE     0x082EA7F0

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
