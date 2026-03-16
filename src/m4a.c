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
 *   248 lines, calls UpdateCursorBlink/ProcessScreenFade/UpdatePaletteFadeStep
 *   refs: gSoundInfo (0x0300081C), gGfxBufferPtr (0x030034A0)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SoundMain);
/*
 * SoundDmaInit: DMA controller setup for sound data transfers.
 * Configures DMA channels for PCM sample streaming to FIFO.
 *   79 lines, has DMA register writes (REG_DMA3SAD/DAD/CNT)
 *   calls: thunk_FUN_080001e0 (memory alloc)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SoundDmaInit);
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
INCLUDE_ASM("asm/nonmatchings/m4a", SoundReset);
/*
 * DmaControllerInit: full DMA initialization with channel config.
 * Sets up DMA for both Direct Sound channels (FIFO A and B).
 *   125 lines, has DMA register writes
 *   calls: SoundReset (SoundReset)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", DmaControllerInit);
/*
 * SoundInfoInit: initialize SoundInfo struct fields.
 * Fills in default values for the sound info structure at gSoundInfo.
 *   73 lines, leaf function
 *   refs: gSoundInfo (0x0300081C), gStreamPtr (0x03004D84)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SoundInfoInit);
INCLUDE_ASM("asm/nonmatchings/m4a", StreamCmd_GetStreamPtr);
INCLUDE_ASM("asm/nonmatchings/m4a", StreamCmd_ValidateStream);

/* ── Sound Data & Buffer Management ── */

/*
 * SoundBufferAlloc: allocate sound mixing buffers.
 * Allocates WRAM buffers for the PCM mixing pipeline.
 *   21 lines, calls FUN_08051868
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SoundBufferAlloc);
/*
 * SoundContextInit: setup sound context and mixer state.
 * Initializes the mixer configuration, channel assignments, and
 * links the sound context to the global control structures.
 *   102 lines, leaf function
 *   refs: gSoundInfo, gStreamPtr, gControlBlock (0x03004C20)
 */
/**
 * StreamCmd_SetSoundReverb: sets gSoundInfo reverb from stream nibble.
 * (shares literal pool with StreamCmd_GetSoundReverb — can't decompile independently)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", StreamCmd_SetSoundReverb);
INCLUDE_ASM("asm/nonmatchings/m4a", StreamCmd_GetSoundReverb);
INCLUDE_ASM("asm/nonmatchings/m4a", SoundContextInit);
/*
 * SoundChannelTableInit: initialize sound channel table entries.
 * Sets up the per-channel state for all active sound channels.
 *   86 lines, calls FUN_08051868
 */
/**
 * StreamCmd_StopSoundAndSync: stops sound, syncs freq to BG scroll, advances by 2.
 */
INCLUDE_ASM("asm/nonmatchings/m4a", StreamCmd_StopSoundAndSync);
INCLUDE_ASM("asm/nonmatchings/m4a", StreamCmd_StopAndAdvance);
INCLUDE_ASM("asm/nonmatchings/m4a", StreamCmd_ReadFreqParam);
INCLUDE_ASM("asm/nonmatchings/m4a", StreamCmd_SetChannelMode);

/* ── MIDI / Music Sequence Processing ── */

/*
 * MidiReadUnaligned: read a potentially unaligned MIDI value.
 * Used to parse variable-length MIDI data from track bytecode.
 *   32 lines, calls ReadUnalignedU16
 */
/**
 * StreamCmd_SetSoundFreqs: sets two sound frequency values from stream data.
 *
 * Reads u16 from stream bytes[2-3] -> gSoundInfo[0x10],
 * reads u16 from stream bytes[4-5] -> gSoundInfo[0x12].
 * Advances stream by 6.
 */
INCLUDE_ASM("asm/nonmatchings/m4a", StreamCmd_SetSoundFreqs);
INCLUDE_ASM("asm/nonmatchings/m4a", StreamCmd_AdvanceStream);
/*
 * MidiProcessEvent: dispatch a MIDI note or control event.
 * Processes a single event from the track bytecode stream,
 * handling note-on, note-off, and control change messages.
 *   72 lines, calls DmaControllerInit (DmaControllerInit)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MidiProcessEvent);
/*
 * MPlayMain_SetAndProcess: stores a value into gSoundInfo[0xD]
 * then calls MidiProcessEvent (MidiProcessEvent).
 *   r0: value to store
 *   10 lines (split from former 587-line MPlayMain)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayMain_SetAndProcess);
/*
 * MPlayMain: CORE music player tick — largest function in m4a.
 * Called each frame to advance all active music tracks. Reads track bytecode,
 * processes MIDI commands, manages DMA transfers for PCM playback, and
 * updates channel state.
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayMain);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayTrackCallback);

/* ── Voice / Instrument Utilities ── */

