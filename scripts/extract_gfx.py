"""Extract graphics assets from Klonoa: Empire of Dreams ROM.

Walks three ROM tables (GFX_ASSET_TABLE, TILESET_TABLE, SPRITE_FRAME_TABLE),
extracts compressed blobs, decompresses them, and renders PNGs for visual
inspection. Compressed .bin files are byte-exact copies from ROM for use in
the build system.

Usage:
    python scripts/extract_gfx.py [baserom.gba]
"""

import json
import math
import os
import struct
import sys

from gba_decompress import bios_huffman_decompress, bios_lz77_decompress, decompress_asset

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is required. Install with: pip install Pillow")
    sys.exit(1)

ROM_PATH = "baserom.gba"
OUT_DIR = "graphics"

# ROM table addresses (from include/globals.h)
ROM_GFX_ASSET_TABLE = 0x18B7AC
ROM_TILESET_TABLE = 0x18B8E0
ROM_SPRITE_FRAME_TABLE = 0x78FC8
ROM_BG_TILE_TABLE = 0x189034
ROM_BG_TILEMAP_TABLE = 0x1892BC
ROM_BG_PALETTE_TABLE = 0x188F5C

# World names for readable filenames
WORLD_NAMES = [
    "world_map",       # 0
    "breezegale",      # 1 (Bell Hill)
    "jugpot",          # 2
    "forlock",         # 3
    "volk",            # 4
    "ishras_viel",     # 5 (Ishras Viel / Baladium)
    "timber_tracks",   # 6
    "the_dark",        # 7 (Garlen's lair)
    "ex_levels",       # 8
]

# Default grayscale palette for 4bpp (16 colors)
GRAYSCALE_4BPP = [(i * 17, i * 17, i * 17) for i in range(16)]

# Default grayscale palette for 8bpp (256 colors)
GRAYSCALE_8BPP = [(i, i, i) for i in range(256)]


def gba_rgb555_to_rgb888(color16):
    """Convert GBA RGB555 (16-bit) to RGB888 tuple."""
    r = (color16 & 0x1F) << 3
    g = ((color16 >> 5) & 0x1F) << 3
    b = ((color16 >> 10) & 0x1F) << 3
    return (r, g, b)


def parse_palette(data, offset, num_colors=16):
    """Parse GBA RGB555 palette data into list of RGB888 tuples."""
    colors = []
    for i in range(num_colors):
        if offset + i * 2 + 1 >= len(data):
            colors.append((0, 0, 0))
            continue
        c16 = struct.unpack_from('<H', data, offset + i * 2)[0]
        colors.append(gba_rgb555_to_rgb888(c16))
    return colors


