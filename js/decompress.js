/**
 * GBA BIOS decompression: LZ77 + Huffman.
 * Direct port of scripts/gba_decompress.py.
 */

/**
 * Decompress GBA BIOS LZ77 (SWI 0x11 / LZ77UnCompWram).
 * @param {Uint8Array} data
 * @param {number} offset
 * @returns {Uint8Array}
 */
export function lz77Decompress(data, offset = 0) {
    const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
    const header = dv.getUint32(offset, true);
    const decompSize = (header >>> 8) & 0xFFFFFF;
    let pos = offset + 4;
    const output = new Uint8Array(decompSize);
    let outPos = 0;

    while (outPos < decompSize) {
        if (pos >= data.length) break;
        const flags = data[pos++];

        for (let bit = 7; bit >= 0; bit--) {
            if (outPos >= decompSize) break;

            if (flags & (1 << bit)) {
                // Back-reference
                if (pos + 1 >= data.length) break;
                const b0 = data[pos++];
                const b1 = data[pos++];
                const length = ((b0 >>> 4) & 0xF) + 3;
                const disp = ((b0 & 0xF) << 8) | b1;
                let src = outPos - disp - 1;

                for (let i = 0; i < length; i++) {
                    if (outPos >= decompSize) break;
                    output[outPos++] = (src + i < 0) ? 0 : output[src + i];
                }
            } else {
                // Literal
                if (pos >= data.length) break;
                output[outPos++] = data[pos++];
            }
        }
    }
    return output;
}

/**
 * Decompress GBA BIOS Huffman (SWI 0x13 / HuffUnComp).
 * Uses mGBA-verified tree traversal formula.
 * @param {Uint8Array} data
 * @param {number} offset
 * @returns {Uint8Array}
 */
export function huffmanDecompress(data, offset = 0) {
    const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
    const header = dv.getUint32(offset, true);
    const dataBits = header & 0x0F;
    const decompSize = (header >>> 8) & 0xFFFFFF;

    const treeSizeHalf = data[offset + 4];
    const rootAddr = offset + 5;

    const treesize = treeSizeHalf * 2 + 1;
    const bitstreamRaw = offset + 4 + treesize + 1;
    const bitstreamStart = (bitstreamRaw + 3) & ~3;

    const output = new Uint8Array(decompSize);
    let outPos = 0;
    let bitPos = 0;

    let halfWritten = false;
    let currentByte = 0;

    while (outPos < decompSize) {
        let nodeAddr = rootAddr;
        let node = data[nodeAddr];

        while (true) {
            // Read one bit (MSB first within 32-bit LE words)
            const wordIdx = bitPos >>> 5;
            const bitInWord = bitPos & 31;
            const wordOffset = bitstreamStart + wordIdx * 4;
            if (wordOffset + 3 >= data.length) { bitPos++; break; }
            const word = dv.getUint32(wordOffset, true);
            const bit = (word >>> (31 - bitInWord)) & 1;
            bitPos++;

            // mGBA formula
            const ofs = node & 0x3F;
            const childAddr = (nodeAddr & ~1) + ofs * 2 + 2 + bit;

            // Leaf flags: bit 7 = left(0) is leaf, bit 6 = right(1) is leaf
            const isLeaf = (node >>> (7 - bit)) & 1;

            if (isLeaf) {
                const value = data[childAddr];
                if (dataBits === 4) {
                    if (!halfWritten) {
                        currentByte = value & 0x0F;
                        halfWritten = true;
                    } else {
                        currentByte |= (value & 0x0F) << 4;
                        output[outPos++] = currentByte;
                        halfWritten = false;
                    }
                } else {
                    output[outPos++] = value;
                }
                break;
            } else {
                nodeAddr = childAddr;
                node = data[childAddr];
            }
        }
    }
    return output;
}

/**
 * Decompress a Klonoa asset using the FUN_08043af4 pipeline.
 * Header bit 31: Huffman+LZ77 vs LZ77-only. Bits 0-30: decompressed size.
 * @param {Uint8Array} data
 * @param {number} offset
 * @returns {{ result: Uint8Array, size: number, isHuffman: boolean }}
 */
export function decompressAsset(data, offset = 0) {
    const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
    const header = dv.getUint32(offset, true);
    const isHuffman = !!(header & 0x80000000);
    const decompSize = header & 0x7FFFFFFF;

    if (decompSize === 0) return { result: new Uint8Array(0), size: 0, isHuffman: false };

    const compOffset = offset + 4;
    let result;

    if (isHuffman) {
        const intermediate = huffmanDecompress(data, compOffset);
        result = lz77Decompress(intermediate, 0);
    } else {
        result = lz77Decompress(data, compOffset);
    }

    return { result, size: decompSize, isHuffman };
}