/*
 * VoiceUtil: minimal voice utility function.
 *   22 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", VoiceGetParams);
/*
 * VoiceLookup: voice parameter lookup wrapper.
 *   28 lines, calls VoiceUtil (VoiceGetParams)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", VoiceLookupAndApply);
/*
 * InstrumentLookup: look up instrument data from ROM_INSTRUMENT_TABLE.
 * Given a program/voice number, returns a pointer to the instrument entry
 * (12-byte voice struct: type, key, samplePtr, ADSR).
 *   14 lines, calls InstrumentGetEntry (FUN_0804f73e)
 *   refs: ROM_INSTRUMENT_TABLE (0x081179E4)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", InstrumentLookup);
/*
 * InstrumentGetEntry: get a specific instrument entry from ROM.
 * Indexes into the voice/instrument table and returns the entry data.
 *   20 lines, leaf function
 *   refs: ROM_INSTRUMENT_TABLE (0x081179E4)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", InstrumentGetEntry);

/* ── Sound Channel Control ── */

/*
 * MidiDecodeByte: decode a single MIDI byte from the track stream.
 * Reads one byte, advances the stream position, and returns the value.
 *   11 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MidiDecodeByte);
/*
 * MidiNoteSetup: set up a MIDI note-on event on a sound channel.
 * Configures the channel's pitch, volume, and instrument for playback.
 *   42 lines, calls InstrumentGetEntry (InstrumentGetEntry)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MidiNoteSetup);
INCLUDE_ASM("asm/nonmatchings/m4a", MidiNoteWithVelocity);
/*
 * MidiCommandHandler: MIDI command dispatch table handler.
 * Processes track bytecode commands (0xB1-0xCF): tempo, voice select,
 * volume, pan, pitch bend, loops, etc. Writes to REG_SOUND1CNT_L
 * for CGB channel control.
 *   192 lines, writes to REG_SOUND1CNT_L (0x04000060), REG_DMA1SAD (0x040000BC)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MidiCommandHandler);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayTrackReset);

/* ── Music Playback Engine ── */

/*
 * MPlayContinue: continue music playback for all active tracks.
 * Processes pending MIDI events, updates note timing, manages
 * voice allocation, and coordinates multi-track playback.
 *   327 lines
 *   calls: PlaySoundWithContext_D8/DC, CgbModVol
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayContinue);
/*
 * SoundContextRef: get a reference to the current sound context.
 * Returns the active MusicPlayer context for the caller.
 *   39 lines, calls FUN_0804fb8c
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SoundContextRef);
/*
 * MPlayStop_Channel: stop playback on a single music channel.
 * Silences one channel without affecting other active tracks.
 *   28 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayStop_Channel);
/*
 * MPlayStop: stop all music playback across all tracks.
 * Silences all channels, resets playback state, and releases
 * voice allocations. Third largest function in m4a (313 lines).
 *   313 lines
 *   calls: PlaySoundWithContext_D8 (PlaySoundWithContext_D8)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayNoteProcess);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayChannelRelease);

/* ── Sound Effect Processing ── */

/*
 * SoundEffectUtil: sound effect utility / parameter setup.
 *   18 lines, leaf function
 */
/**
 * SoundEffectParamInit: initializes sound effect channel parameters.
 * Clears volume and pan bytes, sets channel flags based on existing state.
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SoundEffectParamInit);
/*
 * SoundEffectProcess: process a sound effect playback chain.
 * Handles SFX queuing, priority, and channel assignment.
 *   24 lines, calls SoundEffectUtil (SoundEffectParamInit)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SoundEffectChain);
/*
 * FreqTableLookup: look up pitch/frequency from ROM tables.
 * Converts MIDI note numbers to hardware frequency values using
 * ROM_FREQ_TABLE_1 (0x08117A74) and ROM_FREQ_TABLE_2 (0x08117B28).
 *   51 lines, calls FUN_0804f284
 */
INCLUDE_ASM("asm/nonmatchings/m4a", FreqTableLookup);
/*
 * MPlayChannelReset: reset a music player channel to idle state.
 * Checks the Sappy magic marker (SAPPY_MAGIC = 0x68736D53 = "Smsh")
 * to verify the sound engine is initialized before resetting.
 *   33 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayChannelReset);

/* ── Sound Init Dispatcher & Track Control ── */

