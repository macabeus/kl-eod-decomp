/**
 * IndexedDB ROM persistence + SHA1 validation.
 * Falls back to module-level variable if IndexedDB is unavailable.
 */

import { EXPECTED_SHA1, ROM_SIZE } from './rom-tables.js';

const DB_NAME = 'klonoa-eod-decomp';
const STORE_NAME = 'rom';
const ROM_KEY = 'baserom';

// Fallback for incognito / no-IndexedDB
let _memoryRom = null;

function openDB() {
    return new Promise((resolve, reject) => {
        try {
            const req = indexedDB.open(DB_NAME, 1);
            req.onupgradeneeded = () => req.result.createObjectStore(STORE_NAME);
            req.onsuccess = () => resolve(req.result);
            req.onerror = () => reject(req.error);
        } catch (e) {
            reject(e);
        }
    });
}

/** Compute SHA-1 hex string from an ArrayBuffer. */
export async function computeSha1(buf) {
    const hash = await crypto.subtle.digest('SHA-1', buf);
    return Array.from(new Uint8Array(hash)).map(b => b.toString(16).padStart(2, '0')).join('');
}

/** Validate ROM size and SHA-1. Returns null if valid, or an error string. */
export async function validateRom(buf) {
    if (buf.byteLength !== ROM_SIZE) {
        return `Wrong ROM size: expected ${ROM_SIZE} bytes, got ${buf.byteLength}`;
    }
    const sha1 = await computeSha1(buf);
    if (sha1 !== EXPECTED_SHA1) {
        return `SHA-1 mismatch: expected ${EXPECTED_SHA1}, got ${sha1}`;
    }
    return null;
}

/** Store ROM ArrayBuffer in IndexedDB (with memory fallback). */
export async function storeRom(buf) {
    _memoryRom = buf;
    try {
        const db = await openDB();
        return new Promise((resolve, reject) => {
            const tx = db.transaction(STORE_NAME, 'readwrite');
            tx.objectStore(STORE_NAME).put(buf, ROM_KEY);
            tx.oncomplete = () => { db.close(); resolve(); };
            tx.onerror = () => { db.close(); reject(tx.error); };
        });
    } catch (e) {
        // IndexedDB unavailable, memory fallback is already set
    }
}

/** Load ROM from IndexedDB (or memory fallback). Returns ArrayBuffer or null. */
export async function loadRom() {
    if (_memoryRom) return _memoryRom;
    try {
        const db = await openDB();
        return new Promise((resolve, reject) => {
            const tx = db.transaction(STORE_NAME, 'readonly');
            const req = tx.objectStore(STORE_NAME).get(ROM_KEY);
            req.onsuccess = () => { db.close(); resolve(req.result || null); };
            req.onerror = () => { db.close(); reject(req.error); };
        });
    } catch (e) {
        return null;
    }
}

/** Clear stored ROM from IndexedDB and memory. */
export async function clearRom() {
    _memoryRom = null;
    try {
        const db = await openDB();
        return new Promise((resolve, reject) => {
            const tx = db.transaction(STORE_NAME, 'readwrite');
            tx.objectStore(STORE_NAME).delete(ROM_KEY);
            tx.oncomplete = () => { db.close(); resolve(); };
            tx.onerror = () => { db.close(); reject(tx.error); };
        });
    } catch (e) {
        // ignore
    }
}
