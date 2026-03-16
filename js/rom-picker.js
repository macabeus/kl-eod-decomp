/**
 * Shared ROM file picker UI component.
 * Shows file input + drag-and-drop, validates SHA1, persists in IndexedDB.
 */

import { loadRom, storeRom, validateRom, clearRom } from './rom-store.js';

/**
 * Initialize the ROM picker in the given container.
 * @param {string} containerSelector  CSS selector for the picker container
 * @param {function} onRomLoaded      Called with DataView when ROM is ready
 */
export async function initRomPicker(containerSelector, onRomLoaded) {
    const container = document.querySelector(containerSelector);
    if (!container) return;

    const content = document.getElementById('content');

    // Build picker UI
    container.innerHTML = `
        <div class="rom-picker-box">
            <h2>Load ROM</h2>
            <p>Select or drag your <strong>Klonoa: Empire of Dreams</strong> GBA ROM to explore the game's assets.</p>
            <div class="rom-drop-zone" id="rom-drop-zone">
                <div class="rom-drop-label">Drop ROM file here or click to browse</div>
                <input type="file" id="rom-file-input" accept=".gba,.bin" hidden>
            </div>
            <div class="rom-error" id="rom-error"></div>
        </div>
    `;

    const dropZone = document.getElementById('rom-drop-zone');
    const fileInput = document.getElementById('rom-file-input');
    const errorEl = document.getElementById('rom-error');

    dropZone.addEventListener('click', () => fileInput.click());
    dropZone.addEventListener('dragover', (e) => { e.preventDefault(); dropZone.classList.add('dragover'); });
    dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
    dropZone.addEventListener('drop', (e) => {
        e.preventDefault();
        dropZone.classList.remove('dragover');
        if (e.dataTransfer.files.length) handleFile(e.dataTransfer.files[0]);
    });
    fileInput.addEventListener('change', () => {
        if (fileInput.files.length) handleFile(fileInput.files[0]);
    });

    async function handleFile(file) {
        errorEl.textContent = '';
        dropZone.querySelector('.rom-drop-label').textContent = 'Validating...';

        try {
            const buf = await file.arrayBuffer();
            const err = await validateRom(buf);
            if (err) {
                errorEl.textContent = err;
                dropZone.querySelector('.rom-drop-label').textContent = 'Drop ROM file here or click to browse';
                return;
            }
            await storeRom(buf);
            activateRom(buf);
        } catch (e) {
            errorEl.textContent = 'Failed to read file: ' + e.message;
            dropZone.querySelector('.rom-drop-label').textContent = 'Drop ROM file here or click to browse';
        }
    }

    function activateRom(buf) {
        container.classList.add('hidden');
        if (content) content.classList.remove('hidden');
        showNavStatus(true);
        onRomLoaded(new DataView(buf), new Uint8Array(buf));
    }

    // Check for stored ROM on init
    try {
        const stored = await loadRom();
        if (stored) {
            const err = await validateRom(stored);
            if (!err) {
                activateRom(stored);
                return;
            }
        }
    } catch (e) {
        // no stored ROM, show picker
    }

    // No ROM — show picker, hide content
    container.classList.remove('hidden');
    if (content) content.classList.add('hidden');
}

/** Show/hide ROM status indicator in nav bar. */
function showNavStatus(loaded) {
    let indicator = document.getElementById('rom-status');
    if (!indicator) {
        indicator = document.createElement('span');
        indicator.id = 'rom-status';
        const nav = document.querySelector('nav');
        if (nav) nav.appendChild(indicator);
    }
    if (loaded) {
        indicator.className = 'rom-status-loaded';
        indicator.innerHTML = 'ROM loaded <button id="rom-clear-btn" class="rom-clear-btn">Clear</button>';
        document.getElementById('rom-clear-btn').addEventListener('click', async (e) => {
            e.stopPropagation();
            await clearRom();
            location.reload();
        });
    } else {
        indicator.className = 'rom-status-none';
        indicator.textContent = 'No ROM';
    }
}
