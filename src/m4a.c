#include "global.h"
#include "gba.h"
#include "globals.h"
#include "include_asm.h"

/* ══════════════════════════════════════════════════════════════════════
 * m4a — Nintendo MusicPlayer2000 ("Sappy") sound engine
 *
 * Format: 39 songs + 126 SFX stored as MIDI-like bytecode in ROM.
 * Song table at ROM_MUSIC_META_TABLE (0x08118AE4).
 * 3 voice/instrument tables, ~300KB of signed 8-bit PCM samples.
 * Uses Direct Sound (FIFO A/B via DMA) + CGB channels 1-4.
 * ══════════════════════════════════════════════════════════════════════ */

/* ── Sound System Initialization & Shutdown ── */

/*
 * SoundMain: top-level sound system initialization.
 * Sets up gSoundInfo, initializes the stream/gfx subsystems,
 * and prepares the sound engine for playback.
 *   248 lines, calls FUN_0804c898/FUN_0804df80/FUN_0804e008
 *   refs: gSoundInfo (0x0300081C), gGfxBufferPtr (0x030034A0)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804eb64);
/*
 * SoundDmaInit: DMA controller setup for sound data transfers.
 * Configures DMA channels for PCM sample streaming to FIFO.
 *   79 lines, has DMA register writes (REG_DMA3SAD/DAD/CNT)
 *   calls: thunk_FUN_080001e0 (memory alloc)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804ed68);
/*
 * Frees the sound info struct and its inner sample buffer.
 * Reads gSoundInfo, frees the inner buffer (at offset -4),
 * then frees the struct itself.
 *   no parameters
 *   no return value
 */
void FreeSoundStruct(void) {
    u32 *p = (u32 *)0x0300081C; /* &gSoundInfo */
    thunk_FUN_0800020c(*(u32 *)(*p) - 4);
    thunk_FUN_0800020c(*p);
}
/*
 * SoundReset: minimal sound state reset.
 * Clears active sound state without full reinitialization.
 *   23 lines, leaf function
 *   refs: gEntityArray (0x03000900)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804ee34);
/*
 * DmaControllerInit: full DMA initialization with channel config.
 * Sets up DMA for both Direct Sound channels (FIFO A and B).
 *   125 lines, has DMA register writes
 *   calls: SoundReset (FUN_0804ee34)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804ee60);
/*
 * SoundInfoInit: initialize SoundInfo struct fields.
 * Fills in default values for the sound info structure at gSoundInfo.
 *   73 lines, leaf function
 *   refs: gSoundInfo (0x0300081C), gStreamPtr (0x03004D84)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804ef50);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804efa2);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804efbe);

/* ── Sound Data & Buffer Management ── */

/*
 * SoundBufferAlloc: allocate sound mixing buffers.
 * Allocates WRAM buffers for the PCM mixing pipeline.
 *   21 lines, calls FUN_08051868
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804efde);
/*
 * SoundContextInit: setup sound context and mixer state.
 * Initializes the mixer configuration, channel assignments, and
 * links the sound context to the global control structures.
 *   102 lines, leaf function
 *   refs: gSoundInfo, gStreamPtr, gControlBlock (0x03004C20)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f004);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f036);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f04a);
/*
 * SoundChannelTableInit: initialize sound channel table entries.
 * Sets up the per-channel state for all active sound channels.
 *   86 lines, calls FUN_08051868
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f0d0);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f116);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f136);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f15a);

/* ── MIDI / Music Sequence Processing ── */

/*
 * MidiReadUnaligned: read a potentially unaligned MIDI value.
 * Used to parse variable-length MIDI data from track bytecode.
 *   32 lines, calls ReadUnalignedU16
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f180);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f1b6);
/*
 * MidiProcessEvent: dispatch a MIDI note or control event.
 * Processes a single event from the track bytecode stream,
 * handling note-on, note-off, and control change messages.
 *   72 lines, calls DmaControllerInit (FUN_0804ee60)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f1c4);
/*
 * MPlayMain_SetAndProcess: stores a value into gSoundInfo[0xD]
 * then calls MidiProcessEvent (FUN_0804f1c4).
 *   r0: value to store
 *   10 lines (split from former 587-line MPlayMain)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f248);
/*
 * MPlayMain: CORE music player tick — largest function in m4a.
 * Called each frame to advance all active music tracks. Reads track bytecode,
 * processes MIDI commands, manages DMA transfers for PCM playback, and
 * updates channel state.
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f25e);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f6ba);

/* ── Voice / Instrument Utilities ── */

