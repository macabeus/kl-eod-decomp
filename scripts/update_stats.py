#!/usr/bin/env python3
"""
Compute decomp stats from source files and TOML, then update website and CLAUDE.md.

Reads:
  - klonoa-eod-decomp.toml  (module list, renames)
  - src/*.c                  (INCLUDE_ASM counts, C function definitions)

Updates:
  - CLAUDE.md               (module function counts)
  - gh-pages: index.html, matching.html, game-engine.html

Usage:
  python3 scripts/update_stats.py              # compute + update files
  python3 scripts/update_stats.py --dry-run    # just print stats
"""

import json
import os
import re
import subprocess
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DECOMP_TOML = os.path.join(ROOT, "klonoa-eod-decomp.toml")
SRC_DIR = os.path.join(ROOT, "src")
GLOBALS_H = os.path.join(ROOT, "include", "globals.h")

# Modules that have source files (excludes libgcc which has no .c file)
SRC_MODULES = [
    "system", "math", "engine", "code_0", "code_1",
    "code_3", "gfx", "m4a", "syscalls", "util",
]


def load_toml():
    with open(DECOMP_TOML, "rb") as f:
        config = tomllib.load(f)
    renames = config.get("renames", {})
    return config, renames


def count_include_asm(filepath):
    """Count INCLUDE_ASM macros in a C file."""
    with open(filepath) as f:
        return sum(1 for line in f if "INCLUDE_ASM" in line)


def count_c_functions(filepath):
    """Count C function definitions (not forward declarations, not INCLUDE_ASM)."""
    with open(filepath) as f:
        lines = f.readlines()

    # Types that can start a function definition
    type_pattern = re.compile(
        r"^(void|u8|u16|u32|s8|s16|s32|int|static|const|bool|IWRAM_CODE)\b"
    )

    count = 0
    for i, line in enumerate(lines):
        stripped = line.strip()
        # Skip empty, comments, preprocessor, INCLUDE_ASM
        if not stripped or stripped.startswith("//") or stripped.startswith("#"):
            continue
        if "INCLUDE_ASM" in line:
            continue
        # Must start with a type keyword
        if not type_pattern.match(stripped):
            continue
        # Must contain a parenthesis (function signature)
        if "(" not in stripped:
            continue
        # Must NOT be a forward declaration (ending with ;)
        if stripped.endswith(";"):
            continue
        # Check this line or next line has {
        has_brace = "{" in stripped
        if not has_brace and i + 1 < len(lines):
            has_brace = "{" in lines[i + 1]
        if has_brace:
            count += 1

    return count


def get_function_names(filepath):
    """Extract all function names from a C source file (INCLUDE_ASM + definitions)."""
    with open(filepath) as f:
        text = f.read()

    names = []

    # INCLUDE_ASM names
    for m in re.finditer(r"INCLUDE_ASM\([^,]+,\s*(\w+)\)", text):
        names.append(m.group(1))

    # C function definition names
    type_pattern = re.compile(
        r"^(void|u8|u16|u32|s8|s16|s32|int|static|const|bool|IWRAM_CODE)\b"
    )
    lines = text.split("\n")
    for i, line in enumerate(lines):
        stripped = line.strip()
        if not stripped or stripped.startswith("//") or stripped.startswith("#"):
            continue
        if "INCLUDE_ASM" in line:
            continue
        if not type_pattern.match(stripped) or "(" not in stripped:
            continue
        if stripped.endswith(";"):
            continue
        has_brace = "{" in stripped
        if not has_brace and i + 1 < len(lines):
            has_brace = "{" in lines[i + 1]
        if has_brace:
            m = re.search(r"(\w+)\s*\(", stripped)
            if m:
                names.append(m.group(1))

    return names


