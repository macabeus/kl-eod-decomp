"""GBA BIOS decompression routines (LZ77 + Huffman) for Klonoa: Empire of Dreams.

Implements the decompression pipeline used by FUN_08043af4 (DecompressData):
  - If header bit 31 is set: Huffman (4-bit) then LZ77
  - Otherwise: LZ77 only
  - Decompressed size = header & 0x7FFFFFFF
"""

import struct


def bios_lz77_decompress(data, offset=0):
    """Decompress GBA BIOS LZ77 (SWI 0x11 / LZ77UnCompWram).

    Format:
      Byte 0: type marker (0x10 standard, but not strictly checked)
      Bytes 1-3: decompressed size (24-bit LE)
      Then compressed data stream.

    Args:
        data: bytes-like object containing LZ77 compressed data
        offset: starting offset within data

    Returns:
        bytes: decompressed data
    """
    header = struct.unpack_from('<I', data, offset)[0]
    decomp_size = (header >> 8) & 0xFFFFFF
    pos = offset + 4
    output = bytearray()

    while len(output) < decomp_size:
        if pos >= len(data):
            break
        flags = data[pos]
        pos += 1

        for bit in range(7, -1, -1):
            if len(output) >= decomp_size:
                break

            if flags & (1 << bit):
                # Back-reference
                if pos + 1 >= len(data):
                    break
                b0 = data[pos]
                b1 = data[pos + 1]
                pos += 2

                length = ((b0 >> 4) & 0xF) + 3
                disp = ((b0 & 0xF) << 8) | b1
                src = len(output) - disp - 1

                for i in range(length):
                    if len(output) >= decomp_size:
                        break
                    if src + i < 0:
                        output.append(0)
                    else:
                        output.append(output[src + i])
            else:
                # Literal byte
                if pos >= len(data):
                    break
                output.append(data[pos])
                pos += 1

    return bytes(output[:decomp_size])


def bios_huffman_decompress(data, offset=0):
    """Decompress GBA BIOS Huffman (SWI 0x13 / HuffUnComp / UnpackTilemap).

    Uses the mGBA-verified tree traversal formula:
      child = (node_addr & ~1) + (node & 0x3F) * 2 + 2 + bit

    Leaf flags: bit 7 = left child is leaf, bit 6 = right child is leaf.

    Args:
        data: bytes-like object containing Huffman compressed data
        offset: starting offset within data

    Returns:
        bytes: decompressed data
    """
    header = struct.unpack_from('<I', data, offset)[0]
    comp_type = header & 0xF0
    data_bits = header & 0x0F
    decomp_size = (header >> 8) & 0xFFFFFF

    if comp_type != 0x20:
        raise ValueError(f"Not Huffman data: type 0x{header & 0xFF:02X} (expected 0x2X)")

    tree_size_half = data[offset + 4]
    root_addr = offset + 5  # root node is right after tree_size byte

    # GBA BIOS bitstream position: source + treesize + 1 + 1, then 4-byte aligned
    # where source = offset+4 (tree_size byte), treesize = tree_size_half*2+1
    treesize = tree_size_half * 2 + 1
    bitstream_raw = offset + 4 + treesize + 1
    bitstream_start = (bitstream_raw + 3) & ~3

    output = bytearray()
    bit_pos = 0

    half_written = False
    current_byte = 0

    while len(output) < decomp_size:
        node_addr = root_addr
        node = data[node_addr]

        while True:
            # Read one bit from bitstream (MSB first within 32-bit words)
            word_idx = bit_pos >> 5
            bit_in_word = bit_pos & 31
            word_offset = bitstream_start + word_idx * 4
            if word_offset + 3 >= len(data):
                bit_pos += 1
                break
            word = struct.unpack_from('<I', data, word_offset)[0]
            bit = (word >> (31 - bit_in_word)) & 1
            bit_pos += 1

            # mGBA formula: child = (nPointer & ~1) + offset*2 + 2 + bit
            ofs = node & 0x3F
            child_addr = (node_addr & ~1) + ofs * 2 + 2 + bit

            # Leaf flags: bit 7 = left (0) child is leaf, bit 6 = right (1) child is leaf
            is_leaf = (node >> (7 - bit)) & 1

            if is_leaf:
                value = data[child_addr]
                if data_bits == 4:
                    if not half_written:
                        current_byte = value & 0x0F
                        half_written = True
                    else:
                        current_byte |= (value & 0x0F) << 4
                        output.append(current_byte)
                        half_written = False
                else:
                    output.append(value)
                break
            else:
                node_addr = child_addr
                node = data[child_addr]

    return bytes(output[:decomp_size])


def decompress_asset(data, offset=0):
    """Decompress a Klonoa asset using the FUN_08043af4 pipeline.

    The first 4 bytes are a header:
      - Bit 31: if set, data is Huffman-then-LZ77 compressed
      - Bits 0-30: decompressed size

    The compressed data starts at offset+4.

    Args:
        data: bytes-like object containing the asset
        offset: starting offset within data

    Returns:
        tuple: (decompressed_bytes, decomp_size, is_huffman)
    """
    header = struct.unpack_from('<I', data, offset)[0]
    is_huffman = bool(header & 0x80000000)
    decomp_size = header & 0x7FFFFFFF

    if decomp_size == 0:
        return bytes(), 0, False

    comp_offset = offset + 4

    if is_huffman:
        # First: Huffman decompress
        intermediate = bios_huffman_decompress(data, comp_offset)
        # Then: LZ77 decompress the result
        result = bios_lz77_decompress(intermediate, 0)
    else:
        # LZ77 only
        result = bios_lz77_decompress(data, comp_offset)

    return result, decomp_size, is_huffman


if __name__ == '__main__':
    import sys
    if len(sys.argv) < 3:
        print("Usage: python gba_decompress.py <rom.gba> <offset_hex>")
        sys.exit(1)

    rom_path = sys.argv[1]
    offset = int(sys.argv[2], 16)

    with open(rom_path, 'rb') as f:
        rom = f.read()

    result, size, is_huff = decompress_asset(rom, offset)
    comp_type = "Huffman+LZ77" if is_huff else "LZ77"
    print(f"Offset: 0x{offset:06X}")
    print(f"Compression: {comp_type}")
    print(f"Decompressed size: {size} bytes")
    print(f"Actual output: {len(result)} bytes")