/*
 * VoiceUtil: minimal voice utility function.
 *   22 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f6d4);
/*
 * VoiceLookup: voice parameter lookup wrapper.
 *   28 lines, calls VoiceUtil (FUN_0804f6d4)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f6f4);
/*
 * InstrumentLookup: look up instrument data from ROM_INSTRUMENT_TABLE.
 * Given a program/voice number, returns a pointer to the instrument entry
 * (12-byte voice struct: type, key, samplePtr, ADSR).
 *   14 lines, calls InstrumentGetEntry (FUN_0804f73e)
 *   refs: ROM_INSTRUMENT_TABLE (0x081179E4)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f724);
/*
 * InstrumentGetEntry: get a specific instrument entry from ROM.
 * Indexes into the voice/instrument table and returns the entry data.
 *   20 lines, leaf function
 *   refs: ROM_INSTRUMENT_TABLE (0x081179E4)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f73c);

/* ── Sound Channel Control ── */

/*
 * MidiDecodeByte: decode a single MIDI byte from the track stream.
 * Reads one byte, advances the stream position, and returns the value.
 *   11 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f758);
/*
 * MidiNoteSetup: set up a MIDI note-on event on a sound channel.
 * Configures the channel's pitch, volume, and instrument for playback.
 *   42 lines, calls InstrumentGetEntry (FUN_0804f73c)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f766);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f786);
/*
 * MidiCommandHandler: MIDI command dispatch table handler.
 * Processes track bytecode commands (0xB1-0xCF): tempo, voice select,
 * volume, pan, pitch bend, loops, etc. Writes to REG_SOUND1CNT_L
 * for CGB channel control.
 *   192 lines, writes to REG_SOUND1CNT_L (0x04000060), REG_DMA1SAD (0x040000BC)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f7b4);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f936);

/* ── Music Playback Engine ── */

/*
 * MPlayContinue: continue music playback for all active tracks.
 * Processes pending MIDI events, updates note timing, manages
 * voice allocation, and coordinates multi-track playback.
 *   327 lines
 *   calls: PlaySoundWithContext_D8/DC, FUN_08050820
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f944);
/*
 * SoundContextRef: get a reference to the current sound context.
 * Returns the active MusicPlayer context for the caller.
 *   39 lines, calls FUN_0804fb8c
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fb9c);
/*
 * MPlayStop_Channel: stop playback on a single music channel.
 * Silences one channel without affecting other active tracks.
 *   28 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fbe0);
/*
 * MPlayStop: stop all music playback across all tracks.
 * Silences all channels, resets playback state, and releases
 * voice allocations. Third largest function in m4a (313 lines).
 *   313 lines
 *   calls: PlaySoundWithContext_D8 (PlaySoundWithContext_D8)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fc10);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fe10);

/* ── Sound Effect Processing ── */

/*
 * SoundEffectUtil: sound effect utility / parameter setup.
 *   18 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fe50);
/*
 * SoundEffectProcess: process a sound effect playback chain.
 * Handles SFX queuing, priority, and channel assignment.
 *   24 lines, calls SoundEffectUtil (FUN_0804fe50)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fe6c);
/*
 * FreqTableLookup: look up pitch/frequency from ROM tables.
 * Converts MIDI note numbers to hardware frequency values using
 * ROM_FREQ_TABLE_1 (0x08117A74) and ROM_FREQ_TABLE_2 (0x08117B28).
 *   51 lines, calls FUN_0804f284
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fea0);
/*
 * MPlayChannelReset: reset a music player channel to idle state.
 * Checks the Sappy magic marker (SAPPY_MAGIC = 0x68736D53 = "Smsh")
 * to verify the sound engine is initialized before resetting.
 *   33 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804ff08);

/* ── Sound Init Dispatcher & Track Control ── */

/*
 * m4aSoundInit_Impl: full sound system initialization dispatcher.
 * Calls BitUnPack for data decompression, sets up sound channels,
 * and prepares the music player for first use.
 *   50 lines
 *   calls: BitUnPack, FUN_08050200, FUN_08050344
 *   refs: ROM_MUSIC_TABLE (0x08118AB4)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804ff44);
/*
 * Wrapper that calls InitSoundEngine (FUN_0804f294) to initialize
 * the MusicPlayer2000 sound engine.
 *   no parameters
 *   no return value
 */
