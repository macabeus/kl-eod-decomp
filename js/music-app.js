/**
 * Music page controller — converts m4a songs to MIDI on-the-fly from ROM.
 */

import { SONGS } from './rom-tables.js';
import { convertSong } from './agb2mid.js';
import { initRomPicker } from './rom-picker.js';

let currentBlobUrl = null;
let currentLi = null;
let romDv = null;
let romArr = null;

function buildSongList() {
    const songListEl = document.getElementById('song-list');
    const player = document.getElementById('player');
    const nowPlaying = document.getElementById('now-playing');
    if (!songListEl || !player) return;

    songListEl.innerHTML = '';

    SONGS.forEach(song => {
        const li = document.createElement('li');
        li.innerHTML = `
            <span class="song-id">#${String(song.id).padStart(2, '0')}</span>
            <span class="song-name">${song.name}</span>
            <span class="song-tracks">${song.tracks}ch</span>
        `;
        li.addEventListener('click', () => {
            try {
                const midiBytes = convertSong(romDv, romArr, song.id);
                const blob = new Blob([midiBytes], { type: 'audio/midi' });

                // Revoke previous URL to prevent leaks
                if (currentBlobUrl) {
                    URL.revokeObjectURL(currentBlobUrl);
                }
                currentBlobUrl = URL.createObjectURL(blob);

                player.src = currentBlobUrl;
                player.start();
                nowPlaying.textContent = `\u25B6 #${String(song.id).padStart(2, '0')} \u2014 ${song.name}`;

                if (currentLi) currentLi.classList.remove('active');
                li.classList.add('active');
                currentLi = li;
            } catch (e) {
                console.error(`Failed to convert song ${song.id}:`, e);
                nowPlaying.textContent = `Error converting song ${song.id}: ${e.message}`;
            }
        });
        songListEl.appendChild(li);
    });
}

// Entry point
initRomPicker('#rom-picker', (dv, rom) => {
    romDv = dv;
    romArr = rom;
    buildSongList();
});
