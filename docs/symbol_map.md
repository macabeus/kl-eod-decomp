# Symbol Map

Semantic names for not-yet-decompiled functions, identified by analyzing
call graphs, hardware register usage, ROM data references, and WRAM access patterns.

These names are **proposed** — they become official when the function is decompiled
and added to `[renames]` in `klonoa-eod-decomp.toml`.

## Text / UI Rendering Pipeline (code_0)

| Address | Proposed Name | Evidence |
|---------|---------------|----------|
| FUN_0800d188 | TextStateMachine | 6147-line function; refs REG_BLDCNT/BLDALPHA, gEntityArray, gFrameCounter; calls DrawSpriteTilesPartial, WaitForVBlank; master UI/text state machine |
| FUN_0800b3c0 | RenderCharacterTiles | Refs ROM sprite table 0x0800B7B4, gEntityArray, gRenderFlags; calls DrawSpriteTiles, DrawSpriteTilesFlipped, WaitForVBlank; character tile renderer |
| FUN_08009064 | RenderDialogBox | Refs ROM_SPRITE_FRAME_TABLE (0x08078FC8 ×3), gSpriteSlotIndex, gEntityArray, gStatusTable; no bl calls; dialog/message box sprite layout |
| FUN_080098c8 | RenderDialogSprites | Refs ROM_SPRITE_FRAME_TABLE (×4), gOamBuffer, gEntityArray; calls InitOamEntries; dialog sprite rendering |
| FUN_080070a0 | RenderMenuUI | Refs ROM_SPRITE_FRAME_TABLE (×11), 30+ WRAM globals; calls InitOamEntries, RenderDialogBox; master menu/HUD renderer |
| FUN_08005cf4 | RenderHUDTop | Refs ROM_SPRITE_FRAME_TABLE, gOamBuffer, gEntityArray; calls InitOamEntries; top HUD element rendering |
| FUN_08005fa4 | RenderHUDBottom | Refs ROM_SPRITE_FRAME_TABLE (×6), gEntityArray, gStatusTable; no bl calls; bottom HUD element rendering |
| FUN_0800ca0c | SetupDisplayConfig | Refs ROM_DISPLAY_CONFIG_TABLE (0x080D821C), 20+ WRAM globals; calls FUN_08046db8; configures display modes and layer setup |
| FUN_0800ac34 | UpdateUIState | Calls TextStateMachine, WaitForVBlank, PlaySoundEffect; manages UI state transitions |
| FUN_0800bef0 | UpdateTextScroll | Refs gTextScrollState (0x030034DC); text scroll/advance logic |

## Engine Rendering Pipeline (engine)

| Address | Proposed Name | Evidence |
|---------|---------------|----------|
| FUN_08003dc0 | DrawSpriteTiles | 3971 lines; no bl/pool; core sprite/tile VRAM writer (pure register computation) |
| FUN_08003d80 | DrawSpriteTilesPartial | Near DrawSpriteTiles; partial tile rendering variant |
| FUN_08003da0 | DrawSpriteTilesFlipped | Between the two DrawSprite functions; flipped tile variant |
| FUN_08001158 | InitGraphicsSystem | Calls AllocAndDecompress, DecompressData, CopyDataToVram, SetupVBlankSoundHandler; refs all VRAM regions, BG control regs; full graphics initialization |
| FUN_08003904 | RenderFrame | Calls DrawSpriteTiles (×11), StopAllSound, UpdateAllSoundChannels, RenderCharacterTiles, SetupVBlankSoundHandler; per-frame rendering dispatch |

## Entity / Object System (code_3)

| Address | Proposed Name | Evidence |
|---------|---------------|----------|
| FUN_0803ac18 | IsEntityActive | Called by UpdateEntities in loop; returns bool per entity slot |
| FUN_0803ad94 | UpdateEntityState | Called by UpdateEntities for active entities; first update pass |
| FUN_0803af38 | UpdateEntityAnimation | Called by UpdateEntities for active entities; second update pass |
| FUN_080468b0 | UpdateGameLogic | Called by GameUpdate when not paused; first subsystem update |
| FUN_08045874 | UpdatePhysics | Called by GameUpdate; second subsystem update |
| FUN_08045f68 | UpdateCollision | Called by GameUpdate; third subsystem update |
| FUN_08046288 | UpdateCamera | Called by GameUpdate; fourth subsystem update |

## Memory / Asset Management (code_3)

| Address | Proposed Name | Evidence |
|---------|---------------|----------|
| FUN_08043af4 | DecompressData | Called by AllocAndDecompress and InitGraphicsSystem; decompression routine |
| FUN_08043b34 | CopyDataToVram | Called by InitGraphicsSystem; bulk data copy |

## Sound Engine (m4a)

### Sound System Init & Shutdown

