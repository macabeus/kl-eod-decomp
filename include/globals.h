#ifndef GUARD_GLOBALS_H
#define GUARD_GLOBALS_H

#include "global.h"

/* ══════════════════════════════════════════════════════════════════════
 *  Struct Definitions
 * ══════════════════════════════════════════════════════════════════════ */

/* BGLayerState: per-layer BG configuration (28 bytes, 3 entries at gBGLayerState).
 * Controls VRAM destinations, scroll positions, tilemap dimensions, and DMA params. */
struct BGLayerState {
    u32 tileVramDest; /* +0x00: VRAM charblock address for tile DMA */
    u32 tmapVramDest; /* +0x04: VRAM screenbase address for tilemap DMA */
    u16 scrollX; /* +0x08: horizontal scroll position (subpixel, >>4 for pixels) */
    u16 scrollY; /* +0x0A: vertical scroll position (subpixel, >>4 for pixels) */
    u16 unk_0C; /* +0x0C: unknown */
    u16 unk_0E; /* +0x0E: unknown */
    u16 mapWidth; /* +0x10: tilemap width in tiles */
    u16 mapHeight; /* +0x12: tilemap height in tiles */
    u16 flags; /* +0x14: layer flags */
    u16 dmaTileCount; /* +0x16: number of tile rows for DMA */
    u8 dmaRowSize; /* +0x18: bytes per DMA row */
    u8 pad_19; /* +0x19: padding */
    u16 pad_1A; /* +0x1A: padding */
}; /* total: 0x1C = 28 bytes */

/* GfxStreamEntry: per-entry state for the graphics command stream processor
 * (36 bytes, 32 entries at gBuffer_52A4).
 * Controls animations, motion paths, and timed events driven by the stream. */
struct GfxStreamEntry {
    u8 status; /* +0x00: entry status (low 3 bits = type, bit 3+ = flags) */
    u8 unk_01; /* +0x01: sub-flags (bit 7 = direction, bits 6:3 = speed) */
    u8 unk_02; /* +0x02: secondary flags */
    u8 unk_03; /* +0x03: bit 7 = sign flag (set from timer sign) */
    u16 unk_04; /* +0x04: tile/frame base index */
    u16 unk_06; /* +0x06: tile count / allocation size */
    u16 unk_08; /* +0x08: counter / position A */
    u16 unk_0A; /* +0x0A: counter / position B */
    u16 unk_0C; /* +0x0C: frame index */
    u16 unk_0E; /* +0x0E: unknown */
    u16 unk_10; /* +0x10: unknown */
    u16 unk_12; /* +0x12: unknown */
    u16 unk_14; /* +0x14: timer value (s16, sign sets bit 7 of byte 3) */
    u16 unk_16; /* +0x16: unknown */
    u16 unk_18; /* +0x18: unknown */
    u16 unk_1A; /* +0x1A: unknown */
    u16 unk_1C; /* +0x1C: unknown */
    u8 unk_1E; /* +0x1E: frame/animation state */
    u8 unk_1F; /* +0x1F: frame/animation param */
    u32 callback; /* +0x20: function pointer for per-frame update */
}; /* total: 0x24 = 36 bytes */

/* ── Graphics Stream ── */

/* Pointer to the current position in the graphics/music data stream.
 * Double indirection: the u32 at this address holds a u8* into the stream.
 * Nearly all gfx stream command functions read/advance this pointer. */
#define gStreamPtr            (*(u8 **)0x03004D84)

/* Pointer to the graphics buffer control struct.
 * Dereferenced for palette state, flags, and buffer management. */
#define gGfxBufferPtr         (*(u32 *)0x030034A0)

/* Secondary stream output: palette/color value mirror. */
#define gStreamColorOut       (*(u16 *)0x03005420)

/* Stream color mirror (written alongside gStreamColorOut). */
#define gStreamColorMirror    (*(u16 *)0x030034AC)

/* Decompressed data buffer pointer (allocated by LoadAndDecompress functions). */
#define gDecompBuffer         (*(u32 *)0x030007D0)