def compute_stats():
    config, renames = load_toml()

    # Per-module stats
    modules = {}
    all_src_names = []
    for mod in SRC_MODULES:
        src_path = os.path.join(SRC_DIR, f"{mod}.c")
        if not os.path.exists(src_path):
            continue
        asm_count = count_include_asm(src_path)
        c_count = count_c_functions(src_path)
        names = get_function_names(src_path)
        all_src_names.extend(names)
        total = asm_count + c_count
        matched = c_count
        modules[mod] = {
            "total": total,
            "matched": matched,
            "unmatched": asm_count,
        }

    # Named = source functions that have semantic names (no FUN_/sub_ prefix)
    named_functions = sum(
        1 for n in all_src_names
        if not n.startswith("FUN_") and not n.startswith("sub_")
    )
    unnamed_functions = sum(
        1 for n in all_src_names
        if n.startswith("FUN_") or n.startswith("sub_")
    )

    # Count named globals from globals.h
    iwram_globals = 0
    rom_globals = 0
    if os.path.exists(GLOBALS_H):
        with open(GLOBALS_H) as f:
            globals_text = f.read()
        iwram_globals = len(re.findall(
            r"#define\s+\w+\s+.*0x03[0-9A-Fa-f]+", globals_text
        ))
        rom_globals = len(re.findall(
            r"#define\s+\w+\s+.*0x08[0-9A-Fa-f]+", globals_text
        ))

    # TOML rename stats (for reference)
    total_renames = len(renames)

    total_functions = sum(m["total"] for m in modules.values())
    total_matched = sum(m["matched"] for m in modules.values())
    total_unmatched = sum(m["unmatched"] for m in modules.values())

    pct = (total_matched / total_functions * 100) if total_functions else 0

    return {
        "modules": modules,
        "total_functions": total_functions,
        "total_matched": total_matched,
        "total_unmatched": total_unmatched,
        "match_pct": round(pct, 1),
        "named_functions": named_functions,
        "unnamed_functions": unnamed_functions,
        "toml_renames": total_renames,
        "iwram_globals": iwram_globals,
        "rom_globals": rom_globals,
    }


def update_claude_md(stats):
    """Update CLAUDE.md module counts."""
    claude_md = os.path.join(ROOT, "CLAUDE.md")
    with open(claude_md) as f:
        text = f.read()

    # Build new modules line
    parts = []
    for mod in SRC_MODULES:
        s = stats["modules"][mod]
        parts.append(f"`{mod}.c` ({s['total']} funcs)")
    new_line = "**Modules** (src/): " + ", ".join(parts) + "."

    # Replace the existing Modules line (match to end of line)
    text = re.sub(
        r"\*\*Modules\*\* \(src/\):.*",
        new_line,
        text,
    )

    with open(claude_md, "w") as f:
        f.write(text)
    print(f"  Updated {claude_md}")


def get_gh_pages_file(filename):
    """Read a file from gh-pages branch."""
    result = subprocess.run(
        ["git", "show", f"gh-pages:{filename}"],
        capture_output=True, text=True, cwd=ROOT,
    )
    if result.returncode != 0:
        print(f"  WARNING: could not read gh-pages:{filename}")
        return None
    return result.stdout


def write_gh_pages_file(filename, content):
    """Write updated content to gh-pages branch via a temp checkout."""
    # We'll collect updates and apply them all at once
    pass


def update_index_html(text, stats):
    """Update index.html stats."""
    total = stats["total_functions"]
    matched = stats["total_matched"]
    named = stats["named_functions"]
    pct = stats["match_pct"]

    # Update progress bar width and text
    text = re.sub(
        r'style="width: [\d.]+%;">[\d.]+%',
        f'style="width: {pct}%;">{pct}%',
        text,
    )

    # Update progress label
    iwram = stats["iwram_globals"]
    rom = stats["rom_globals"]
    text = re.sub(
        r"\d+ of \d+ functions matched.*?</p>",
        f"{matched} of {total} functions matched &bull; "
        f"{named} named &bull; {iwram} IWRAM + {rom} ROM globals defined</p>",
        text,
    )

    # Update stat cards - Total Functions
    text = re.sub(
        r'(<div class="stat-value">)\d+(</div>\s*<div class="stat-label">Total Functions)',
        rf"\g<1>{total}\2",
        text,
    )

    # Update stat cards - Matched
    text = re.sub(
        r'(<div class="stat-value">)\d+(</div>\s*<div class="stat-label">Matched)',
        rf"\g<1>{matched}\2",
        text,
    )

    # Update stat cards - Named
    text = re.sub(
        r'(<div class="stat-value">)\d+(</div>\s*<div class="stat-label">Named)',
        rf"\g<1>{named}\2",
        text,
    )

    # Rename "Named" label to "Named Functions" for clarity
    text = text.replace(
        '<div class="stat-label">Named</div>',
        '<div class="stat-label">Named Functions</div>',
    )

    # Update any "all 463 functions" text
    text = re.sub(r"all \d+ functions", f"all {total} functions", text)

    return text