/*
 * m4aSoundInit_Impl: full sound system initialization dispatcher.
 * Calls BitUnPack for data decompression, sets up sound channels,
 * and prepares the music player for first use.
 *   50 lines
 *   calls: BitUnPack, SoundHardwareInit, DirectSoundFifoSetup
 *   refs: ROM_MUSIC_TABLE (0x08118AB4)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", m4aSoundInit_Impl);
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
 *   21 lines, calls LoadSoundData (MPlayLoadSongData)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", InitSceneState);
/*
 * m4aSongNumContinue: continue or queue a music track.
 * If the same song is already playing, does nothing.
 * Otherwise queues the song for the next VBlank cycle.
 *   38 lines, refs: ROM_MUSIC_TABLE, ROM_MUSIC_META_TABLE
 */
INCLUDE_ASM("asm/nonmatchings/m4a", m4aSongNumContinue);
/*
 * m4aSongNumLoad: load music data from ROM into WRAM.
 * Copies track headers and sets up the MusicPlayer state for
 * a new song without starting playback.
 *   41 lines
 *   calls: MPlayChannelReset (MPlayChannelReset), LoadSoundData (MPlayLoadSongData)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", m4aSongNumLoad);
/*
 * m4aMPlayCommand: execute a music player command.
 * Processes high-level commands like fade, tempo change, etc.
 *   r0: command ID
 *   26 lines, refs: ROM_MUSIC_TABLE, ROM_MUSIC_META_TABLE
 */
INCLUDE_ASM("asm/nonmatchings/m4a", m4aMPlayCommand);
/*
 * m4aSongNumStop: stop the currently playing music track.
 * Halts playback and releases all voices for the active song.
 *   26 lines, calls MPlayChannelReset (MPlayChannelReset)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", m4aSongNumStop);
/*
 * m4aSoundVSync: VBlank sound update — called every frame.
 * Loops over 4 sound channels, calling MPlayChannelUpdate for each.
 * This is the main per-frame entry point for the sound engine.
 *   23 lines, refs: ROM_MUSIC_TABLE (0x08118AB4)
 *   calls: MPlayChannelUpdate (MPlayStop)
 */
/**
 * StopAllMusicPlayers: stops all 4 music player instances.
 * Iterates ROM_MUSIC_TABLE (0x08118AB4), calling MPlayStop on each.
 */
INCLUDE_ASM("asm/nonmatchings/m4a", StopAllMusicPlayers);
/*
 * Wrapper that calls MPlayChannelReset (MPlayChannelReset) to stop/reset
 * a single sound channel.
 *   r0: pointer to sound channel struct
 *   no return value
 */
void StopSoundChannel(u32 r0) {
    MPlayChannelReset(r0);
}

/* ── Interrupt & VBlank Handlers ── */

/*
 * m4aSoundVSyncSetup: set up VBlank-synchronized sound processing.
 * Configures the VBlank interrupt to call the sound update routine
 * each frame, ensuring audio stays in sync with display refresh.
 *   23 lines, calls MPlayChannelReset (MPlayChannelReset)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", StopSoundEffects);
/*
 * SappyStateCheck: verify the Sappy engine is properly initialized.
 * Checks for SAPPY_MAGIC (0x68736D53 = "Smsh" in little-endian)
 * at the engine state address. Returns early if not initialized.
 *   45 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SappyStateVerify);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayTrackVerify);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayTrackFadeAndVerify);
/*
 * SoundEffectTrigger: trigger a sound effect through the SE player.
 * Uses gMPlayInfo_SE as the MusicPlayer context for SFX playback.
 *   37 lines, calls PlaySoundWithContext_DC
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SoundEffectTrigger);

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
INCLUDE_ASM("asm/nonmatchings/m4a", SoundHardwareInit);
INCLUDE_ASM("asm/nonmatchings/m4a", SoundHardwareInit_Tail);
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
INCLUDE_ASM("asm/nonmatchings/m4a", DirectSoundFifoSetup);
/*
 * SoundTimerSetup: configure timer for PCM sample rate.
 * Sets TM0/TM1 to generate interrupts at the mixing frequency,
 * which triggers DMA transfers to refill the FIFO buffers.
 *   72 lines, calls m4aSoundVSyncOn (EnableInterruptsAfterGfxSetup), FUN_080518a4
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SoundTimerSetup);
/*
 * SoundSystemConfigure: configure sound system operating mode.
 * Sets reverb, mixing frequency, and channel allocation based
 * on the game's audio requirements.
 *   80 lines
 *   HW: REG_SOUNDBIAS (0x04000089)
 *   calls: SoundTimerSetup, m4aSoundShutdown (DisableInterruptsForGfxSetup)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SoundSystemConfigure);
/*
 * SoundPlatformDetect: detect audio platform capabilities.
 * Checks hardware version and adjusts sound parameters accordingly.
 *   45 lines, calls PlaySoundEffect (FUN_0805186c)
 */