/* Graphics buffer freed by ShutdownGfxStream. */
#define gGfxStreamBuffer      (*(u32 *)0x030007C8)

/* Buffer freed by FreeBuffer_52A4. */
#define gBuffer_52A4          (*(u32 *)0x030052A4)

/* BLDY fade level counter. Incremented/decremented during transitions
 * and written to REG_BLDY (0x04000054) by the VBlank handler. */
#define gBldyFadeLevel        (*(u8 *)0x030007D8)

/* ── Sound / Music (m4a / MusicPlayer2000 / Sappy engine) ── */

/* Main sound info struct pointer. Contains channel state, mixer config,
 * and pointers to the currently playing tracks. */
#define gSoundInfo            (*(u32 *)0x0300081C)

/* Music player context pointers. Each MusicPlayer instance has its own
 * context for independent track playback. */
#define gMPlayInfo_BGM        (*(u32 *)0x030064D8)
#define gMPlayInfo_SE         (*(u32 *)0x030064DC)

/* Sound command dispatch table pointer. */
#define gSoundTablePtr        (*(u32 *)0x03006450)

/* Sound command dispatch secondary table pointer. */
#define gSoundCmdTablePtr     (*(u32 *)0x03006454)

/* Sound engine state/event buffer. */
#define gSoundEventBuffer     ((u8 *)0x030054A0)

/* Sappy engine magic marker: "Smsh" (0x68736D53) in little-endian.
 * Used to verify the sound engine is properly initialized. */
#define SAPPY_MAGIC           0x68736D53

/* ── EEPROM / Timer Transfer ── */

/* Index of the hardware timer (0-3) used for EEPROM transfers.
 * Set by InitEepromTimer, read by EepromBeginTransfer/EepromEndTransfer. */
#define gEepromTimerIdx       (*(vu8 *)0x03000378)

/* Pointer to the active timer control register (REG_TM0CNT + idx*4).
 * Updated by InitEepromTimer; used for direct timer register writes. */
#define gEepromTimerRegPtr    (*(vu32 *)0x03000380)

/* Saved IME state, stored before disabling interrupts for EEPROM I/O. */
#define gEepromSavedIME       (*(u16 *)0x03000384)

/* EEPROM transfer size (halfword count for the current DMA operation). */
#define gEepromTransferSize   (*(u16 *)0x0300037A)

/* EEPROM transfer state flag. Cleared at start, set on completion. */
#define gEepromStateFlag      (*(u8 *)0x0300037C)

/* EEPROM configuration table pointer. Set by SelectTimerTableByMode;
 * entry[2] = sector count (used for bounds checking in verify/read). */
#define gEepromConfigTable    (*(u16 **)0x030066F0)

/* ROM EEPROM config tables: mode 4 (standard) and mode 0x40 (alternate). */
#define ROM_EEPROM_CONFIG_STD 0x08188DE4
#define ROM_EEPROM_CONFIG_ALT 0x08188DF0

/* GBA hardware register base address for timer index computation.
 * REG_IE, REG_IME, and individual timer regs are defined in gba.h. */
#define REG_TM0CNT_BASE       0x04000100

/* ── Input System ── */

/* Current frame pressed keys (active-high, edge-detected).
 * Written by ReadKeyInput each frame. */
#define gKeysPressed          (*(u16 *)0x03004DA0)

/* Previous frame raw key state (active-high).
 * Used for edge detection in ReadKeyInput. */
#define gKeysPrevious         (*(u16 *)0x030051E4)

/* Extended input state for ProcessInputAndTimers.
 * Separate from the simple ReadKeyInput state. */
#define gInputState           (*(u16 *)0x03004668)
#define gInputPrevious        (*(u16 *)0x0300362C)

/* ── Game State ── */

/* Pause flag: when non-zero, GameUpdate skips the main update loop. */
#define gPauseFlag            (*(vu8 *)0x030034E4)

/* Frame/tick counter — decremented each frame, byte-sized. */
#define gFrameCounter         (*(u8 *)0x03005498)

/* Sound reset flag: when non-zero, VBlankHandler calls SoundInit. */
#define gSoundResetFlag       (*(u8 *)0x03003420)

