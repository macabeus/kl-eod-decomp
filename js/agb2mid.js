/**
 * M4A (GBA MusicPlayer2000 / Sappy) to Standard MIDI File converter.
 * Produces SMF Format 0, 24 ticks per quarter note.
 */

import { readU32, readU8, SONG_TABLE } from './rom-tables.js';

// Duration lookup table (49 entries, indices 0-48)
const LEN_TBL = [
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 28, 30, 32, 36, 40, 42, 44,
    48, 52, 54, 56, 60, 64, 66, 68, 72, 76, 78, 80, 84, 88, 90, 92, 96,
];

const TICKS_PER_BEAT = 24;

/** Write a MIDI variable-length quantity into an array. */
function writeVLQ(arr, value) {
    if (value < 0) value = 0;
    const bytes = [];
    bytes.push(value & 0x7F);
    value >>>= 7;
    while (value > 0) {
        bytes.push((value & 0x7F) | 0x80);
        value >>>= 7;
    }
    for (let i = bytes.length - 1; i >= 0; i--) arr.push(bytes[i]);
}

/** Write a big-endian 32-bit integer. */
function writeU32BE(arr, v) {
    arr.push((v >>> 24) & 0xFF, (v >>> 16) & 0xFF, (v >>> 8) & 0xFF, v & 0xFF);
}

/** Write a big-endian 16-bit integer. */
function writeU16BE(arr, v) {
    arr.push((v >>> 8) & 0xFF, v & 0xFF);
}

/**
 * Convert a song from the ROM to a Standard MIDI File (Uint8Array).
 * @param {DataView} dv  DataView over the ROM
 * @param {Uint8Array} rom  Uint8Array of the ROM
 * @param {number} songIndex  Song index (0-based into SONG_TABLE)
 * @returns {Uint8Array}  SMF bytes
 */
export function convertSong(dv, rom, songIndex) {
    // Read song pointer from table
    const songPtr = readU32(dv, SONG_TABLE + songIndex * 8);
    const songOff = songPtr - 0x08000000;

    // Song header
    const numTracks = rom[songOff];
    // const blockCount = rom[songOff + 1]; // unused for MIDI
    // const priority = dv.getUint16(songOff + 2, true); // unused
    // const voiceGroupPtr = readU32(dv, songOff + 4); // unused for MIDI

    // Read track pointers
    const trackPtrs = [];
    for (let t = 0; t < numTracks; t++) {
        const ptr = readU32(dv, songOff + 8 + t * 4);
        trackPtrs.push(ptr - 0x08000000);
    }

    // Parse all tracks, collecting timed MIDI events
    const events = []; // { tick, bytes[] }

    for (let ch = 0; ch < numTracks; ch++) {
        parseTrack(rom, dv, trackPtrs[ch], ch, events);
    }

    // Sort events by tick (stable: preserve insertion order for same tick)
    events.sort((a, b) => a.tick - b.tick);

    // Build MIDI track data
    const trackData = [];

    // Optional: Add a text marker
    const marker = 'Converted by kl-eod-decomp';
    trackData.push(0x00); // delta=0
    trackData.push(0xFF, 0x06); // text event
    writeVLQ(trackData, marker.length);
    for (let i = 0; i < marker.length; i++) trackData.push(marker.charCodeAt(i));

    // Write events as delta-time + MIDI bytes
    let lastTick = 0;
    for (const ev of events) {
        const delta = ev.tick - lastTick;
        writeVLQ(trackData, delta);
        for (const b of ev.bytes) trackData.push(b);
        lastTick = ev.tick;
    }

    // End of track
    writeVLQ(trackData, 0);
    trackData.push(0xFF, 0x2F, 0x00);

    // Build SMF
    const midi = [];
    // MThd
    midi.push(0x4D, 0x54, 0x68, 0x64); // 'MThd'
    writeU32BE(midi, 6);                 // header length
    writeU16BE(midi, 0);                 // format 0
    writeU16BE(midi, 1);                 // 1 track
    writeU16BE(midi, TICKS_PER_BEAT);    // division

    // MTrk
    midi.push(0x4D, 0x54, 0x72, 0x6B); // 'MTrk'
    writeU32BE(midi, trackData.length);
    for (const b of trackData) midi.push(b);

    return new Uint8Array(midi);
}

/**
 * Parse one m4a track and append timed MIDI events.
 */