/**
 * SoundChannelResetAll: resets all channel status bytes and processes channels.
 *
 * Checks SAPPY_MAGIC, clears 12 channel status bytes (stride 0x40),
 * then calls FUN_0805186c for channels 1-4 with the voice table.
 * Restores SAPPY_MAGIC on exit.
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SoundChannelResetAll);
/*
 * m4aSoundShutdown: emergency stop — shut down all sound output.
 * Silences all channels, stops DMA, and resets hardware registers.
 * Called during fatal errors or system shutdown.
 *   59 lines, calls BitUnPack
 */
/**
 * m4aSoundShutdown: emergency shutdown of the sound system.
 *
 * Checks magic offset (0x978C92AD + magic <= 1 means active).
 * Increments magic by 10 to lock, stops DMA1/DMA2 if active,
 * sets DMA control to 0x0400 mode, then calls BitUnPack to
 * clear the channel state array.
 */
void DisableInterruptsForGfxSetup(void)
{
    u32 scratch;
    u32 *info = *(u32 **)0x03007FF0;
    u32 magic = info[0];

    if (magic + 0x978C92AD > 1)
        return;

    info[0] = magic + 10;

    {
        vu32 *dma1 = (vu32 *)0x040000C4;
        if (*dma1 & 0x02000000) {
            *dma1 = 0x84400004;
        }
    }

    {
        vu32 *dma2 = (vu32 *)0x040000D0;
        if (*dma2 & 0x02000000) {
            *dma2 = 0x84400004;
        }
    }

    *(vu16 *)0x040000C6 = 0x0400;
    *(vu16 *)0x040000D2 = 0x0400;

    scratch = 0;
    BitUnPack(&scratch, (u8 *)info + 0x350, (u32 *)0x05000318);
}

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
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayOpen);
/*
 * MPlayOpen: load and open music player data from ROM.
 * Reads song header from ROM, allocates channels, loads instrument
 * data, and prepares the MusicPlayer for playback.
 *   121 lines
 *   calls: SoundContextRef (SoundContextRef), SoundSystemConfigure (SoundSystemConfigure)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayLoadSongData);
/*
 * MPlayChannelUpdate: update a single music player channel.
 * Processes pending notes, advances timing, and updates the
 * channel's hardware registers for the current frame.
 *   35 lines, calls SoundContextRef (SoundContextRef)
 */
/**
 * MPlayStop: stops all tracks in a music player.
 * Checks Sappy magic, locks engine, sets stop flag, iterates
 * all tracks calling SoundContextRef, then restores magic.
 */
void MPlayStop(u32 *player)
{
    u32 magic = player[0x34 / 4];

    if (magic != 0x68736D53)
        return;

    player[0x34 / 4] = magic + 1;
    player[0x04 / 4] |= 0x80000000;

    {
        s32 numTracks = (s32)(u8)((u8 *)player)[0x08];
        u8 *track = (u8 *)player[0x2C / 4];

        while (numTracks > 0) {
            SoundContextRef((u32)player, (u32)track);
            numTracks--;
            track += 0x50;
        }
    }

    player[0x34 / 4] = 0x68736D53;
}

/* ── Volume, Pitch & CGB Sound Control ── */