/* Interrupt Master Enable write address for VBlank acknowledge. */
#define gIMEAcknowledge       (*(u16 *)0x03007FF8)

/* ── Entity / Object System ── */

/* Base of the main entity/sprite struct array (~36 bytes per element).
 * Most-referenced address in the entire codebase (1634 refs). */
#define gEntityArray          ((u8 *)0x03002920)

/* OAM shadow buffer entry pointers (used by ClearVideoState and friends).
 * Linker symbols for specific offsets into gEntityArray. */
extern u32 gOamBuffer0[]; /* 0x03002920: entry 0 (full buffer) */
extern u32 gOamBuffer1[]; /* 0x0300293C: entry 1 (skip first) */
extern u32 gOamBuffer6[]; /* 0x03002A8C: entry 6 (skip first 6) */

/* OAM shadow buffer entry pointer (used for sprite attribute writes). */
#define gOamEntryPtr       (*(u32 *)0x03000820)

/* Entity work buffer / scratch space (allocated during transitions). */
#define gEntityWorkBuffer  ((u8 *)0x03002910)

/* Entity index/status lookup table (entity behavior state tracking). */
#define gEntityStatusTable ((u8 *)0x0300363C)

/* Current entity context pointer (0x40 bytes, entity field access). */
#define gCurrentEntityCtx  (*(u32 *)0x03004670)

/* Entity source table pointer (0x18 bytes, sprite/position data). */
#define gEntitySourcePtr   (*(u32 *)0x03004658)

/* Level/entity data pointer (dereferenced for level layout data). */
#define gLevelDataPtr      (*(u32 *)0x03005288)

/* Global control/state block — flags and status at word/byte offsets. */
#define gControlBlock      ((u8 *)0x03004C20)

/* Control block secondary flags (game state transitions, bit fields). */
#define gControlFlags      ((u8 *)0x03004C08)

/* Game state struct array (byte-field access, ~200+ bytes per entry). */
#define gGameStateArray    ((u8 *)0x03005220)

/* Game state flags struct (byte fields at various offsets). */
extern u8 gGameFlagsPtr[];
#define gGameFlags         ((u8 *)0x03005400)

/* Frame/animation/particle state buffer (entity behavior state). */
#define gAnimStateBuffer   ((u8 *)0x03003590)

/* Graphics/decompression buffer control (0x2C bytes). */
#define gGfxDecompCtrl     (*(u32 *)0x030047FC)

/* UI/rendering state block (byte fields with 4-bit masks). */
#define gUIRenderState     ((u8 *)0x030034B0)

/* BG layer configuration array (3 entries, see struct BGLayerState).
 * Initialized by InitLevelBG with charblock assignments:
 *   Entry 0: tileVram=0x06000000, tmapVram=0x0600E000
 *   Entry 1: tileVram=0x06004000, tmapVram=0x0600E800
 *   Entry 2: tileVram=0x06008000, tmapVram=0x0600F000 */
#define gBGLayerState      ((struct BGLayerState *)0x03003430)

/* Decompress/DMA buffer control struct.
 * Holds pointers to allocated decompression buffers during scene setup.
 *   +0x00: buf0  (tile data A / tilemap layer 0)
 *   +0x04: buf1  (tile data B / tilemap layer 1)
 *   +0x08: buf2  (tile data C)
 *   +0x0C: buf3  (tile data D)
 *   +0x14: buf5  (tilemap layer 2) */
#define gDecompBufferCtrl  ((u8 *)0x03004790)

/* Entity/object pointer (double indirection). */
#define gEntityPtr         (*(u32 *)0x03004654)

/* OAM source data / lookup table (halfword-indexed). */
#define gOamSourceTable    ((u16 *)0x03004680)

/* Status byte lookup table. */
#define gStatusTable       ((u8 *)0x03000830)

/* Mixed-mode struct (byte + halfword fields). */
#define gMixedState        ((u8 *)0x03003510)

/* VRAM write cursor: current palette RAM destination for DMA transfers.
 * Advanced by 0x20 after each 32-byte palette DMA during scene setup. */