def update_matching_html(text, stats):
    """Update matching.html stats."""
    total = stats["total_functions"]
    matched = stats["total_matched"]
    unmatched = stats["total_unmatched"]
    named = stats["named_functions"]

    # Update intro paragraph: "Of 463 functions, 52 are fully matched"
    text = re.sub(
        r"Of \d+ functions,\s*\n?\s*\d+ are fully matched\. The remaining \d+ are",
        f"Of {total} functions,\n            {matched} are fully matched. "
        f"The remaining {unmatched} are",
        text,
    )

    # Update "all N have semantic names"
    text = re.sub(
        r"all \d+ have semantic names",
        f"all {named} have semantic names",
        text,
    )

    # Update "N frequently-referenced IWRAM globals and ROM data tables"
    iwram = stats["iwram_globals"]
    rom = stats["rom_globals"]
    text = re.sub(
        r"\d+ frequently-referenced\s+IWRAM globals and ROM data tables have been",
        f"{iwram} IWRAM globals and {rom} ROM data tables have been",
        text,
    )

    # Update module table rows
    for mod in SRC_MODULES:
        if mod == "syscalls":
            continue  # syscalls not in matching table
        s = stats["modules"][mod]
        # Match pattern like: <td>system.c</td><td>4 of 14 matched</td>
        # or <td>math.c</td><td>0 of 4</td>
        text = re.sub(
            rf"(<td>{mod}\.c</td><td>)\d+ of \d+( matched)?",
            rf"\g<1>{s['matched']} of {s['total']} matched",
            text,
        )
        # Also match without .c extension: <td>system</td><td>4 of 14
        text = re.sub(
            rf"(<td>{mod}</td><td>)\d+ of \d+",
            rf"\g<1>{s['matched']} of {s['total']}",
            text,
        )

    # Update "all 463 functions" references
    text = re.sub(r"all \d+ functions", f"all {total} functions", text)

    # Update "Approximate categorization of all 463 functions"
    text = re.sub(
        r"categorization of all \d+ functions",
        f"categorization of all {total} functions",
        text,
    )

    # Update "Matched to C" count in breakdown table
    text = re.sub(
        r"(Matched to C</td><td>)\d+",
        rf"\g<1>{matched}",
        text,
    )

    # Update "~411 functions" (remaining blocked)
    text = re.sub(
        r"remaining ~?\d+ functions",
        f"remaining ~{unmatched} functions",
        text,
    )

    return text


def update_game_engine_html(text, stats):
    """Update game-engine.html stats."""
    total = stats["total_functions"]
    named = stats["named_functions"]

    # Update "100% of functions now named (475 total)"
    text = re.sub(
        r"functions now named \(\d+ total\)",
        f"functions now named ({named} total)",
        text,
    )

    # Update module table in Code Statistics
    for mod in SRC_MODULES:
        s = stats["modules"][mod]
        # Match: <tr><td>system</td><td>10</td>
        text = re.sub(
            rf"(<tr><td>{mod}</td><td>)\d+(</td>)",
            rf"\g<1>{s['total']}\2",
            text,
        )

    # Update total row
    text = re.sub(
        r"(<td><strong>Total</strong></td><td><strong>)\d+(</strong></td>)",
        rf"\g<1>{total}\2",
        text,
    )

    # Update "all N functions"
    text = re.sub(r"all \d+ functions", f"all {total} functions", text)

    return text