| Address | Proposed Name | Evidence |
|---------|---------------|----------|
| FUN_0804eb64 | SoundMain | Main sound system setup; calls stream/gfx init, refs gSoundInfo |
| FUN_0804ed68 | SoundDmaInit | DMA controller setup for sound; has DMA reg writes |
| FUN_0804ee34 | SoundReset | Minimal state reset (leaf, 23 lines) |
| FUN_0804ee60 | DmaControllerInit | Full DMA init; calls SoundReset |
| FUN_0804ef50 | SoundInfoInit | Initialize SoundInfo struct fields (leaf) |
| FUN_0804f294 | InitSoundEngine | Called by SoundInit wrapper |

### Sound Data & Buffer Management

| Address | Proposed Name | Evidence |
|---------|---------------|----------|
| FUN_0804efde | SoundBufferAlloc | Allocate mixing buffers |
| FUN_0804f004 | SoundContextInit | Setup mixer state; refs gSoundInfo, gStreamPtr, gControlBlock |
| FUN_0804f0d0 | SoundChannelTableInit | Initialize channel table entries |

### MIDI / Music Sequence Engine

| Address | Proposed Name | Evidence |
|---------|---------------|----------|
| FUN_0804f180 | MidiReadUnaligned | Read misaligned MIDI value; calls ReadUnalignedU16 |
| FUN_0804f1c4 | MidiProcessEvent | Dispatch MIDI note/control event |
| FUN_0804f248 | MPlayMain | **CORE**: main music player tick (587 lines, largest in m4a, uses DMA) |
| FUN_0804f6d4 | VoiceUtil | Minimal voice utility (leaf, 22 lines) |
| FUN_0804f6f4 | VoiceLookup | Voice lookup wrapper |
| FUN_0804f724 | InstrumentLookup | ROM instrument table lookup (ROM_INSTRUMENT_TABLE) |
| FUN_0804f73c | InstrumentGetEntry | Get instrument entry from ROM (leaf) |
| FUN_0804f758 | MidiDecodeByte | Decode single MIDI byte (leaf, 11 lines) |
| FUN_0804f766 | MidiNoteSetup | Setup MIDI note on channel |
| FUN_0804f7b4 | MidiCommandHandler | MIDI command dispatch table; writes REG_SOUND1CNT_L |
| FUN_0804f944 | MPlayContinue | Music playback continuation (327 lines) |
| FUN_0804fb9c | SoundContextRef | Get sound context reference |

### Music Playback Control

| Address | Proposed Name | Evidence |
|---------|---------------|----------|
| FUN_0804fbe0 | MPlayStop_Channel | Stop single music channel (leaf) |
| FUN_0804fc10 | MPlayStop | Stop all music playback (313 lines) |
| FUN_0804fe50 | SoundEffectUtil | Sound effect utility (leaf) |
| FUN_0804fe6c | SoundEffectProcess | Process sound effect chain |
| FUN_0804fea0 | FreqTableLookup | Frequency lookup from ROM pitch tables |
| FUN_0804ff08 | MPlayChannelReset | Reset channel state; checks Sappy magic 0x68736D53 |
| FUN_0804ff44 | m4aSoundInit_Impl | Full sound system init dispatcher |
| FUN_0804ffc8 | m4aSongNumStart | Start playing music track by ID (ROM_MUSIC_TABLE) |
| FUN_0804fff6 | m4aSongNumContinue | Continue/queue music track |
| FUN_08050042 | m4aSongNumLoad | Load music data from ROM |
| FUN_08050094 | m4aMPlayCommand | Execute music player command |
| FUN_080500c8 | m4aSongNumStop | Stop current music track |
| FUN_080500fc | m4aSoundVSync | VBlank: update all sound channels (×4 loop) |

### Sound Hardware & Direct Sound

| Address | Proposed Name | Evidence |
|---------|---------------|----------|
| FUN_08050134 | m4aSoundVSyncSetup | Setup VBlank sound sync |
| FUN_08050162 | SappyStateCheck | Verify Sappy magic marker (0x68736D53) |
| FUN_080501ba | SoundEffectTrigger | Trigger sound effect via gMPlayInfo_SE |
| FUN_08050200 | SoundHardwareInit | **CRITICAL**: init all GBA sound regs (14 HW regs: SOUNDCNT, FIFO, DMA) |
| FUN_08050344 | DirectSoundFifoSetup | **CRITICAL**: FIFO_A/B and DMA config (8 HW regs) |
| FUN_0805043c | SoundTimerSetup | Configure timer for sample rate |
| FUN_080504e0 | SoundSystemConfigure | Configure sound system mode |
| FUN_08050578 | SoundPlatformDetect | Detect platform/capabilities |
| FUN_080505cc | m4aSoundShutdown | Emergency stop all sound |
| FUN_08050648 | m4aSoundVSyncOn | Register VBlank sound handler |
| FUN_08050684 | VBlankSoundCallback | VBlank-triggered sound update |
| FUN_080506fc | MPlayOpen | Load & open music player data from ROM |
| FUN_080507e0 | MPlayChannelUpdate | Update single music channel |

### CGB Sound (Channels 1-4) & Pitch Control

