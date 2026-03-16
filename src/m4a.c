#include "global.h"
#include "gba.h"
#include "globals.h"
#include "include_asm.h"

/* ── Sound System Initialization & Shutdown ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804eb64); /* SoundMain — main sound system setup, calls stream/gfx init */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804ed68); /* SoundDmaInit — DMA controller setup for sound transfers */
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
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804ee34); /* SoundReset — minimal state reset */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804ee60); /* DmaControllerInit — full DMA init with channel setup */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804ef50); /* SoundInfoInit — initialize SoundInfo struct fields */

/* ── Sound Data & Buffer Management ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804efde); /* SoundBufferAlloc — allocate sound mixing buffers */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f004); /* SoundContextInit — setup sound context / mixer state */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f0d0); /* SoundChannelTableInit — initialize channel table entries */

/* ── MIDI / Music Sequence Processing ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f180); /* MidiReadUnaligned — read unaligned MIDI value */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f1c4); /* MidiProcessEvent — dispatch MIDI note/control event */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f248); /* MPlayMain — CORE: main music player tick (587 lines, DMA) */

/* ── Voice / Instrument Utilities ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f6d4); /* VoiceUtil — minimal voice utility */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f6f4); /* VoiceLookup — voice lookup wrapper */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f724); /* InstrumentLookup — ROM instrument table lookup */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f73c); /* InstrumentGetEntry — get instrument from ROM_INSTRUMENT_TABLE */

/* ── Sound Channel Control ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f758); /* MidiDecodeByte — decode single MIDI byte */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f766); /* MidiNoteSetup — setup MIDI note on channel */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f7b4); /* MidiCommandHandler — MIDI command dispatch (writes SOUND1CNT) */

/* ── Music Playback Engine ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804f944); /* MPlayContinue — music playback continuation (327 lines) */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fb9c); /* SoundContextRef — get sound context reference */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fbe0); /* MPlayStop_Channel — stop single music channel */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fc10); /* MPlayStop — stop all music playback (313 lines) */

/* ── Sound Effect Processing ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fe50); /* SoundEffectUtil — sound effect utility */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fe6c); /* SoundEffectProcess — process sound effect chain */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fea0); /* FreqTableLookup — frequency lookup from ROM pitch tables */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804ff08); /* MPlayChannelReset — reset channel state (Sappy magic check) */

/* ── Sound Init Dispatcher & Track Control ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804ff44); /* m4aSoundInit_Impl — full sound system init dispatcher */
/*
 * Wrapper that calls FUN_0804f294 to initialize the sound engine.
 *   no parameters
 *   no return value
 */
void SoundInit(void) {
    FUN_0804f294();
}
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804ffc8); /* m4aSongNumStart — start playing music track by ID */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0804fff6); /* m4aSongNumContinue — continue/queue music track */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050042); /* m4aSongNumLoad — load music data from ROM */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050094); /* m4aMPlayCommand — execute music player command */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080500c8); /* m4aSongNumStop — stop current music track */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080500fc); /* m4aSoundVSync — VBlank: update all sound channels (×4 loop) */
/*
 * Wrapper that calls FUN_0804ff08 to stop/reset a sound channel.
 *   r0: pointer to sound channel struct
 *   no return value
 */
void StopSoundChannel(u32 r0) {
    FUN_0804ff08(r0);
}

/* ── Interrupt & VBlank Handlers ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050134); /* m4aSoundVSyncSetup — setup VBlank sound sync */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050162); /* SappyStateCheck — verify Sappy magic marker (0x68736D53) */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080501ba); /* SoundEffectTrigger — trigger sound effect via gMPlayInfo_SE */

/* ── Sound Hardware Initialization ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050200); /* SoundHardwareInit — CRITICAL: init all GBA sound regs (14 HW regs) */
/*
 * Calls FUN_0805186c (PlaySoundEffect) with the given parameter
 * and the BGM music player context as the second argument.
 *   r0: sound effect ID
 *   no return value
 */
void PlaySoundWithContext_D8(u32 r0) {
    FUN_0805186c(r0, gMPlayInfo_BGM);
}
/*
 * Calls FUN_0805186c (PlaySoundEffect) with the given parameter
 * and the SE music player context as the second argument.
 *   r0: sound effect ID
 *   no return value
 */
void PlaySoundWithContext_DC(u32 r0) {
    FUN_0805186c(r0, gMPlayInfo_SE);
}

/* ── Direct Sound & DMA Configuration ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050344); /* DirectSoundFifoSetup — FIFO_A/B and DMA config (8 HW regs) */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0805043c); /* SoundTimerSetup — configure timer for sample rate */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080504e0); /* SoundSystemConfigure — configure sound system mode */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050578); /* SoundPlatformDetect — detect platform/capabilities */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080505cc); /* m4aSoundShutdown — emergency stop all sound */

/* ── VBlank Sound Update Pipeline ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050648); /* m4aSoundVSyncOn — register VBlank sound handler */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050684); /* VBlankSoundCallback — VBlank-triggered sound update */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080506fc); /* MPlayOpen — load & open music player data from ROM */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080507e0); /* MPlayChannelUpdate — update single music channel */

/* ── Volume, Pitch & CGB Sound Control ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050820); /* CgbModVol — CGB channel volume modulation */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080508e8); /* CgbLookupTable — CGB frequency/volume lookup data */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0805099e); /* MidiKeyToCgbFreq — MIDI note → CGB frequency (4 pitch regs) */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050a94); /* CgbLookupUtil — CGB utility lookup */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050afc); /* CgbSound — CGB channel hardware control (14 HW regs) */

/* ── Core Mixer / Channel Processing Loop ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050c70); /* SoundMixerMain — CORE: process all mixer channels (393 lines) */

/* ── State Machine & Sappy Verification ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050f4a); /* SoundStateCheck1 — Sappy magic state check */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050f70); /* SoundStateCheck2 — Sappy magic state check */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08050fd8); /* SoundStateCheck3 — Sappy magic state check */

/* ── MIDI Note & Command Encoding ── */

INCLUDE_ASM("asm/nonmatchings/m4a", FUN_0805104c); /* MidiNoteLookup — MIDI note to frequency lookup */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080510b4); /* MidiUtilConvert — MIDI utility converter */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080510d4); /* MidiCommandEncode1 — encode MIDI command type 1 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08051148); /* MidiCommandEncode2 — encode MIDI command type 2 */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_080511bc); /* MPlayCommandDispatch — music command dispatcher (ROM table) */
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08051314); /* SoundCommandHandler — command dispatch from ROM_SOUND_CMD_TABLE */
/*
 * Dispatches a sound command through FUN_08051870 (DispatchSoundCommand)
 * using the global sound table pointer.
 *   r0, r1: command arguments passed through
 *   no return value
 */
void SoundCommand_6450(u32 r0, u32 r1) {
    FUN_08051870(r0, r1, gSoundTablePtr);
}
INCLUDE_ASM("asm/nonmatchings/m4a", FUN_08051348); /* BitMaskLUT — 32-bit channel bitmask lookup table */