#define gVramWriteCursor   (*(u32 *)0x030007DC)

/* Initial VRAM write cursor value, saved at stream init and restored on reset. */
#define gVramCursorInit    (*(u32 *)0x030034F4)

/* Palette VRAM write cursor: tracks current VRAM destination during
 * sequential tile/palette DMA transfers in scene setup. */
#define gPaletteVramCursor (*(u32 *)0x03005490)

/* Initial palette cursor value, saved at stream init and restored on reset. */
#define gPaletteCursorInit (*(u32 *)0x030052AC)

/* Pointer to decompressed collision/layout map data for current level. */
#define gCollisionMapPtr   (*(u32 *)0x03005290)

/* Level dimension bounds: +0x00=scrollX max, +0x02=scrollY max,
 * +0x04=width, +0x06=height. Set by InitLevelBG. */
#define gLevelBounds       ((u16 *)0x03005468)

/* Pointer to per-level runtime state. Used by tilemap streaming. */
#define gLevelStatePtr     (*(u32 *)0x030034A0)

/* Tilemap work buffer (0x400 bytes): temporary staging for tilemap
 * row/column streaming during BG scrolling. */
#define gTilemapWorkBuffer ((u8 *)0x03004DB0)

/* Screenblock staging buffers in IWRAM: DMA'd to VRAM during VBlank.
 * Each is 0x800 bytes (1024 halfwords = one 32x32 screenblock). */
#define gScreenBufferA     ((u8 *)0x03000900)
#define gScreenBufferB     ((u8 *)0x03001100)
#define gScreenBufferC     ((u8 *)0x03001900)

/* VBlank interrupt callback function pointer.
 * Set to different handlers depending on scene:
 *   FUN_080009D9 for normal levels
 *   FUN_08000CE1 for mode-7
 *   FUN_08000BD5 for title screen */
#define gVBlankCallback    (*(u32 *)0x030047C0)

/* VBlank callback array: two function pointers at 0x030047C0.
 * Set by SetupGfxCallbacks for handler dispatch. */
extern u32 gVBlankCallbackArray[];

/* Callback state array: function pointers and state at 0x03003510.
 * Used by transition functions for scene setup callbacks. */
extern u32 gCallbackStateArray[];

/* ── UI / Text Rendering ── */

/* Sprite attribute table for HUD/dialog rendering.
 * Array of 8-byte OAM-like entries at 0x03004800. */
#define gOamBuffer       ((u8 *)0x03004800)

/* Current sprite/OAM slot index for HUD. */
#define gSpriteSlotIndex (*(u16 *)0x0300466C)

/* Sprite draw count / limit. */
#define gSpriteDrawCount (*(u16 *)0x030051DC)

/* Sprite data table pointer (u32 stored at 0x030051DC by SetSpriteTableFromIndex). */
extern u32 gSpriteRenderPtr;

/* ROM sprite data lookup table (array of u32 pointers). */
extern const u32 gSpriteDataTable[];

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

/* Input/state dispatch table used by ProcessInputAndTimers.
 * Array of function pointers / state transition entries. */
#define ROM_STATE_DISPATCH_TABLE 0x080D9150

/* Sprite tileset sub-table used by LoadSpriteFrame.
 * Indexed by frame number within a tileset. */
#define ROM_SPRITE_SUBTABLE      0x0818B8A8

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

/* BG control register flags lookup table.
 * Used by InitLevelBG for REG_BG0CNT/BG1CNT/BG3CNT setup. */
#define ROM_BG_CONTROL_FLAGS     0x08051BD4

/* Extra BG tables for sublevel==0 (world map / special screens). */
#define ROM_BG_EXTRA_TILES_A     0x0818955C
#define ROM_BG_EXTRA_TILEMAPS_A  0x08189574
#define ROM_BG_OBJ_TILESET_TABLE 0x08189544

/* Per-level collision/layout map table. Decompressed into gCollisionMapPtr. */
#define ROM_COLLISION_MAP_TABLE  0x0818B7AC