def render_4bpp_tiles(tile_data, palette, tiles_per_row=16):
    """Render 4bpp GBA tile data as a PIL Image.

    Each tile is 8x8 pixels, 32 bytes (4 bits per pixel).
    """
    if len(tile_data) < 32:
        return None

    num_tiles = len(tile_data) // 32
    if num_tiles == 0:
        return None

    cols = min(tiles_per_row, num_tiles)
    rows = math.ceil(num_tiles / cols)
    width = cols * 8
    height = rows * 8

    img = Image.new('RGB', (width, height), (0, 0, 0))
    pixels = img.load()

    for t in range(num_tiles):
        tx = (t % cols) * 8
        ty = (t // cols) * 8
        tile_offset = t * 32

        for y in range(8):
            for x in range(0, 8, 2):
                byte_idx = tile_offset + y * 4 + x // 2
                if byte_idx >= len(tile_data):
                    continue
                byte = tile_data[byte_idx]
                # Low nibble = left pixel, high nibble = right pixel
                px_left = byte & 0x0F
                px_right = (byte >> 4) & 0x0F
                if tx + x < width and ty + y < height:
                    pixels[tx + x, ty + y] = palette[px_left]
                if tx + x + 1 < width and ty + y < height:
                    pixels[tx + x + 1, ty + y] = palette[px_right]

    return img


def render_8bpp_tiles(tile_data, palette, tiles_per_row=16):
    """Render 8bpp GBA tile data as a PIL Image.

    Each tile is 8x8 pixels, 64 bytes (8 bits per pixel).
    """
    if len(tile_data) < 64:
        return None

    num_tiles = len(tile_data) // 64
    if num_tiles == 0:
        return None

    cols = min(tiles_per_row, num_tiles)
    rows = math.ceil(num_tiles / cols)
    width = cols * 8
    height = rows * 8

    img = Image.new('RGB', (width, height), (0, 0, 0))
    pixels = img.load()

    for t in range(num_tiles):
        tx = (t % cols) * 8
        ty = (t // cols) * 8
        tile_offset = t * 64

        for y in range(8):
            for x in range(8):
                byte_idx = tile_offset + y * 8 + x
                if byte_idx >= len(tile_data):
                    continue
                px = tile_data[byte_idx]
                if px < len(palette) and tx + x < width and ty + y < height:
                    pixels[tx + x, ty + y] = palette[px]

    return img


def compose_bg(tiles_raw, tilemap_raw, palette_raw, map_w=32, map_h=32):
    """Compose a GBA background from tile charblock, tilemap, and palette data.

    Arranges 8x8 tiles according to the tilemap, applying palette bank selection
    and horizontal/vertical flip flags per the GBA BG screenblock format.

    Args:
        tiles_raw: raw 4bpp tile data (32 bytes per 8x8 tile)
        tilemap_raw: GBA screenblock entries (u16 per cell)
        tilemap_raw format: bits 0-9 = tile ID, bit 10 = hflip,
                           bit 11 = vflip, bits 12-15 = palette bank
        palette_raw: 512 bytes of GBA RGB555 palette (16 banks x 16 colors)
        map_w: tilemap width in tiles (default 32)
        map_h: tilemap height in tiles (default 32)

    Returns:
        PIL Image or None if data is insufficient
    """
    if len(tiles_raw) < 32 or len(tilemap_raw) < 2:
        return None

    # Parse all 16 palette banks (16 colors each for 4bpp)
    palettes = []
    for bank in range(16):
        pal = []
        for c in range(16):
            off = (bank * 16 + c) * 2
            if off + 1 < len(palette_raw):
                c16 = struct.unpack_from('<H', palette_raw, off)[0]
                pal.append(gba_rgb555_to_rgb888(c16))
            else:
                pal.append((0, 0, 0))
        palettes.append(pal)

    width = map_w * 8
    height = map_h * 8
    img = Image.new('RGB', (width, height), (0, 0, 0))
    pixels = img.load()

    num_tiles = len(tiles_raw) // 32

    for my in range(map_h):
        for mx in range(map_w):
            idx = my * map_w + mx
            if idx * 2 + 1 >= len(tilemap_raw):
                continue
            entry = struct.unpack_from('<H', tilemap_raw, idx * 2)[0]
            tile_id = entry & 0x3FF
            hflip = (entry >> 10) & 1
            vflip = (entry >> 11) & 1
            pal_bank = (entry >> 12) & 0xF

            if tile_id >= num_tiles:
                continue

            pal = palettes[pal_bank]
            tile_offset = tile_id * 32
            px_x = mx * 8
            px_y = my * 8

            for ty in range(8):
                for tx in range(0, 8, 2):
                    byte_idx = tile_offset + ty * 4 + tx // 2
                    if byte_idx >= len(tiles_raw):
                        continue
                    byte_val = tiles_raw[byte_idx]
                    lo = byte_val & 0x0F
                    hi = (byte_val >> 4) & 0x0F

                    if hflip:
                        dx0 = 7 - tx - 1
                        dx1 = 7 - tx
                        c0, c1 = hi, lo
                    else:
                        dx0 = tx
                        dx1 = tx + 1
                        c0, c1 = lo, hi

                    dy = (7 - ty) if vflip else ty

                    if px_x + dx0 < width and px_y + dy < height:
                        pixels[px_x + dx0, px_y + dy] = pal[c0]
                    if px_x + dx1 < width and px_y + dy < height:
                        pixels[px_x + dx1, px_y + dy] = pal[c1]

    return img


def compose_bg_rgba(tiles_raw, tilemap_raw, palette_raw, map_w=32, map_h=32):
    """Compose a GBA background with transparency (color 0 = transparent).

    Same as compose_bg but returns RGBA image for layer compositing.
    """
    if len(tiles_raw) < 32 or len(tilemap_raw) < 2:
        return None

    palettes = []
    for bank in range(16):
        pal = []
        for c in range(16):
            off = (bank * 16 + c) * 2
            if off + 1 < len(palette_raw):
                c16 = struct.unpack_from('<H', palette_raw, off)[0]
                pal.append(gba_rgb555_to_rgb888(c16))
            else:
                pal.append((0, 0, 0))
        palettes.append(pal)

    width = map_w * 8
    height = map_h * 8
    img = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    pixels = img.load()
    num_tiles = len(tiles_raw) // 32

    for my in range(map_h):
        for mx in range(map_w):
            idx = my * map_w + mx
            if idx * 2 + 1 >= len(tilemap_raw):
                continue
            entry = struct.unpack_from('<H', tilemap_raw, idx * 2)[0]
            tile_id = entry & 0x3FF
            hflip = (entry >> 10) & 1
            vflip = (entry >> 11) & 1
            pal_bank = (entry >> 12) & 0xF
            if tile_id >= num_tiles:
                continue
            pal = palettes[pal_bank]
            tile_offset = tile_id * 32
            px_x = mx * 8
            px_y = my * 8

            for ty in range(8):
                for tx in range(0, 8, 2):
                    byte_idx = tile_offset + ty * 4 + tx // 2
                    if byte_idx >= len(tiles_raw):
                        continue
                    byte_val = tiles_raw[byte_idx]
                    lo = byte_val & 0x0F
                    hi = (byte_val >> 4) & 0x0F

                    if hflip:
                        dx0, dx1 = 7 - tx - 1, 7 - tx
                        c0, c1 = hi, lo
                    else:
                        dx0, dx1 = tx, tx + 1
                        c0, c1 = lo, hi

                    dy = (7 - ty) if vflip else ty

                    if c0 != 0 and px_x + dx0 < width and px_y + dy < height:
                        pixels[px_x + dx0, px_y + dy] = pal[c0] + (255,)
                    if c1 != 0 and px_x + dx1 < width and px_y + dy < height:
                        pixels[px_x + dx1, px_y + dy] = pal[c1] + (255,)

    return img


def decompress_bg_asset(rom, offset):
    """Decompress a BG tile/tilemap/palette asset and strip the 4-byte sub-header."""
    result, _, _ = decompress_asset(rom, offset)
    return result[4:]


def extract_composed_backgrounds(rom, manifest):
    """Extract and compose all level backgrounds using tile + tilemap + palette tables.

    Reads ROM_BG_TILE_TABLE (162 entries), ROM_BG_TILEMAP_TABLE (162 entries),
    and ROM_BG_PALETTE_TABLE (54 entries) to produce composed PNG images for
    every vision/world/layer combination.

    Table indexing:
        BG tiles/tilemaps: 162 entries = 6 visions x 9 worlds x 3 layers
            index = (vision-1) * 27 + world * 3 + layer
        BG palettes: 54 entries = 6 visions x 9 worlds
            index = (vision-1) * 9 + world
    """
    print("\n=== Extracting Composed Backgrounds ===")

    # Read all three tables
    tile_offsets = []
    tmap_offsets = []
    pal_offsets = []

    for i in range(162):
        val = struct.unpack_from('<I', rom, ROM_BG_TILE_TABLE + i * 4)[0]
        tile_offsets.append(val - 0x08000000 if val >= 0x08000000 else None)

    for i in range(162):
        val = struct.unpack_from('<I', rom, ROM_BG_TILEMAP_TABLE + i * 4)[0]
        tmap_offsets.append(val - 0x08000000 if val >= 0x08000000 else None)

    for i in range(54):
        val = struct.unpack_from('<I', rom, ROM_BG_PALETTE_TABLE + i * 4)[0]
        pal_offsets.append(val - 0x08000000 if val >= 0x08000000 else None)

    out_dir = os.path.join(OUT_DIR, "backgrounds")
    os.makedirs(out_dir, exist_ok=True)

    # Cache decompressed data to avoid redundant work (some entries share offsets)
    decomp_cache = {}

    def get_decomp(offset):
        if offset not in decomp_cache:
            decomp_cache[offset] = decompress_bg_asset(rom, offset)
        return decomp_cache[offset]

    composed_count = 0

    for vision in range(1, 7):  # visions 1-6
        for world in range(9):  # worlds 0-8
            pal_idx = (vision - 1) * 9 + world
            if pal_idx >= len(pal_offsets) or pal_offsets[pal_idx] is None:
                continue

            try:
                pal_raw = get_decomp(pal_offsets[pal_idx])
            except Exception as e:
                print(f"  v{vision}/w{world} palette FAILED: {e}")
                continue

            world_name = WORLD_NAMES[world] if world < len(WORLD_NAMES) else f"world_{world}"

            for layer in range(3):
                bg_idx = (vision - 1) * 27 + world * 3 + layer
                if bg_idx >= len(tile_offsets):
                    continue

                tile_off = tile_offsets[bg_idx]
                tmap_off = tmap_offsets[bg_idx]
                if tile_off is None or tmap_off is None:
                    continue

                try:
                    tiles = get_decomp(tile_off)
                    tilemap = get_decomp(tmap_off)
                except Exception as e:
                    print(f"  v{vision}/{world_name}/L{layer} FAILED: {e}")
                    continue

                # Determine tilemap dimensions from size
                map_entries = len(tilemap) // 2
                if map_entries >= 2048:
                    map_w, map_h = 64, 32
                elif map_entries >= 1024:
                    map_w, map_h = 32, 32
                else:
                    map_w = 32
                    map_h = max(1, map_entries // 32)

                img = compose_bg(tiles, tilemap, pal_raw, map_w, map_h)
                if img is None:
                    continue

                name = f"v{vision}_{world_name}_L{layer}"
                png_path = os.path.join(out_dir, f"{name}.png")
                img.save(png_path)
                composed_count += 1

                asset_info = {
                    "table": "BG_COMPOSED",
                    "vision": vision,
                    "world": world,
                    "world_name": world_name,
                    "layer": layer,
                    "tile_offset": f"0x{tile_off:06X}",
                    "tilemap_offset": f"0x{tmap_off:06X}",
                    "palette_offset": f"0x{pal_offsets[pal_idx]:06X}",
                    "dimensions": f"{img.width}x{img.height}",
                    "png": png_path,
                }
                manifest["assets"].append(asset_info)

                if composed_count <= 10 or composed_count % 20 == 0:
                    print(f"  {name} ({img.width}x{img.height}) -> {png_path}")

    print(f"  Composed {composed_count} background layers total")

    # Generate composite images (all 3 layers overlaid)
    composite_dir = os.path.join(OUT_DIR, "composites")
    os.makedirs(composite_dir, exist_ok=True)
    composite_count = 0

    for vision in range(1, 7):
        for world in range(9):
            pal_idx = (vision - 1) * 9 + world
            if pal_idx >= len(pal_offsets) or pal_offsets[pal_idx] is None:
                continue

            try:
                pal_raw = get_decomp(pal_offsets[pal_idx])
            except Exception:
                continue

            world_name = WORLD_NAMES[world] if world < len(WORLD_NAMES) else f"world_{world}"

            # Render all 3 layers with transparency
            rgba_layers = []
            max_w, max_h = 0, 0
            for layer in range(3):
                bg_idx = (vision - 1) * 27 + world * 3 + layer
                if bg_idx >= len(tile_offsets):
                    rgba_layers.append(None)
                    continue
                tile_off = tile_offsets[bg_idx]
                tmap_off = tmap_offsets[bg_idx]
                if tile_off is None or tmap_off is None:
                    rgba_layers.append(None)
                    continue
                try:
                    tiles = get_decomp(tile_off)
                    tilemap = get_decomp(tmap_off)
                except Exception:
                    rgba_layers.append(None)
                    continue
                map_entries = len(tilemap) // 2
                if map_entries >= 2048:
                    mw, mh = 64, 32
                elif map_entries >= 1024:
                    mw, mh = 32, 32
                else:
                    mw, mh = 32, max(1, map_entries // 32)
                layer_img = compose_bg_rgba(tiles, tilemap, pal_raw, mw, mh)
                rgba_layers.append(layer_img)
                if layer_img:
                    max_w = max(max_w, layer_img.width)
                    max_h = max(max_h, layer_img.height)

            if max_w == 0 or max_h == 0:
                continue

            # Composite: L0 (back), L1 (mid), L2 (front)
            composite = Image.new('RGBA', (max_w, max_h), (0, 0, 0, 255))
            for layer_img in rgba_layers:
                if layer_img is None:
                    continue
                # Tile the layer to fill the composite size (GBA wraps BG layers)
                tiled = Image.new('RGBA', (max_w, max_h), (0, 0, 0, 0))
                for tx in range(0, max_w, layer_img.width):
                    for ty in range(0, max_h, layer_img.height):
                        tiled.paste(layer_img, (tx, ty))
                composite = Image.alpha_composite(composite, tiled)

            name = f"v{vision}_{world_name}"
            png_path = os.path.join(composite_dir, f"{name}.png")
            composite.convert('RGB').save(png_path)
            composite_count += 1

            asset_info = {
                "table": "BG_COMPOSITE",
                "vision": vision,
                "world": world,
                "world_name": world_name,
                "dimensions": f"{max_w}x{max_h}",
                "png": png_path,
            }
            manifest["assets"].append(asset_info)

    print(f"  Generated {composite_count} composite images")


def find_compressed_size(rom, offset):
    """Determine the compressed size of an asset starting at offset.

    Reads the asset header and walks the compressed data to find its end.
    For Huffman+LZ77: header(4) + huffman_stream
    For LZ77 only: header(4) + lz77_stream
    """
    header = struct.unpack_from('<I', rom, offset)[0]
    is_huffman = bool(header & 0x80000000)

    if is_huffman:
        # Header(4) + Huffman data
        huff_offset = offset + 4
        huff_header = struct.unpack_from('<I', rom, huff_offset)[0]
        huff_decomp_size = (huff_header >> 8) & 0xFFFFFF

        tree_size_half = rom[huff_offset + 4]
        treesize = tree_size_half * 2 + 1
        bitstream_raw = huff_offset + treesize + 1
        bitstream_start = (bitstream_raw + 3) & ~3

        # Estimate: the huffman bitstream encodes huff_decomp_size bytes
        # We can't easily compute the exact end without decoding, so
        # we'll find where the next asset starts or use a generous bound
        # For now, use the next table entry as boundary
        return None  # Will use table-based sizing
    else:
        # LZ77 only - similar issue, can't easily find end without decoding
        return None


def extract_gfx_assets(rom, manifest):
    """Extract assets from ROM_GFX_ASSET_TABLE."""
    print("\n=== Extracting GFX Assets ===")

    # Read table entries
    entries = []
    pos = ROM_GFX_ASSET_TABLE
    while True:
        val = struct.unpack_from('<I', rom, pos)[0]
        if val < 0x08000000 or val > 0x08400000:
            break
        entries.append(val - 0x08000000)
        pos += 4

    print(f"Found {len(entries)} GFX asset entries")

    # Determine asset boundaries for compressed blob extraction
    # Sort unique offsets to find boundaries
    unique_offsets = sorted(set(entries))

    for i, offset in enumerate(entries):
        asset_info = {
            "table": "GFX_ASSET_TABLE",
            "index": i,
            "rom_offset": f"0x{offset:06X}",
        }

        header = struct.unpack_from('<I', rom, offset)[0]
        is_huffman = bool(header & 0x80000000)
        decomp_size = header & 0x7FFFFFFF

        # Detect palette entries: first 2 bytes are 0x0000 (GBA transparent black),
        # followed by non-zero GBA RGB555 color values. These are NOT compressed.
        is_palette = False
        if not is_huffman and offset + 4 < len(rom):
            first_halfword = struct.unpack_from('<H', rom, offset)[0]
            if first_halfword == 0x0000 and decomp_size > 0:
                # Check second halfword is a plausible GBA color (non-zero, < 0x8000)
                second_halfword = struct.unpack_from('<H', rom, offset + 2)[0]
                if 0 < second_halfword < 0x8000:
                    is_palette = True

        if decomp_size == 0 or is_palette:
            asset_info["type"] = "raw"
            asset_info["size"] = 0

            if is_palette or 12 <= i <= 20:
                # These are palette data (arrays of GBA RGB555 colors)
                # Determine size from spacing between entries
                idx_in_unique = unique_offsets.index(offset)
                if idx_in_unique + 1 < len(unique_offsets):
                    size = unique_offsets[idx_in_unique + 1] - offset
                else:
                    size = 512  # default: 256 colors * 2 bytes
                size = min(size, 512)

                pal_data = rom[offset:offset + size]
                num_colors = size // 2
                palette = parse_palette(rom, offset, num_colors)

                name = f"palette_{i:02d}"
                bin_path = os.path.join(OUT_DIR, "palettes", f"{name}.bin")
                png_path = os.path.join(OUT_DIR, "palettes", f"{name}.png")

                # Save raw binary
                os.makedirs(os.path.dirname(bin_path), exist_ok=True)
                with open(bin_path, 'wb') as f:
                    f.write(pal_data)

                # Render palette swatch
                swatch_w = min(num_colors, 16) * 16
                swatch_h = math.ceil(num_colors / 16) * 16
                img = Image.new('RGB', (swatch_w, swatch_h), (0, 0, 0))
                pixels = img.load()
                for ci, color in enumerate(palette):
                    cx = (ci % 16) * 16
                    cy = (ci // 16) * 16
                    for dy in range(16):
                        for dx in range(16):
                            if cx + dx < swatch_w and cy + dy < swatch_h:
                                pixels[cx + dx, cy + dy] = color
                img.save(png_path)

                asset_info["type"] = "palette"
                asset_info["num_colors"] = num_colors
                asset_info["size"] = size
                asset_info["bin"] = bin_path
                asset_info["png"] = png_path
                print(f"  GFX[{i:2d}] palette ({num_colors} colors) -> {png_path}")
            else:
                # Raw tile data or empty
                print(f"  GFX[{i:2d}] raw/empty at 0x{offset:06X}")
                asset_info["type"] = "raw_tiles"

            manifest["assets"].append(asset_info)
            continue

        # Compressed asset - extract blob and decompress
        asset_info["compression"] = "huffman+lz77" if is_huffman else "lz77"
        asset_info["decomp_size"] = decomp_size

        try:
            result, _, _ = decompress_asset(rom, offset)
        except Exception as e:
            print(f"  GFX[{i:2d}] FAILED: {e}")
            asset_info["error"] = str(e)
            manifest["assets"].append(asset_info)
            continue

        # Determine compressed blob size by finding next unique offset
        idx_in_unique = unique_offsets.index(offset)
        if idx_in_unique + 1 < len(unique_offsets):
            comp_size = unique_offsets[idx_in_unique + 1] - offset
        else:
            comp_size = len(rom) - offset

        # Save compressed binary
        name = f"gfx_{i:02d}"
        subdir = "backgrounds"
        bin_path = os.path.join(OUT_DIR, subdir, f"{name}.bin")
        png_path = os.path.join(OUT_DIR, subdir, f"{name}.png")

        os.makedirs(os.path.dirname(bin_path), exist_ok=True)
        with open(bin_path, 'wb') as f:
            f.write(rom[offset:offset + comp_size])

        # Skip 4-byte asset header (metadata skipped by game's DMA copy)
        tile_data = result[4:]

        # Render as 4bpp tile sheet (16 tiles wide = 128px, matches GBA charblock layout)
        img = render_4bpp_tiles(tile_data, GRAYSCALE_4BPP, tiles_per_row=16)
        if img:
            img.save(png_path)

        asset_info["comp_size"] = comp_size
        asset_info["bin"] = bin_path
        asset_info["png"] = png_path if img else None
        manifest["assets"].append(asset_info)
        print(f"  GFX[{i:2d}] {decomp_size:6d} bytes ({asset_info['compression']}) -> {png_path}")


def extract_tileset_data(rom, manifest):
    """Extract assets from ROM_TILESET_TABLE.

    Each entry points to a sub-array of {count, ptr} pairs describing
    tileset animation frames/layers.
    """
    print("\n=== Extracting Tileset Data ===")

    # Read table entries
    entries = []
    pos = ROM_TILESET_TABLE
    while True:
        val = struct.unpack_from('<I', rom, pos)[0]
        if val < 0x08000000 or val > 0x08400000:
            break
        entries.append(val - 0x08000000)
        pos += 4

    print(f"Found {len(entries)} tileset entries")

    # For each tileset entry, use entry boundaries to limit sub-array size
    for i, offset in enumerate(entries):
        # Boundary: next entry or reasonable max
        if i + 1 < len(entries):
            max_bytes = entries[i + 1] - offset
        else:
            max_bytes = 2048
        max_sub = min(max_bytes // 8, 256)

        sub_pos = offset
        sub_entries = []
        for j in range(max_sub):
            count = struct.unpack_from('<I', rom, sub_pos)[0]
            ptr = struct.unpack_from('<I', rom, sub_pos + 4)[0]
            if count > 100 or ptr < 0x08000000 or ptr > 0x08400000:
                break
            sub_entries.append((count, ptr - 0x08000000))
            sub_pos += 8

        if i < 5 or i % 30 == 0:
            print(f"  Tileset[{i:3d}] at 0x{offset:06X}: {len(sub_entries)} sub-entries")

        asset_info = {
            "table": "TILESET_TABLE",
            "index": i,
            "rom_offset": f"0x{offset:06X}",
            "sub_entries": len(sub_entries),
        }
        manifest["assets"].append(asset_info)

    print(f"  ... ({len(entries)} total tilesets)")


def extract_sprite_frames(rom, manifest):
    """Extract assets from ROM_SPRITE_FRAME_TABLE.

    Each entry is {count, ptr} where ptr points to sprite frame metadata.
    """
    print("\n=== Extracting Sprite Frame Data ===")

    entries = []
    pos = ROM_SPRITE_FRAME_TABLE
    for i in range(500):
        count = struct.unpack_from('<I', rom, pos)[0]
        ptr = struct.unpack_from('<I', rom, pos + 4)[0]
        if count > 1000 or (count == 0 and ptr == 0):
            break
        entries.append((count, ptr - 0x08000000 if ptr >= 0x08000000 else ptr))
        pos += 8

    print(f"Found {len(entries)} sprite frame entries")

    asset_info = {
        "table": "SPRITE_FRAME_TABLE",
        "total_entries": len(entries),
        "note": "Sprite frame metadata (OAM attributes, not raw tile data)",
    }
    manifest["assets"].append(asset_info)


def main():
    rom_path = sys.argv[1] if len(sys.argv) > 1 else ROM_PATH

    if not os.path.exists(rom_path):
        print(f"ERROR: ROM not found at {rom_path}")
        sys.exit(1)

    with open(rom_path, 'rb') as f:
        rom = f.read()

    print(f"ROM: {rom_path} ({len(rom)} bytes)")
    os.makedirs(OUT_DIR, exist_ok=True)

    manifest = {
        "rom": rom_path,
        "rom_size": len(rom),
        "assets": [],
    }

    extract_gfx_assets(rom, manifest)
    extract_composed_backgrounds(rom, manifest)
    extract_tileset_data(rom, manifest)
    extract_sprite_frames(rom, manifest)

    # Write manifest
    manifest_path = os.path.join(OUT_DIR, "manifest.json")
    with open(manifest_path, 'w') as f:
        json.dump(manifest, f, indent=2)
    print(f"\nManifest written to {manifest_path}")
    print(f"Total assets catalogued: {len(manifest['assets'])}")


if __name__ == '__main__':
    main()
