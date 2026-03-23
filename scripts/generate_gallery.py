"""Generate gallery.html for the gh-pages website from extracted graphics.

Reads graphics/manifest.json and produces a static HTML gallery page showing
composed level backgrounds (tile + tilemap + palette), raw tile sheets,
and sprite palettes.

Usage:
    python scripts/generate_gallery.py [output_path]
"""

import json
import os
import sys
from collections import defaultdict

MANIFEST_PATH = "graphics/manifest.json"
DEFAULT_OUTPUT = "gallery.html"

WORLD_DISPLAY = {
    0: "World Map",
    1: "Breezegale",
    2: "Jugpot",
    3: "Forlock",
    4: "Volk",
    5: "Ishras Viel",
    6: "Timber Tracks",
    7: "The Dark",
    8: "EX Levels",
}

LAYER_NAMES = ["BG0 (far background)", "BG1 (mid layer)", "BG2 (near/foreground)"]


def generate_composed_section(composed_assets):
    """Generate HTML for composed level backgrounds grouped by world."""
    by_world = defaultdict(list)
    for a in composed_assets:
        by_world[a["world"]].append(a)

    html = []
    html.append("""
        <div class="section-header">
            <h2>Composed Level Backgrounds</h2>
            <span class="count">162 layers (6 visions &times; 9 worlds &times; 3 BG layers)</span>
        </div>
        <p>
            Level backgrounds composed from three ROM tables: tile charblocks
            (<code>ROM_BG_TILE_TABLE</code>), tilemaps (<code>ROM_BG_TILEMAP_TABLE</code>),
            and palettes (<code>ROM_BG_PALETTE_TABLE</code>). Each image shows tiles
            arranged according to the GBA screenblock tilemap with correct palette
            bank selection and flip flags applied. Filter by world or vision below.
        </p>

        <div class="filter-bar">
            <label>World:</label>
            <select id="world-filter" onchange="filterBackgrounds()">
                <option value="all">All Worlds</option>
""")

    for w in sorted(by_world.keys()):
        name = WORLD_DISPLAY.get(w, f"World {w}")
        html.append(f'                <option value="{w}">{name}</option>')

    html.append("""
            </select>
            <label>Vision:</label>
            <select id="vision-filter" onchange="filterBackgrounds()">
                <option value="all">All Visions</option>
                <option value="1">Vision 1</option>
                <option value="2">Vision 2</option>
                <option value="3">Vision 3</option>
                <option value="4">Vision 4</option>
                <option value="5">Vision 5</option>
                <option value="6">Vision 6</option>
            </select>
            <label>Layer:</label>
            <select id="layer-filter" onchange="filterBackgrounds()">
                <option value="all">All Layers</option>
                <option value="0">BG0 (far)</option>
                <option value="1">BG1 (mid)</option>
                <option value="2">BG2 (near)</option>
            </select>
        </div>

        <div class="gallery-grid" id="bg-gallery">
""")

    # Generate cards sorted by world, then vision, then layer
    for world in sorted(by_world.keys()):
        items = sorted(by_world[world], key=lambda a: (a["vision"], a["layer"]))
        world_name = WORLD_DISPLAY.get(world, f"World {world}")

        for a in items:
            vision = a["vision"]
            layer = a["layer"]
            png = a["png"].replace("graphics/", "gallery/")
            dims = a.get("dimensions", "256x256")
            layer_label = LAYER_NAMES[layer] if layer < len(LAYER_NAMES) else f"Layer {layer}"
            title = f"V{vision} {world_name} — {layer_label}"

            html.append(f"""
            <div class="gallery-card bg-card" data-world="{world}" data-vision="{vision}" data-layer="{layer}">
                <div class="preview" onclick="openLightbox('{png}')">
                    <img src="{png}" alt="{title}" loading="lazy">
                </div>
                <div class="info">
                    <h3>{title}</h3>
                    <div class="meta">
                        <span>{dims}</span>
                    </div>
                </div>
            </div>""")

    html.append("        </div>")
    return "\n".join(html)