| Address | Proposed Name | Evidence |
|---------|---------------|----------|
| FUN_08050820 | CgbModVol | CGB channel volume modulation |
| FUN_080508e8 | CgbLookupTable | CGB frequency/volume lookup data (leaf) |
| FUN_0805099e | MidiKeyToCgbFreq | MIDI note → CGB frequency; writes 4 pitch regs |
| FUN_08050a94 | CgbLookupUtil | CGB utility lookup (leaf) |
| FUN_08050afc | CgbSound | CGB channel hardware control (14 HW regs: SOUND1-4CNT, WAVE_RAM) |
| FUN_08050c70 | SoundMixerMain | **CORE**: process all mixer channels (393 lines, 2nd largest) |

### Sound State Machine & MIDI Encoding

| Address | Proposed Name | Evidence |
|---------|---------------|----------|
| FUN_08050f4a | SoundStateCheck1 | Sappy magic state check (leaf) |
| FUN_08050f70 | SoundStateCheck2 | Sappy magic state check (leaf) |
| FUN_08050fd8 | SoundStateCheck3 | Sappy magic state check (leaf) |
| FUN_0805104c | MidiNoteLookup | MIDI note to frequency lookup (leaf) |
| FUN_080510b4 | MidiUtilConvert | MIDI utility converter (leaf) |
| FUN_080510d4 | MidiCommandEncode1 | Encode MIDI command type 1 |
| FUN_08051148 | MidiCommandEncode2 | Encode MIDI command type 2 |
| FUN_080511bc | MPlayCommandDispatch | Music command dispatcher (ROM table, 174 lines) |
| FUN_08051314 | SoundCommandHandler | Command dispatch from ROM_SOUND_CMD_TABLE |
| FUN_08051348 | BitMaskLUT | 32-bit channel bitmask lookup table (leaf, 118 lines) |
| FUN_0805186c | PlaySoundEffect | Play sound effect; called by PlaySoundWithContext_D8/DC |
| FUN_08051870 | DispatchSoundCommand | Dispatch sound command; called by SoundCommand_6450 |

## System / Utility

| Address | Proposed Name | Evidence |
|---------|---------------|----------|
| FUN_08025ba4 | VBlankWaitAndUpdate | Called by GameUpdate unconditionally at end |
| FUN_08025b78 | WaitForVBlank | Called by RenderCharacterTiles (×7), TextStateMachine (×3) |
| FUN_0804c050 | FinalizeGfxStream | Called by ShutdownGfxStream |
| FUN_0804c0ec | ProcessStreamOpcode | Called by DispatchStreamCommand_C0EC |
| FUN_0804c218 | ExecuteStreamCommand | Called by ProcessStreamCommand_C218 |
| FUN_08050094 | ExecuteMusicCommand | Called by ProcessStreamCommand_50094 |
| FUN_080008dc | MemoryCopy | Called by TextStateMachine |
| FUN_0800a468 | InitOamEntries | Inits 128 OAM entries from template; called by RenderMenuUI, RenderDialogSprites, RenderHUDTop |

## ROM Data Tables

| Address | Name | Description |
|---------|------|-------------|
| 0x08078FC8 | ROM_SPRITE_FRAME_TABLE | Sprite frame/animation data; array of {count, dataPtr} pairs |
| 0x080D821C | ROM_DISPLAY_CONFIG_TABLE | Display configuration / sprite mapping table |
| 0x080E2A7C | ROM_OAM_TEMPLATE | OAM template data (initial attribute values) |
| 0x0818B7AC | ROM_GFX_ASSET_TABLE | Graphics asset table for InitGraphicsSystem |
| 0x0818B8E0 | ROM_TILESET_TABLE | Tileset table for RenderFrame |
| 0x0800B7B4 | ROM_CHAR_TILE_MAP | Character-to-tile mapping for RenderCharacterTiles |

### Sound ROM Data Tables

| Address | Name | Description |
|---------|------|-------------|
| 0x08118AB4 | ROM_MUSIC_TABLE | Music track table: {count, trackDataPtr} entries indexed by track ID |
| 0x08118AE4 | ROM_MUSIC_META_TABLE | Music track metadata (offsets, lengths, loop points) |
| 0x08117C8C | ROM_SOUND_CMD_TABLE | Sound command dispatch: function pointer array by command byte |
| 0x081179E4 | ROM_INSTRUMENT_TABLE | Instrument/voice data: waveform, envelope, pitch per instrument |
| 0x08117A74 | ROM_FREQ_TABLE_1 | Pitch/frequency lookup table 1 |
| 0x08117B28 | ROM_FREQ_TABLE_2 | Pitch/frequency lookup table 2 |
| 0x08117B70 | ROM_PITCH_TABLE | MIDI note-to-pitch conversion table |
| 0x08117BF4 | ROM_WAVE_DUTY_TABLE | Square wave duty cycle table |
| 0x08117C0C | ROM_NOISE_TABLE | Noise channel parameter table |
| 0x08117C48 | ROM_ENVELOPE_TABLE | Volume envelope data |
| 0x08117C58 | ROM_SWEEP_TABLE | Frequency sweep data |
| 0x081177E4 | ROM_SOUND_INIT_DATA | Sound configuration init data |