def apply_gh_pages_updates(updates):
    """Apply updates to gh-pages branch files."""
    # Create a temporary worktree for gh-pages
    worktree_dir = os.path.join(ROOT, ".gh-pages-tmp")

    # Clean up any leftover worktree
    if os.path.exists(worktree_dir):
        subprocess.run(
            ["git", "worktree", "remove", "--force", worktree_dir],
            cwd=ROOT, capture_output=True,
        )

    try:
        subprocess.run(
            ["git", "worktree", "add", worktree_dir, "gh-pages"],
            cwd=ROOT, check=True, capture_output=True,
        )

        for filename, content in updates.items():
            filepath = os.path.join(worktree_dir, filename)
            with open(filepath, "w") as f:
                f.write(content)
            print(f"  Updated gh-pages:{filename}")

        # Stage and commit
        subprocess.run(
            ["git", "add"] + list(updates.keys()),
            cwd=worktree_dir, check=True, capture_output=True,
        )

        # Check if there are changes to commit
        result = subprocess.run(
            ["git", "diff", "--cached", "--quiet"],
            cwd=worktree_dir, capture_output=True,
        )
        if result.returncode != 0:
            subprocess.run(
                ["git", "commit", "-m", "Update stats from source (auto-generated)"],
                cwd=worktree_dir, check=True, capture_output=True,
            )
            print("  Committed gh-pages changes")
        else:
            print("  No gh-pages changes needed")

    finally:
        subprocess.run(
            ["git", "worktree", "remove", "--force", worktree_dir],
            cwd=ROOT, capture_output=True,
        )


def main():
    dry_run = "--dry-run" in sys.argv

    os.chdir(ROOT)
    stats = compute_stats()

    # Print summary
    print("=== Decomp Stats ===")
    print(f"Total Functions:    {stats['total_functions']}")
    print(f"Matched:            {stats['total_matched']}")
    print(f"Unmatched:          {stats['total_unmatched']}")
    print(f"Match %:            {stats['match_pct']}%")
    print(f"Named Functions:    {stats['named_functions']} (semantic names in source)")
    print(f"Unnamed Functions:  {stats['unnamed_functions']} (still FUN_/sub_ in source)")
    print(f"TOML Renames:       {stats['toml_renames']}")
    print(f"IWRAM Globals:      {stats['iwram_globals']}")
    print(f"ROM Data Tables:    {stats['rom_globals']}")
    print()

    print("Per-module:")
    for mod in SRC_MODULES:
        s = stats["modules"][mod]
        print(f"  {mod:12s}: {s['matched']:3d} / {s['total']:3d} matched, "
              f"{s['unmatched']:3d} asm")
    print()

    # Verify invariants
    ok = True
    if stats["named_functions"] > stats["total_functions"]:
        print(f"WARNING: Named ({stats['named_functions']}) > "
              f"Total ({stats['total_functions']})")
        ok = False
    if stats["total_matched"] + stats["total_unmatched"] != stats["total_functions"]:
        print("WARNING: Matched + Unmatched != Total")
        ok = False
    if stats["named_functions"] + stats["unnamed_functions"] != stats["total_functions"]:
        print("WARNING: Named + Unnamed != Total")
        ok = False
    if ok:
        print("All invariants OK")
    print()

    if dry_run:
        print(json.dumps(stats, indent=2))
        return

    # Update CLAUDE.md
    print("Updating CLAUDE.md...")
    update_claude_md(stats)

    # Update gh-pages files
    print("Updating gh-pages files...")
    gh_updates = {}

    index_html = get_gh_pages_file("index.html")
    if index_html:
        gh_updates["index.html"] = update_index_html(index_html, stats)

    matching_html = get_gh_pages_file("matching.html")
    if matching_html:
        gh_updates["matching.html"] = update_matching_html(matching_html, stats)

    game_engine_html = get_gh_pages_file("game-engine.html")
    if game_engine_html:
        gh_updates["game-engine.html"] = update_game_engine_html(
            game_engine_html, stats
        )

    if gh_updates:
        apply_gh_pages_updates(gh_updates)

    print("\nDone!")


if __name__ == "__main__":
    main()