def generate_tilesheets_section(gfx_assets):
    """Generate HTML for raw tile sheet assets."""
    compressed = [a for a in gfx_assets if a.get("compression")]
    if not compressed:
        return ""

    html = []
    html.append("""
        <div class="section-header">
            <h2>Raw Tile Sheets</h2>
            <span class="count">""" + str(len(compressed)) + """ compressed assets (Huffman + LZ77)</span>
        </div>
        <p>
            World Map select screens and EX bonus level tile charblocks, shown as
            raw sequential tile grids (not composed with tilemaps). These are loaded
            via <code>ROM_GFX_ASSET_TABLE</code> for non-level screens.
        </p>

        <div class="gallery-grid">
""")

    NAMES = {
        0: "World Map 1 — Bell Hill",
        1: "World Map 2 — Jugpot",
        2: "World Map 3 — Forlock",
        3: "World Map 4 — Volk",
        4: "World Map 5 — Ishras Viel",
        5: "World Map 1 (dup)",
        6: "EX Vision 1",
        7: "EX Vision 2",
        8: "EX Vision 3",
        9: "EX Vision 4",
        10: "EX Vision 5",
        11: "EX Vision 6",
    }

    for a in compressed:
        idx = a["index"]
        name = NAMES.get(idx, f"GFX Asset {idx}")
        png = a.get("png", "").replace("graphics/", "gallery/")
        decomp = a.get("decomp_size", 0)
        comp_type = a.get("compression", "unknown")
        tiles = (decomp - 4) // 32 if decomp > 4 else 0

        html.append(f"""
            <div class="gallery-card">
                <div class="preview" onclick="openLightbox('{png}')">
                    <img src="{png}" alt="{name}" loading="lazy">
                </div>
                <div class="info">
                    <h3>{name}</h3>
                    <div class="meta">
                        <span>{a.get('rom_offset', '')}</span>
                        <span>{decomp:,} B</span>
                        <span>{tiles:,} tiles</span>
                    </div>
                </div>
            </div>""")

    html.append("        </div>")
    return "\n".join(html)


def generate_palettes_section(gfx_assets):
    """Generate HTML for palette assets."""
    palettes = [a for a in gfx_assets if a.get("type") == "palette"]
    if not palettes:
        return ""

    html = []
    html.append(f"""
        <div class="section-header">
            <h2>Sprite Palettes</h2>
            <span class="count">{len(palettes)} OBJ color palettes (GBA RGB555)</span>
        </div>
        <p>
            Sprite (OBJ) palettes loaded into OBJ palette RAM. BG palettes are
            loaded separately from a 54-entry table at <code>0x08188F5C</code>.
        </p>

        <div class="palette-grid">
""")

    for a in palettes:
        idx = a["index"]
        png = a.get("png", "").replace("graphics/", "gallery/")
        num_colors = a.get("num_colors", 16)
        offset = a.get("rom_offset", "")

        html.append(f"""
            <div class="palette-card">
                <img src="{png}" alt="Palette {idx}">
                <div class="info">palette_{idx:02d} ({num_colors} colors) @ {offset}</div>
            </div>""")

    html.append("        </div>")
    return "\n".join(html)