/* Per-level parameter table, stored at 0x03005294. */
#define ROM_LEVEL_PARAM_TABLE    0x08189A24

/* Layer configuration sub-tables used by SetupLevelLayerConfig.
 * Define per-layer charblock/screenblock/scroll/dimension properties. */
#define ROM_LAYER_SCROLL_FLAGS   0x080576D4
#define ROM_LAYER_WIDTH_TABLE    0x08057714
#define ROM_LAYER_HEIGHT_TABLE   0x08057794
#define ROM_LAYER_VSCROLL_TABLE  0x08057814
#define ROM_LAYER_TILE_BPP       0x08057894
#define ROM_LAYER_CHARBLOCK_IDX  0x080578D4
#define ROM_LAYER_SCREENBLOCK    0x08057914

/* ── Scene-Specific Shared Tilesets ── */

/* These compressed tilesets are loaded into VRAM charblocks during scene init
 * (SetupSceneGfx / FUN_0804886c). They provide shared tiles (HUD, items, etc.)
 * that are referenced by per-level BG tilemaps via absolute tile IDs. */
#define ROM_SCENE_TILESET_A      0x08366214 /* -> charblocks 0-3 via palettePtr */
#define ROM_SCENE_TILESET_B      0x08367468 /* -> small OBJ tiles */
#define ROM_SCENE_TILES_CB0      0x082F4D3C /* -> VRAM 0x06000000 (charblock 0) */
#define ROM_SCENE_TILES_CB1      0x082F518C /* -> VRAM 0x06004000 (charblock 1) */
#define ROM_SCENE_TILES_CB2      0x082F5D0C /* -> VRAM 0x06008000 (charblock 2) */
#define ROM_SCENE_TILES_CB3      0x082F7D64 /* -> VRAM 0x0600C000 (charblock 3) */
#define ROM_SCENE_TILEMAP_DATA   0x082F5920 /* -> IWRAM tilemap buffers */

/* Scene-specific palette data loaded during SetupSceneGfx. */
#define ROM_SCENE_PALETTE_A      0x08078F88
#define ROM_SCENE_PALETTE_B      0x08078FA8

/* OBJ (sprite) tileset for scene overlay. */
#define ROM_SCENE_OBJ_TILES      0x082F4934

/* Sprite layout table for HUD/scene overlay objects.
 * 12-byte entries terminated by 0xFFFF. */
#define ROM_SCENE_SPRITE_TABLE   0x08116590

/* Per-level BG palette table.
 * Indexed by level index; each entry is a ROM pointer to compressed
 * 0x1C0-byte palette data for BG layers. Used by FinalizeLevelLayerSetup. */
extern const u32 gLevelPaletteTable[];
#define ROM_LEVEL_PALETTE_TABLE 0x08189B4C

/* GFX data stream pointer table.
 * Indexed by stream ID; each entry is a ROM pointer to compressed
 * stream data. Used by LoadAndDecompressStream. */
extern const u32 gStreamDataTable[];
#define ROM_STREAM_TABLE     0x08189AFC

/* World map BG tile data. */
#define ROM_WORLDMAP_TILES   0x082EA584
#define ROM_WORLDMAP_TILEMAP 0x082EA730
#define ROM_WORLDMAP_PALETTE 0x082EA7F0

/* ── Sound ROM Data Tables ── */

/* MusicPlayer: 12-byte entry in the music player table.
 *   +0x00: u32 info   - pointer to MusicPlayerInfo struct
 *   +0x04: u32 track  - pointer to MusicPlayerTrack array
 *   +0x08: u16 numTracks, u8 unk_A, u8 pad */
struct MusicPlayer {
    u32 info;
    u32 track;
    u16 numTracks;
    u8 unk_A;
    u8 pad;
};

/* Song: 8-byte entry in the song table.
 *   +0x00: u32 header - pointer to song header data
 *   +0x04: u16 ms     - index into MusicPlayer table
 *   +0x06: u16 pad */
struct Song {
    u32 header;
    u16 ms;
    u16 pad;
};

/* Music player table: array of struct MusicPlayer (12 bytes each).
 * Indexed by Song.ms to find the player for a given song. */
