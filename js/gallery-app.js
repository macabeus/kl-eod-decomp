/**
 * Gallery page controller — renders all backgrounds from ROM dynamically.
 */

import { WORLD_NAMES, WORLD_SLUGS } from './rom-tables.js';
import { renderScene, renderBgLayer, clearCache } from './gfx-render.js';
import { initRomPicker } from './rom-picker.js';

const VISIONS = 6;
const WORLDS = 9;
const LAYERS = 3;

let lightboxEl = null;

function openLightbox(canvas) {
    if (!lightboxEl) {
        lightboxEl = document.createElement('div');
        lightboxEl.className = 'lightbox';
        lightboxEl.addEventListener('click', () => lightboxEl.classList.remove('active'));
        document.body.appendChild(lightboxEl);
    }
    lightboxEl.innerHTML = '';
    const img = new Image();
    img.src = canvas.toDataURL();
    lightboxEl.appendChild(img);
    lightboxEl.classList.add('active');
}

function createGalleryCard(canvas, title, worldIdx) {
    const card = document.createElement('div');
    card.className = 'gallery-card';
    card.dataset.world = String(worldIdx);

    const preview = document.createElement('div');
    preview.className = 'preview';
    preview.addEventListener('click', () => openLightbox(canvas));

    const img = new Image();
    img.src = canvas.toDataURL();
    img.alt = title;
    img.loading = 'lazy';
    preview.appendChild(img);

    const info = document.createElement('div');
    info.className = 'info';
    info.innerHTML = `<h3>${title}</h3><div class="meta"><span>${canvas.width}x${canvas.height}</span></div>`;

    card.appendChild(preview);
    card.appendChild(info);
    return card;
}

async function renderGallery(dv, rom) {
    clearCache();
    const compositeGrid = document.getElementById('composite-gallery');
    const layerGrid = document.getElementById('layer-gallery');

    let compositeCount = 0;
    let layerCount = 0;

    for (let v = 1; v <= VISIONS; v++) {
        for (let w = 0; w < WORLDS; w++) {
            // Yield to event loop periodically
            if ((v * WORLDS + w) % 5 === 0) {
                await new Promise(r => setTimeout(r, 0));
            }

            const { composite, layers } = renderScene(rom, dv, v, w);

            // Composite card
            if (composite && compositeGrid) {
                const title = `V${v} ${WORLD_NAMES[w]}`;
                compositeGrid.appendChild(createGalleryCard(composite, title, w));
                compositeCount++;
            }

            // Individual layer cards
            if (layerGrid) {
                for (let l = 0; l < LAYERS; l++) {
                    if (layers[l]) {
                        const layerNames = ['BG0 (far)', 'BG1 (mid)', 'BG2 (near)'];
                        const title = `V${v} ${WORLD_NAMES[w]} ${layerNames[l]}`;
                        layerGrid.appendChild(createGalleryCard(layers[l], title, w));
                        layerCount++;
                    }
                }
            }
        }
    }

    // Update counts
    const compCount = document.getElementById('composite-count');
    if (compCount) compCount.textContent = compositeCount;
    const lyrCount = document.getElementById('layer-count');
    if (lyrCount) lyrCount.textContent = layerCount;
}

// Setup filter bar
function setupFilters() {
    const filterWorld = document.getElementById('filter-world');
    if (!filterWorld) return;

    filterWorld.addEventListener('change', () => {
        const val = filterWorld.value;
        document.querySelectorAll('.gallery-card').forEach(card => {
            if (val === 'all' || card.dataset.world === val) {
                card.classList.remove('hidden');
            } else {
                card.classList.add('hidden');
            }
        });
    });
}

// Entry point
initRomPicker('#rom-picker', (dv, rom) => {
    setupFilters();
    renderGallery(dv, rom);
});