def generate_gallery_html(manifest, output_path):
    """Generate the complete gallery.html page."""
    gfx_assets = [a for a in manifest["assets"] if a.get("table") == "GFX_ASSET_TABLE"]
    composed = [a for a in manifest["assets"] if a.get("table") == "BG_COMPOSED"]

    composed_section = generate_composed_section(composed)
    tilesheet_section = generate_tilesheets_section(gfx_assets)
    palette_section = generate_palettes_section(gfx_assets)

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Graphics Gallery — Klonoa: Empire of Dreams Decomp</title>
    <link rel="stylesheet" href="style.css">
    <style>
        .gallery-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
            gap: 1.5rem;
            margin: 1.5rem 0;
        }}

        .gallery-card {{
            background: var(--bg-secondary);
            border: 1px solid var(--border);
            border-radius: 8px;
            overflow: hidden;
            transition: border-color 0.2s;
        }}

        .gallery-card:hover {{
            border-color: var(--accent);
        }}

        .gallery-card .preview {{
            background: #000;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 1rem;
            min-height: 200px;
            cursor: pointer;
        }}

        .gallery-card .preview img {{
            max-width: 100%;
            max-height: 300px;
            image-rendering: pixelated;
            image-rendering: -moz-crisp-edges;
            image-rendering: crisp-edges;
        }}

        .gallery-card .info {{
            padding: 0.8rem 1rem;
            border-top: 1px solid var(--border);
        }}

        .gallery-card .info h3 {{
            font-size: 0.95rem;
            color: var(--text-heading);
            margin-bottom: 0.3rem;
        }}

        .gallery-card .info .meta {{
            font-size: 0.8rem;
            color: var(--text-secondary);
            font-family: var(--mono);
        }}

        .gallery-card .info .meta span {{
            display: inline-block;
            margin-right: 1rem;
        }}

        .palette-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
            gap: 1rem;
            margin: 1.5rem 0;
        }}

        .palette-card {{
            background: var(--bg-secondary);
            border: 1px solid var(--border);
            border-radius: 8px;
            overflow: hidden;
            padding: 0.8rem;
        }}

        .palette-card img {{
            width: 100%;
            image-rendering: pixelated;
            image-rendering: crisp-edges;
            border-radius: 4px;
        }}

        .palette-card .info {{
            margin-top: 0.5rem;
            font-size: 0.85rem;
            color: var(--text-secondary);
            font-family: var(--mono);
        }}

        .section-header {{
            display: flex;
            align-items: baseline;
            gap: 1rem;
            margin-top: 2rem;
            margin-bottom: 0.5rem;
        }}

        .section-header h2 {{
            margin: 0;
        }}

        .section-header .count {{
            color: var(--text-secondary);
            font-size: 0.9rem;
        }}

        .stats-row {{
            display: flex;
            gap: 2rem;
            flex-wrap: wrap;
            margin: 1rem 0 2rem;
        }}

        .stat-box {{
            background: var(--bg-secondary);
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 1rem 1.5rem;
            text-align: center;
        }}

        .stat-box .value {{
            font-size: 1.8rem;
            font-weight: bold;
            color: var(--accent);
            font-family: var(--mono);
        }}

        .stat-box .label {{
            font-size: 0.85rem;
            color: var(--text-secondary);
            margin-top: 0.2rem;
        }}

        .filter-bar {{
            display: flex;
            align-items: center;
            gap: 0.8rem;
            flex-wrap: wrap;
            margin: 1rem 0;
            padding: 0.8rem 1rem;
            background: var(--bg-secondary);
            border: 1px solid var(--border);
            border-radius: 8px;
        }}

        .filter-bar label {{
            font-size: 0.85rem;
            color: var(--text-secondary);
            font-weight: bold;
        }}

        .filter-bar select {{
            background: var(--bg-primary);
            color: var(--text-primary);
            border: 1px solid var(--border);
            border-radius: 4px;
            padding: 0.3rem 0.5rem;
            font-family: var(--mono);
            font-size: 0.85rem;
        }}

        .gallery-card.hidden {{
            display: none;
        }}

        /* Lightbox */
        .lightbox {{
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0, 0, 0, 0.9);
            z-index: 1000;
            cursor: pointer;
            align-items: center;
            justify-content: center;
        }}

        .lightbox.active {{
            display: flex;
        }}

        .lightbox img {{
            max-width: 95%;
            max-height: 95%;
            image-rendering: pixelated;
            image-rendering: crisp-edges;
        }}
    </style>
</head>
<body>
    <nav>
        <a href="index.html" class="site-title">Klonoa: EoD Decomp</a>
        <ul class="nav-links">
            <li><a href="index.html">Overview</a></li>
            <li><a href="sound.html">Sound Engine</a></li>
            <li><a href="music.html">Music Player</a></li>
            <li><a href="gallery.html" class="active">Graphics</a></li>
        </ul>
    </nav>

    <div class="container">
        <h1>Graphics Gallery</h1>
        <p class="subtitle">Extracted and composed level backgrounds from the ROM</p>

        <p>
            Level backgrounds composed by combining tile charblock data, screenblock
            tilemaps, and RGB555 palettes from three parallel ROM tables. Each GBA
            background layer is rendered with correct tile arrangement, palette bank
            selection, and horizontal/vertical flip flags.
        </p>

        <div class="stats-row">
            <div class="stat-box">
                <div class="value">162</div>
                <div class="label">Composed Backgrounds</div>
            </div>
            <div class="stat-box">
                <div class="value">9</div>
                <div class="label">Worlds</div>
            </div>
            <div class="stat-box">
                <div class="value">6</div>
                <div class="label">Visions per World</div>
            </div>
            <div class="stat-box">
                <div class="value">3</div>
                <div class="label">BG Layers</div>
            </div>
        </div>

{composed_section}

{tilesheet_section}