extern const struct MusicPlayer gMPlayTable[];
#define ROM_MUSIC_TABLE 0x08118AB4

/* Song metadata table: array of struct Song (8 bytes each).
 * Indexed by song ID to find header and player assignment. */
extern const struct Song gSongTable[];
#define ROM_MUSIC_META_TABLE 0x08118AE4

/* Sound command dispatch table.
 * Array of function pointers indexed by command byte. */
extern const u32 gSoundCmdTable[];
#define ROM_SOUND_CMD_TABLE   0x08117C8C

/* Instrument/voice table.
 * Contains waveform, envelope, and pitch data for each instrument. */
#define ROM_INSTRUMENT_TABLE  0x081179E4

/* Pitch/frequency lookup tables for MIDI note-to-frequency conversion. */
#define ROM_FREQ_TABLE_1      0x08117A74
#define ROM_FREQ_TABLE_2      0x08117B28
#define ROM_PITCH_TABLE       0x08117B70
#define ROM_WAVE_DUTY_TABLE   0x08117BF4
#define ROM_NOISE_TABLE       0x08117C0C
#define ROM_ENVELOPE_TABLE    0x08117C48
#define ROM_SWEEP_TABLE       0x08117C58

/* Sound configuration init data. */
#define ROM_SOUND_INIT_DATA   0x081177E4

/* ── Camera / Scroll State ── */

/* Camera state struct (accessed with s16 fields at various offsets).
 * Offset +0x0C low nibble = camera mode index (0-7, switch in CameraModeSwitchHandler).
 * Used by UpdateScrollPosition, UpdatePlayer*, CameraModeSwitchHandler. */
#define gCameraState          ((u8 *)0x030007E0)

/* Camera/scroll limits computed from level dimensions.
 * Used by UpdateScrollPosition, ComputeScrollLimits, InitLevelState. */
#define gScrollLimits         ((u8 *)0x030007CC)

/* Main loop state flag: controls game loop flow in MainGameFrameLoop.
 * Checked at loop entry and after transitions. */
#define gMainLoopState        (*(u8 *)0x030007F8)

/* ── BG2 Affine Transform Shadows ── */
/* These IWRAM values are written to hardware registers during VBlank:
 *   gBG2PA → REG_BG2PA (0x04000020) — horizontal scale / cos(angle)
 *   gBG2PB → REG_BG2PB (0x04000022) — horizontal shear / sin(angle)
 *   gBG2PC → REG_BG2PC (0x04000024) — vertical shear / -sin(angle)
 *   gBG2PD → REG_BG2PD (0x04000026) — vertical scale / cos(angle)
 *   gBG2X  → REG_BG2X  (0x04000028) — reference point X (32-bit fixed-point)
 *   gBG2Y  → REG_BG2Y  (0x0400002C) — reference point Y (32-bit fixed-point) */
#define gBG2PA                (*(u16 *)0x030047B0)
#define gBG2PB                (*(u16 *)0x03005464)
#define gBG2PC                (*(u16 *)0x030051BC)
#define gBG2PD                (*(u16 *)0x03000808)
#define gBG2X                 (*(u32 *)0x030007FC)
#define gBG2Y                 (*(u32 *)0x030051D0)

/* ── Scene / Transition State ── */

/* Scene fade/blend counter: decremented by 0x10 each frame during transitions.
 * Used by TransitionInitLevelMusic, TransitionFadeOut*, GameplayFrameInit. */
#define gSceneFadeCounter     (*(u16 *)0x03005210)

/* Scene/gfx state struct: used by InitGfxState, InitFadeTransition,
 * MainGameFrameLoop, PlayerMovementPhysics. Part of the graphics pipeline state. */
#define gGfxSceneState        ((u8 *)0x03004D90)

/* Scene script / title sequence state.
 * Used by RunSceneScript, RunTitleSequence. */
#define gSceneScriptState     (*(u32 *)0x03005488)

/* Cutscene/credits animation state.
 * Used by VBlankCallback_Credits, VBlankCallback_Cutscene, TransitionFadeOutWithMusic. */
