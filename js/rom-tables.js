/**
 * ROM address constants and helper reads for Klonoa: Empire of Dreams.
 * All offsets are file offsets (ROM address minus 0x08000000).
 */

// Expected ROM identity
export const EXPECTED_SHA1 = 'a0a298d9dba1ba15d04a42fc2eb35893d1a9569b';
export const ROM_SIZE = 4194304; // 4 MB

// Background tables
export const BG_TILE_TABLE    = 0x189034; // 162 u32 pointers
export const BG_TILEMAP_TABLE = 0x1892BC; // 162 u32 pointers
export const BG_PALETTE_TABLE = 0x188F5C; // 54 u32 pointers
export const BG_TILE_COUNT    = 162;
export const BG_PALETTE_COUNT = 54;

// GFX asset table
export const GFX_ASSET_TABLE  = 0x18B7AC; // 59 entries

// Song table
export const SONG_TABLE       = 0x118AE4; // 165 entries x 8 bytes

// World names (indexed 0-8)
export const WORLD_NAMES = [
    'World Map',       // 0
    'Breezegale',      // 1
    'Jugpot',          // 2
    'Forlock',         // 3
    'Volk',            // 4
    "Ishras Viel",     // 5
    'Timber Tracks',   // 6
    'The Dark',        // 7
    'EX Levels',       // 8
];

// World slug names for IDs
export const WORLD_SLUGS = [
    'world_map', 'breezegale', 'jugpot', 'forlock',
    'volk', 'ishras_viel', 'timber_tracks', 'the_dark', 'ex_levels',
];

// Song metadata (35 BGM songs, indices 0-34)
export const SONGS = [
    { id: 0,  name: 'Title Screen / Intro',  tracks: 7 },
    { id: 1,  name: 'Menu / File Select',     tracks: 4 },
    { id: 2,  name: 'Story Cutscene',         tracks: 3 },
    { id: 3,  name: 'World Map',              tracks: 4 },
    { id: 4,  name: 'Vision 1 \u2014 Breeze', tracks: 6 },
    { id: 5,  name: 'Vision 2 \u2014 Jungle', tracks: 6 },
    { id: 6,  name: 'Vision 3 \u2014 Cave',   tracks: 6 },
    { id: 7,  name: 'Vision 4 \u2014 Temple', tracks: 7 },
    { id: 8,  name: 'Vision 5 \u2014 Ruins',  tracks: 6 },
    { id: 9,  name: 'Vision 6 \u2014 Sky',    tracks: 7 },
    { id: 10, name: 'Vision 7 \u2014 Snow',   tracks: 6 },
    { id: 11, name: 'Vision 8 \u2014 Night',  tracks: 7 },
    { id: 12, name: 'Boss Intro',             tracks: 7 },
    { id: 13, name: 'Boss Battle',            tracks: 4 },
    { id: 14, name: 'Boss Clear / Victory',   tracks: 5 },
    { id: 15, name: 'Cutscene \u2014 Emotional', tracks: 3 },
    { id: 16, name: 'Mini-game',              tracks: 6 },
    { id: 17, name: 'Shop / Item',            tracks: 4 },
    { id: 18, name: 'Puzzle Room',            tracks: 6 },
    { id: 19, name: 'Secret Area',            tracks: 5 },
    { id: 20, name: 'Final World',            tracks: 7 },
    { id: 21, name: 'Final Boss Phase 1',     tracks: 6 },
    { id: 22, name: 'Final Boss Phase 2',     tracks: 6 },
    { id: 23, name: 'Ending Theme',           tracks: 5 },
    { id: 24, name: 'Credits',                tracks: 6 },
    { id: 25, name: 'Game Over',              tracks: 5 },
    { id: 26, name: 'BGM 26',                 tracks: 6 },
    { id: 27, name: 'BGM 27',                 tracks: 6 },
    { id: 28, name: 'BGM 28',                 tracks: 6 },
    { id: 29, name: 'BGM 29',                 tracks: 5 },
    { id: 30, name: 'BGM 30',                 tracks: 6 },
    { id: 31, name: 'Jingle \u2014 Short',    tracks: 3 },
    { id: 32, name: 'BGM 32',                 tracks: 7 },
    { id: 33, name: 'Jingle \u2014 Fanfare',  tracks: 3 },
    { id: 34, name: 'BGM 34',                 tracks: 8 },
];

// Little-endian DataView helpers
export function readU32(dv, off) { return dv.getUint32(off, true); }
export function readU16(dv, off) { return dv.getUint16(off, true); }
export function readU8(dv, off)  { return dv.getUint8(off); }

/** Convert a GBA ROM pointer (0x08XXXXXX) to a file offset. */
export function romPtr(dv, off) {
    const val = readU32(dv, off);
    return (val >= 0x08000000 && val < 0x0A000000) ? val - 0x08000000 : null;
}