{palette_section}

        <div class="section-header">
            <h2>Compression Format</h2>
        </div>
        <p>
            The game uses a two-stage compression pipeline implemented in
            <code>FUN_08043af4</code> (DecompressData):
        </p>
        <ul style="margin: 1rem 0 1rem 2rem; line-height: 2;">
            <li>
                <strong>Header word</strong> (4 bytes): Bit 31 = Huffman flag,
                bits 0-30 = decompressed size
            </li>
            <li>
                <strong>If bit 31 set</strong>: 4-bit Huffman
                (<code>HuffUnComp</code> / SWI 0x13) followed by LZ77
                (<code>LZ77UnCompWram</code> / SWI 0x11)
            </li>
            <li>
                <strong>If bit 31 clear</strong>: LZ77 only
            </li>
        </ul>

        <div class="section-header">
            <h2>Data Tables</h2>
        </div>
        <table>
            <thead>
                <tr>
                    <th>Table</th>
                    <th>ROM Address</th>
                    <th>Entries</th>
                    <th>Description</th>
                </tr>
            </thead>
            <tbody>
                <tr>
                    <td><code>ROM_GFX_ASSET_TABLE</code></td>
                    <td><code>0x0818B7AC</code></td>
                    <td>59</td>
                    <td>World map + EX level tiles, sprite palettes, sprite tile buffers</td>
                </tr>
                <tr>
                    <td><code>ROM_BG_TILE_TABLE</code></td>
                    <td><code>0x08189034</code></td>
                    <td>162</td>
                    <td>BG tile charblock data (6 visions &times; 9 worlds &times; 3 layers)</td>
                </tr>
                <tr>
                    <td><code>ROM_BG_TILEMAP_TABLE</code></td>
                    <td><code>0x081892BC</code></td>
                    <td>162</td>
                    <td>BG screenblock tilemaps (parallel to tile table)</td>
                </tr>
                <tr>
                    <td><code>ROM_BG_PALETTE_TABLE</code></td>
                    <td><code>0x08188F5C</code></td>
                    <td>54</td>
                    <td>BG palettes: 256-color RGB555, indexed as (visionID-1)*9 + worldID</td>
                </tr>
                <tr>
                    <td><code>ROM_TILESET_TABLE</code></td>
                    <td><code>0x0818B8E0</code></td>
                    <td>148</td>
                    <td>Tileset animation frame references</td>
                </tr>
                <tr>
                    <td><code>ROM_SPRITE_FRAME_TABLE</code></td>
                    <td><code>0x08078FC8</code></td>
                    <td>500</td>
                    <td>Sprite animation frames (OAM attribute metadata)</td>
                </tr>
            </tbody>
        </table>
    </div>

    <div class="lightbox" id="lightbox" onclick="closeLightbox()">
        <img id="lightbox-img" src="" alt="Full size preview">
    </div>

    <script>
        function openLightbox(src) {{
            document.getElementById('lightbox-img').src = src;
            document.getElementById('lightbox').classList.add('active');
        }}
        function closeLightbox() {{
            document.getElementById('lightbox').classList.remove('active');
        }}
        document.addEventListener('keydown', function(e) {{
            if (e.key === 'Escape') closeLightbox();
        }});

        function filterBackgrounds() {{
            var world = document.getElementById('world-filter').value;
            var vision = document.getElementById('vision-filter').value;
            var layer = document.getElementById('layer-filter').value;
            var cards = document.querySelectorAll('.bg-card');
            cards.forEach(function(card) {{
                var show = true;
                if (world !== 'all' && card.dataset.world !== world) show = false;
                if (vision !== 'all' && card.dataset.vision !== vision) show = false;
                if (layer !== 'all' && card.dataset.layer !== layer) show = false;
                card.classList.toggle('hidden', !show);
            }});
        }}
    </script>
</body>
</html>"""

    with open(output_path, "w") as f:
        f.write(html)

    print(f"Gallery written to {output_path}")
    print(f"  Composed backgrounds: {len(composed)}")
    print(f"  Tile sheets: {len([a for a in gfx_assets if a.get('compression')])}")
    print(f"  Palettes: {len([a for a in gfx_assets if a.get('type') == 'palette'])}")


def main():
    output_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_OUTPUT

    if not os.path.exists(MANIFEST_PATH):
        print(f"ERROR: Manifest not found at {MANIFEST_PATH}")
        print("Run extract_gfx.py first to generate it.")
        sys.exit(1)

    with open(MANIFEST_PATH) as f:
        manifest = json.load(f)

    generate_gallery_html(manifest, output_path)


if __name__ == "__main__":
    main()