#define gCutsceneState        ((u8 *)0x030051C8)

/* ── Entity Subsystem (extended) ── */

/* Entity spawn state: byte fields at offsets 0-3.
 * +0x00: entity count A, +0x01: entity count B,
 * +0x02: palette anim slot, +0x03: palette anim param.
 * Used by SpawnEntityAtPosition, EntityBehaviorMasterUpdate, EntitySpriteFrameUpdate. */
#define gEntitySpawnState     ((u8 *)0x03003610)

/* Entity collision/physics lookup table: indexed by entity slot offset.
 * Used by EntityGravityAndFloorCheck, PlayerMainUpdate, SetupOAMSprite. */
#define gEntityCollisionTable ((u8 *)0x03000790)

/* Entity death/hit reaction state.
 * Used by EntityDeathAnimation, EntityHitReaction, SpawnEntityAtPosition. */
#define gEntityDeathState     ((u8 *)0x0300528C)

/* OAM sprite processing state byte: compared against 0xFE sentinel.
 * Used by ProcessOamSpriteLayout, HandlePauseMenuInput, InitLevelBG,
 * UpdateScrollPosition, UpdateStageSelectScreen. */
#define gOamProcessState      (*(u8 *)0x030052A0)

/* ── Palette / Visual Effects ── */

/* Palette animation state buffer.
 * Used by AnimatePaletteEffects, InitGfxState, PlayerMovementPhysics. */
#define gPaletteAnimState     ((u8 *)0x030051F0)

/* ── Gameplay / UI State (extended) ── */

/* Gameplay mode/sub-state: byte accessed with DMA multiplier (×0x800).
 * Used by ProcessInputAndUpdateEntities, UpdateUIState, HandlePauseMenuInput,
 * AnimatePaletteEffects. Controls OAM DMA source selection. */
#define gGameplayMode         (*(u8 *)0x030034BC)

/* Gameplay mode secondary state.
 * Used alongside gGameplayMode by ProcessInputAndUpdateEntities, InitGameplayState. */
#define gGameplayModeAlt      ((u8 *)0x030034C0)

/* ── Save System ── */

/* Save data buffer pointers: four IWRAM addresses used together by
 * SaveGameWithVerify, SavePlayerProgress, SaveGameRetryLoop.
 * Each holds a pointer or buffer for EEPROM read/write operations. */
#define gSaveBuffer0          (*(u32 *)0x0300520C)
#define gSaveBuffer1          (*(u32 *)0x03005208)
#define gSaveBuffer2          (*(u32 *)0x0300465C)
#define gSaveBuffer3          (*(u32 *)0x030008E4)

/* ── Sound / DMA (extended) ── */

/* GBA BIOS sound info pointer at 0x03007FF0.
 * Standard m4a/MusicPlayer2000 location for SoundArea struct.
 * Used by InitSoundEngine, SoundHardwareInit, DirectSoundFifoSetup, CgbChannelMix. */
#define gBiosSoundInfo        (*(u32 **)0x03007FF0)

/* ── Entity Data Tables ── */

/* Entity data table: 8-byte entries indexed by entity type.
 * Used by GetEntityLookupData to read behavior parameters.
 * Offset +5: param A, offset +6: param B. */
extern const u8 gEntityDataTable[];
#define ROM_ENTITY_DATA_TABLE   0x081168E8

/* Entity animation/behavior data table (172 refs, most-referenced unnamed ROM address).
 * Halfword-indexed: value >> 2 + 0xC0 or byte + 0x40, scaled ×2.
 * Used by EntityBoss*, FUN_080158ac, FUN_0801af28, and many entity behavior functions. */
#define ROM_ENTITY_ANIM_TABLE   0x080D8E14

/* Entity sprite attribute table (129 refs).
 * Used by PlayerFollowEntityMovement and entity rendering.
 * Adjacent to ROM_OAM_TEMPLATE (0x080E2A7C), likely extended OAM data. */
#define ROM_ENTITY_SPRITE_TABLE 0x080E2B64

#endif /* GUARD_GLOBALS_H */