void SoundInit(void) {
    FUN_0804f294();
}
/*
 * m4aSongNumStart: start playing a music track by song ID.
 * Looks up the song header from ROM_MUSIC_TABLE (0x08118AB4) and
 * ROM_MUSIC_META_TABLE (0x08118AE4), then begins playback.
 *   r0: song ID (0-38 for BGM, 39+ for SFX)
 *   21 lines, calls LoadSoundData (FUN_080506fc)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", InitSceneState);
/*
 * m4aSongNumContinue: continue or queue a music track.
 * If the same song is already playing, does nothing.
 * Otherwise queues the song for the next VBlank cycle.
 *   38 lines, refs: ROM_MUSIC_TABLE, ROM_MUSIC_META_TABLE
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fff6);
/*
 * m4aSongNumLoad: load music data from ROM into WRAM.
 * Copies track headers and sets up the MusicPlayer state for
 * a new song without starting playback.
 *   41 lines
 *   calls: MPlayChannelReset (FUN_0804ff08), LoadSoundData (FUN_080506fc)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050042);
/*
 * m4aMPlayCommand: execute a music player command.
 * Processes high-level commands like fade, tempo change, etc.
 *   r0: command ID
 *   26 lines, refs: ROM_MUSIC_TABLE, ROM_MUSIC_META_TABLE
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050094);
/*
 * m4aSongNumStop: stop the currently playing music track.
 * Halts playback and releases all voices for the active song.
 *   26 lines, calls MPlayChannelReset (FUN_0804ff08)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080500c8);
/*
 * m4aSoundVSync: VBlank sound update — called every frame.
 * Loops over 4 sound channels, calling MPlayChannelUpdate for each.
 * This is the main per-frame entry point for the sound engine.
 *   23 lines, refs: ROM_MUSIC_TABLE (0x08118AB4)
 *   calls: MPlayChannelUpdate (FUN_080507e0)
 */
/**
 * StopAllMusicPlayers: stops all 4 music player instances.
 * Iterates ROM_MUSIC_TABLE (0x08118AB4), calling FUN_080507e0 on each.
 */
INCLUDE_ASM("asm/nonmatchings/m4a", StopAllMusicPlayers);
/*
 * Wrapper that calls MPlayChannelReset (FUN_0804ff08) to stop/reset
 * a single sound channel.
 *   r0: pointer to sound channel struct
 *   no return value
 */
void StopSoundChannel(u32 r0) {
    FUN_0804ff08(r0);
}

/* ── Interrupt & VBlank Handlers ── */

/*
 * m4aSoundVSyncSetup: set up VBlank-synchronized sound processing.
 * Configures the VBlank interrupt to call the sound update routine
 * each frame, ensuring audio stays in sync with display refresh.
 *   23 lines, calls MPlayChannelReset (FUN_0804ff08)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050134);
/*
 * SappyStateCheck: verify the Sappy engine is properly initialized.
 * Checks for SAPPY_MAGIC (0x68736D53 = "Smsh" in little-endian)
 * at the engine state address. Returns early if not initialized.
 *   45 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050162);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050172);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0805018a);
/*
 * SoundEffectTrigger: trigger a sound effect through the SE player.
 * Uses gMPlayInfo_SE as the MusicPlayer context for SFX playback.
 *   37 lines, calls PlaySoundWithContext_DC
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080501ba);

/* ── Sound Hardware Initialization ── */

/*
 * SoundHardwareInit: CRITICAL — initialize all GBA sound hardware registers.
 * Writes to 14 hardware registers to configure the sound system:
 * REG_SOUNDCNT_L/H/X (master volume, channel enable, master on/off),
 * REG_FIFO_A/B (Direct Sound FIFO), DMA1/2 (sample streaming),
 * and various channel control registers.
 *   126 lines
 *   HW: 0x04000060, 0x04000063, 0x04000080, 0x04000084,
 *       0x040000A0, 0x040000A4, 0x040000BC, 0x040000C4,
 *       0x040000C6, 0x040000D0
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050200);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0805031a);
/*
 * Plays a sound effect using the BGM MusicPlayer context (gMPlayInfo_BGM).
 * Used for music-priority sounds that share the BGM mixer.
 *   r0: sound effect ID
 *   no return value
 */