function parseTrack(rom, dv, startOff, channel, events) {
    let pos = startOff;
    let tick = 0;
    let lastKey = 60;
    let lastVel = 127;
    let keyShift = 0;

    // Subroutine return address (single-level)
    let returnPos = -1;
    let returnFlag = false;

    // Active notes: Map<key, { offTick }>
    const activeNotes = new Map();

    // Set of visited GOTO targets to prevent infinite loops
    const visitedGotos = new Set();

    // Emit initial reverb CC (many m4a games use reverb)
    events.push({ tick: 0, bytes: [0xB0 | channel, 0x5B, 0x47] }); // CC 91 (reverb) = 71

    const maxIterations = 100000; // safety limit
    let iterations = 0;

    while (iterations++ < maxIterations) {
        if (pos >= rom.length) break;
        const cmd = rom[pos++];

        if (cmd <= 0x7F) {
            // Argument byte: repeat last repeatable command
            // In m4a, bytes < 0x80 when not following a command are skipped
            // (they're consumed as arguments by the preceding command)
            // This shouldn't happen at the command level, so skip
            continue;
        }

        if (cmd >= 0x80 && cmd <= 0xB0) {
            // Wait command
            const waitTicks = LEN_TBL[cmd - 0x80];
            tick += waitTicks;
            continue;
        }

        switch (cmd) {
            case 0xB1: // FINE - end of track
                // Turn off all active notes
                for (const [key, info] of activeNotes) {
                    events.push({ tick, bytes: [0x80 | channel, key, 0] });
                }
                activeNotes.clear();
                return;

            case 0xB2: { // GOTO - loop
                const ptr = readU32(dv, pos) - 0x08000000;
                pos += 4;
                if (visitedGotos.has(ptr)) {
                    // Already visited: end track to prevent infinite loop
                    for (const [key, info] of activeNotes) {
                        events.push({ tick, bytes: [0x80 | channel, key, 0] });
                    }
                    activeNotes.clear();
                    return;
                }
                visitedGotos.add(ptr);
                pos = ptr;
                continue;
            }

            case 0xB3: { // PATT - call subroutine
                const ptr = readU32(dv, pos) - 0x08000000;
                pos += 4;
                returnPos = pos;
                returnFlag = true;
                pos = ptr;
                continue;
            }

            case 0xB4: // PEND - return from subroutine
                if (returnFlag) {
                    pos = returnPos;
                    returnFlag = false;
                }
                continue;

            case 0xBB: { // TEMPO
                const tempoVal = rom[pos++];
                const bpm = tempoVal * 2;
                const usPerBeat = Math.round(60000000 / bpm);
                events.push({
                    tick,
                    bytes: [0xFF, 0x51, 0x03,
                            (usPerBeat >>> 16) & 0xFF,
                            (usPerBeat >>> 8) & 0xFF,
                            usPerBeat & 0xFF],
                });
                continue;
            }

            case 0xBC: { // KEYSH - key shift / transpose
                const val = rom[pos++];
                keyShift = (val > 127) ? val - 256 : val; // signed
                continue;
            }

            case 0xBD: { // VOICE - program change
                const prog = rom[pos++];
                events.push({ tick, bytes: [0xC0 | channel, prog & 0x7F] });
                continue;
            }

            case 0xBE: { // VOL - volume
                const vol = rom[pos++];
                events.push({ tick, bytes: [0xB0 | channel, 0x07, vol & 0x7F] });
                continue;
            }

            case 0xBF: { // PAN
                const pan = rom[pos++];
                events.push({ tick, bytes: [0xB0 | channel, 0x0A, pan & 0x7F] });
                continue;
            }

            case 0xC0: { // BEND
                const bend = rom[pos++];
                // Map 0x00-0x7F (centered at 0x40) to MIDI pitch bend (centered at 0x2000)
                const midiVal = Math.round(((bend - 0x40) / 0x40) * 0x2000 + 0x2000);
                const clamped = Math.max(0, Math.min(0x3FFF, midiVal));
                events.push({ tick, bytes: [0xE0 | channel, clamped & 0x7F, (clamped >>> 7) & 0x7F] });
                continue;
            }

            case 0xC1: { // BENDR - pitch bend range
                const range = rom[pos++];
                // RPN 0x0000 (pitch bend range)
                events.push({ tick, bytes: [0xB0 | channel, 0x65, 0x00] }); // RPN MSB
                events.push({ tick, bytes: [0xB0 | channel, 0x64, 0x00] }); // RPN LSB
                events.push({ tick, bytes: [0xB0 | channel, 0x06, range & 0x7F] }); // Data Entry
                continue;
            }

            case 0xC2: { // LFOS - LFO speed (no direct MIDI equivalent, skip)
                pos++;
                continue;
            }

            case 0xC3: { // LFODL - LFO delay (no direct MIDI equivalent, skip)
                pos++;
                continue;
            }

            case 0xC4: { // MOD - modulation depth
                const mod = rom[pos++];
                events.push({ tick, bytes: [0xB0 | channel, 0x01, mod & 0x7F] });
                continue;
            }

            case 0xC5: { // MODT - LFO type (skip, no MIDI equivalent)
                pos++;
                continue;
            }

            case 0xC8: { // TUNE - detune
                const tune = rom[pos++];
                // Fine tuning as RPN
                events.push({ tick, bytes: [0xB0 | channel, 0x65, 0x00] });
                events.push({ tick, bytes: [0xB0 | channel, 0x64, 0x01] }); // RPN 0x0001
                events.push({ tick, bytes: [0xB0 | channel, 0x06, tune & 0x7F] });
                continue;
            }

            case 0xCE: { // EOT - note off
                // Optional key argument
                let key = lastKey;
                if (pos < rom.length && rom[pos] < 0x80) {
                    key = rom[pos++];
                }
                key = (key + keyShift) & 0x7F;
                if (activeNotes.has(key)) {
                    events.push({ tick, bytes: [0x80 | channel, key, 0] });
                    activeNotes.delete(key);
                }
                continue;
            }

            case 0xCF: { // TIE - note on (indefinite, ended by EOT)
                // Arguments: key, vel
                let key = lastKey, vel = lastVel;
                if (pos < rom.length && rom[pos] < 0x80) { key = rom[pos++]; lastKey = key; }
                if (pos < rom.length && rom[pos] < 0x80) { vel = rom[pos++]; lastVel = vel; }
                key = (key + keyShift) & 0x7F;
                vel = vel & 0x7F;

                // Turn off previous note with same key
                if (activeNotes.has(key)) {
                    events.push({ tick, bytes: [0x80 | channel, key, 0] });
                }
                events.push({ tick, bytes: [0x90 | channel, key, vel] });
                activeNotes.set(key, { offTick: -1 }); // -1 = indefinite
                continue;
            }

            default: {
                if (cmd >= 0xD0 && cmd <= 0xFF) {
                    // Nxx - note with auto-duration
                    const duration = LEN_TBL[cmd - 0xD0];
                    let key = lastKey, vel = lastVel, gateExtra = 0;

                    if (pos < rom.length && rom[pos] < 0x80) { key = rom[pos++]; lastKey = key; }
                    if (pos < rom.length && rom[pos] < 0x80) { vel = rom[pos++]; lastVel = vel; }
                    if (pos < rom.length && rom[pos] < 0x80) { gateExtra = rom[pos++]; }

                    key = (key + keyShift) & 0x7F;
                    vel = vel & 0x7F;
                    const totalDur = duration + gateExtra;

                    // Turn off previous note with same key
                    if (activeNotes.has(key)) {
                        events.push({ tick, bytes: [0x80 | channel, key, 0] });
                        activeNotes.delete(key);
                    }

                    events.push({ tick, bytes: [0x90 | channel, key, vel] });

                    // Schedule note-off
                    const offTick = tick + totalDur;
                    events.push({ tick: offTick, bytes: [0x80 | channel, key, 0] });
                    // Don't add to activeNotes since we already scheduled the off
                    continue;
                }

                // Unknown command in range 0xB5-0xBA, 0xC6-0xC7, 0xC9-0xCD
                // Most take 0 or 1 argument. Skip 1 byte for safety.
                if (cmd >= 0xB5 && cmd <= 0xBA) {
                    // These don't take arguments in standard m4a
                    continue;
                }
                if (cmd >= 0xC6 && cmd <= 0xCD) {
                    pos++; // skip 1-byte argument
                    continue;
                }
                continue;
            }
        }
    }

    // Safety: turn off remaining notes
    for (const [key] of activeNotes) {
        events.push({ tick, bytes: [0x80 | channel, key, 0] });
    }
}