/*
 * CgbModVol: CGB channel volume modulation.
 * Applies volume envelope and modulation effects to CGB
 * channels (Square 1/2, Wave, Noise).
 *   110 lines, calls SoundContextRef (SoundContextRef)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", CgbModVol);
/*
 * CgbLookupTable: CGB frequency and volume lookup data.
 * Contains precomputed tables for CGB channel pitch and
 * volume conversion. Used by MidiKeyToCgbFreq and CgbSound.
 *   99 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", CgbChannelMix);
/*
 * MidiKeyToCgbFreq: convert MIDI note number to CGB frequency register value.
 * Translates standard MIDI key numbers (0-127) into the frequency
 * register values needed by GBA CGB sound channels 1-4.
 *   133 lines, leaf function
 *   HW: REG_SOUND1CNT_X (0x04000063), REG_SOUND2CNT_H (0x04000069),
 *       REG_SOUND3CNT_X (0x04000070), REG_SOUND4CNT_H (0x04000079)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MidiKeyToCgbFreq);
/*
 * CgbLookupUtil: CGB utility lookup for pitch/volume tables.
 *   59 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", CgbPanLookup);
/*
 * CgbSound: CGB channel hardware control — per-channel register writes.
 * Configures all 4 CGB sound channels (Square1, Square2, Wave, Noise)
 * by writing to their individual control registers. Handles duty cycle,
 * envelope, frequency, sweep, and wave pattern RAM.
 *   189 lines, calls MidiKeyToCgbFreq (MidiKeyToCgbFreq)
 *   HW: REG_SOUND1CNT_L/H/X (0x04000060-0x04000064),
 *       REG_SOUND2CNT_L/H (0x04000068-0x0400006C),
 *       REG_SOUND3CNT_L/H/X (0x04000070-0x04000074),
 *       REG_SOUND4CNT_L/H (0x04000078-0x0400007C),
 *       REG_WAVE_RAM0 (0x04000090)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", CgbSound);

/* ── Core Mixer / Channel Processing Loop ── */

/*
 * SoundMixerMain: CORE — process all mixer channels (2nd largest, 393 lines).
 * The main PCM mixing loop that combines all active Direct Sound channels
 * into the output buffer. Handles sample interpolation, volume mixing,
 * and stereo panning. Output is fed to FIFO via DMA.
 *   393 lines
 *   HW: REG_SOUNDCNT_H (0x04000081), REG_SOUNDBIAS (0x04000089)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SoundMixerMain);

/* ── State Machine & Sappy Verification ── */

/*
 * SoundStateCheck1: verify Sappy engine state (variant 1).
 * Checks SAPPY_MAGIC (0x68736D53) to confirm the engine is live.
 *   22 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayStateCheck1);
/*
 * SoundStateCheck2: verify Sappy engine state (variant 2).
 *   57 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayStateCheck);
/*
 * SoundStateCheck3: verify Sappy engine state (variant 3).
 *   65 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayStateCheck3);

/* ── MIDI Note & Command Encoding ── */

/*
 * MidiNoteLookup: MIDI note number to frequency lookup.
 * Converts MIDI note values (0-127) to internal pitch representation
 * using precomputed ROM tables.
 *   57 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MidiNoteLookup);
/*
 * MidiUtilConvert: MIDI utility value converter.
 * Converts between MIDI controller values and internal representation.
 *   20 lines, leaf function
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MidiUtilConvert);
/*
 * MidiCommandEncode1: encode MIDI command (type 1).
 * Packs MIDI event data into the internal command format.
 *   64 lines, calls MidiUtilConvert (MidiUtilConvert)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MidiCommandEncode1);
/*
 * MidiCommandEncode2: encode MIDI command (type 2).
 * Packs MIDI controller/pitch data into the internal command format.
 *   62 lines, calls MidiUtilConvert (MidiUtilConvert)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MidiCommandEncode2);
/*
 * MPlayCommandDispatch: music player command dispatcher.
 * Reads a command byte from the track stream and dispatches to the
 * appropriate handler via a ROM-based function pointer table.
 *   174 lines
 *   calls: DispatchSoundCommand (FUN_08051870)
 *   refs: gSoundCmdTablePtr (0x03006454), ROM command table (0x080511F0)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayCommandDispatch);
/*
 * SoundCommandHandler: dispatch a sound command from ROM_SOUND_CMD_TABLE.
 * Reads a command byte, indexes into the command table at 0x08117C8C,
 * and calls the corresponding handler function.
 *   r0: sound context, r1: channel struct
 *   16 lines, calls DispatchSoundCommand (FUN_08051870)
 */
INCLUDE_ASM("asm/nonmatchings/m4a", SoundCmd_Dispatch);
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
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayCmd_ReadU32Param);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayCmd_KeyShift);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayCmd_SetVoice);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayCmd_SetVolume);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayCmd_SetPan);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayCmd_SetBend);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayCmd_SetBendRange);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayCmd_SetLFOSpeed);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayCmd_SetLFODelay);
INCLUDE_ASM("asm/nonmatchings/m4a", MPlayCmd_SetModDepth);