void PlaySoundWithContext_D8(u32 r0) {
    FUN_0805186c(r0, gMPlayInfo_BGM);
}
/*
 * Plays a sound effect using the SE MusicPlayer context (gMPlayInfo_SE).
 * Used for standard sound effects with independent mixing.
 *   r0: sound effect ID
 *   no return value
 */
void PlaySoundWithContext_DC(u32 r0) {
    FUN_0805186c(r0, gMPlayInfo_SE);
}

/* ── Direct Sound & DMA Configuration ── */

/*
 * DirectSoundFifoSetup: configure Direct Sound FIFO and DMA channels.
 * Sets up FIFO_A and FIFO_B for PCM sample streaming, configures
 * DMA1/DMA2 for automatic sound data transfer on timer overflow.
 *   109 lines
 *   HW: REG_FIFO_A (0x040000A0), REG_FIFO_B (0x040000A4),
 *       DMA1/2 SAD/DAD/CNT, REG_SOUNDBIAS (0x04000089),
 *       REG_SOUNDCNT_X (0x04000084)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050344);
/*
 * SoundTimerSetup: configure timer for PCM sample rate.
 * Sets TM0/TM1 to generate interrupts at the mixing frequency,
 * which triggers DMA transfers to refill the FIFO buffers.
 *   72 lines, calls m4aSoundVSyncOn (EnableInterruptsAfterGfxSetup), FUN_080518a4
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0805043c);
/*
 * SoundSystemConfigure: configure sound system operating mode.
 * Sets reverb, mixing frequency, and channel allocation based
 * on the game's audio requirements.
 *   80 lines
 *   HW: REG_SOUNDBIAS (0x04000089)
 *   calls: SoundTimerSetup, m4aSoundShutdown (DisableInterruptsForGfxSetup)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080504e0);
/*
 * SoundPlatformDetect: detect audio platform capabilities.
 * Checks hardware version and adjusts sound parameters accordingly.
 *   45 lines, calls PlaySoundEffect (FUN_0805186c)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050578);
/*
 * m4aSoundShutdown: emergency stop — shut down all sound output.
 * Silences all channels, stops DMA, and resets hardware registers.
 * Called during fatal errors or system shutdown.
 *   59 lines, calls BitUnPack
 */
INCLUDE_ASM("asm/nonmatchings/m4a", DisableInterruptsForGfxSetup);

/* ── VBlank Sound Update Pipeline ── */

/*
 * m4aSoundVSyncOn: register the VBlank sound handler.
 * Installs the VBlank callback that drives the sound engine's
 * per-frame update cycle. Must be called after SoundHardwareInit.
 *   30 lines, leaf function
 *   refs: REG_SOUNDCNT_H (0x040000C6)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", EnableInterruptsAfterGfxSetup);
/*
 * VBlankSoundCallback: VBlank-triggered sound update routine.
 * Called by the VBlank interrupt handler to process pending
 * sound events and trigger the next DMA cycle.
 *   63 lines, calls PlaySoundWithContext_DC
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050684);
/*
 * MPlayOpen: load and open music player data from ROM.
 * Reads song header from ROM, allocates channels, loads instrument
 * data, and prepares the MusicPlayer for playback.
 *   121 lines
 *   calls: SoundContextRef (FUN_0804fb9c), SoundSystemConfigure (FUN_080504e0)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080506fc);
/*
 * MPlayChannelUpdate: update a single music player channel.
 * Processes pending notes, advances timing, and updates the
 * channel's hardware registers for the current frame.
 *   35 lines, calls SoundContextRef (FUN_0804fb9c)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080507e0);

/* ── Volume, Pitch & CGB Sound Control ── */

/*
 * CgbModVol: CGB channel volume modulation.
 * Applies volume envelope and modulation effects to CGB
 * channels (Square 1/2, Wave, Noise).
 *   110 lines, calls SoundContextRef (FUN_0804fb9c)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050820);
/*
 * CgbLookupTable: CGB frequency and volume lookup data.
 * Contains precomputed tables for CGB channel pitch and
 * volume conversion. Used by MidiKeyToCgbFreq and CgbSound.
 *   99 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080508e8);
/*
 * MidiKeyToCgbFreq: convert MIDI note number to CGB frequency register value.
 * Translates standard MIDI key numbers (0-127) into the frequency
 * register values needed by GBA CGB sound channels 1-4.
 *   133 lines, leaf function
 *   HW: REG_SOUND1CNT_X (0x04000063), REG_SOUND2CNT_H (0x04000069),
 *       REG_SOUND3CNT_X (0x04000070), REG_SOUND4CNT_H (0x04000079)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0805099e);
/*
 * CgbLookupUtil: CGB utility lookup for pitch/volume tables.
 *   59 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050a94);
/*
 * CgbSound: CGB channel hardware control — per-channel register writes.
 * Configures all 4 CGB sound channels (Square1, Square2, Wave, Noise)
 * by writing to their individual control registers. Handles duty cycle,
 * envelope, frequency, sweep, and wave pattern RAM.
 *   189 lines, calls MidiKeyToCgbFreq (FUN_0805099e)
 *   HW: REG_SOUND1CNT_L/H/X (0x04000060-0x04000064),
 *       REG_SOUND2CNT_L/H (0x04000068-0x0400006C),
 *       REG_SOUND3CNT_L/H/X (0x04000070-0x04000074),
 *       REG_SOUND4CNT_L/H (0x04000078-0x0400007C),
 *       REG_WAVE_RAM0 (0x04000090)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050afc);

/* ── Core Mixer / Channel Processing Loop ── */

/*
 * SoundMixerMain: CORE — process all mixer channels (2nd largest, 393 lines).
 * The main PCM mixing loop that combines all active Direct Sound channels
 * into the output buffer. Handles sample interpolation, volume mixing,
 * and stereo panning. Output is fed to FIFO via DMA.
 *   393 lines
 *   HW: REG_SOUNDCNT_H (0x04000081), REG_SOUNDBIAS (0x04000089)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050c70);

/* ── State Machine & Sappy Verification ── */

/*
 * SoundStateCheck1: verify Sappy engine state (variant 1).
 * Checks SAPPY_MAGIC (0x68736D53) to confirm the engine is live.
 *   22 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050f4a);
/*
 * SoundStateCheck2: verify Sappy engine state (variant 2).
 *   57 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050f70);
/*
 * SoundStateCheck3: verify Sappy engine state (variant 3).
 *   65 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050fd8);

/* ── MIDI Note & Command Encoding ── */

/*
 * MidiNoteLookup: MIDI note number to frequency lookup.
 * Converts MIDI note values (0-127) to internal pitch representation
 * using precomputed ROM tables.
 *   57 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0805104c);
/*
 * MidiUtilConvert: MIDI utility value converter.
 * Converts between MIDI controller values and internal representation.
 *   20 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080510b4);
/*
 * MidiCommandEncode1: encode MIDI command (type 1).
 * Packs MIDI event data into the internal command format.
 *   64 lines, calls MidiUtilConvert (FUN_080510b4)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080510d4);
/*
 * MidiCommandEncode2: encode MIDI command (type 2).
 * Packs MIDI controller/pitch data into the internal command format.
 *   62 lines, calls MidiUtilConvert (FUN_080510b4)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08051148);
/*
 * MPlayCommandDispatch: music player command dispatcher.
 * Reads a command byte from the track stream and dispatches to the
 * appropriate handler via a ROM-based function pointer table.
 *   174 lines
 *   calls: DispatchSoundCommand (FUN_08051870)
 *   refs: gSoundCmdTablePtr (0x03006454), ROM command table (0x080511F0)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080511bc);
/*
 * SoundCommandHandler: dispatch a sound command from ROM_SOUND_CMD_TABLE.
 * Reads a command byte, indexes into the command table at 0x08117C8C,
 * and calls the corresponding handler function.
 *   r0: sound context, r1: channel struct
 *   16 lines, calls DispatchSoundCommand (FUN_08051870)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08051314);
/*
 * Dispatches a sound command through DispatchSoundCommand (FUN_08051870)
 * using the global sound table pointer.
 *   r0, r1: command arguments passed through
 *   no return value
 */
void SoundCommand_6450(u32 r0, u32 r1) {
    FUN_08051870(r0, r1, gSoundTablePtr);
}
/*
 * BitMaskLUT: 32-bit channel bitmask lookup table.
 * Contains precomputed masks (0x00FFFFFF, 0xFF00FFFF, 0xFFFF00FF,
 * 0xFFFFFF00) for isolating individual byte lanes in 32-bit channel
 * state words. Used by the mixer to update per-channel volumes.
 *   118 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08051348);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08051392);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080513a6);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080513ba);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080513ce);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080513e2);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080513f6);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08051402);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0805140e);
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08051422);
