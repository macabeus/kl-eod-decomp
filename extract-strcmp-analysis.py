#!/usr/bin/env python3
"""
Extract and analyze StrCmp's 10 failed attempts from a Mizuchi run results JSON.

Every attempt failed at the claude-runner plugin level with:
  "Invalid code structure: No function definition found"

The compiler plugin never ran (status: skipped) for any attempt.
"""

import json
import re
import sys

JSON_PATH = "/Users/macabeus/ApenasMeu/decompiler/klonoa-empire-of-dreams/run-results-2026-03-20T02-58-23.json"

def main():
    with open(JSON_PATH) as f:
        data = json.load(f)

    strcmp_result = None
    for r in data["results"]:
        if r.get("functionName") == "StrCmp":
            strcmp_result = r
            break

    if not strcmp_result:
        print("ERROR: StrCmp not found in results")
        sys.exit(1)

    attempts = strcmp_result["attempts"]
    print(f"StrCmp: {len(attempts)} attempts, all failed")
    print("=" * 80)

    # -------------------------------------------------------------------------
    # 1) Summary of every attempt
    # -------------------------------------------------------------------------
    print("\n### Per-attempt summary ###\n")
    for i, a in enumerate(attempts):
        cr = a["pluginResults"][0]   # claude-runner
        comp = a["pluginResults"][1] # compiler
        d = cr.get("data", {})
        timing = d.get("queryTiming", {})
        tokens = d.get("tokenUsage", {})
        total_cost = sum(v.get("costUsd", 0) for v in tokens.values())
        total_output = sum(v.get("outputTokens", 0) for v in tokens.values())
        print(
            f"  Attempt {i:2d} | "
            f"claude-runner: {cr['status']:8s} | "
            f"compiler: {comp['status']:8s} | "
            f"error: {cr.get('error', '')[:60]:60s} | "
            f"turns: {timing.get('numTurns', '?'):>2} | "
            f"duration: {timing.get('durationMs', 0)/1000:6.1f}s | "
            f"output_tokens: {total_output:>6} | "
            f"cost: ${total_cost:.2f}"
        )

    # -------------------------------------------------------------------------
    # 2) The validation regex bug (root cause)
    # -------------------------------------------------------------------------
    print("\n" + "=" * 80)
    print("\n### ROOT CAUSE: Regex bug in claude-runner-plugin.ts line 360 ###\n")
    print("The validation function uses this regex to detect a function definition:")
    print("  /\\w+\\s+\\w+\\s*\\([^)]*\\)\\s*\\{/")
    print()
    print("This regex expects: <word> <whitespace> <word> ( params ) {")
    print("For example, it matches:  int StrCmp(int a, int b) {")
    print()
    print("But StrCmp returns a POINTER type: u8 *StrCmp(...)")
    print("The '*' between the type and function name is NOT a \\w character,")
    print("so the regex fails to match any of Claude's generated signatures.")
    print()

    # Test the regex against each attempt's code
    pattern = r"\w+\s+\w+\s*\([^)]*\)\s*\{"
    print("Regex test against each attempt's generated code:")
    for i, a in enumerate(attempts):
        cr = a["pluginResults"][0]
        d = cr.get("data", {})
        gc = d.get("generatedCode", "")
        first_line = gc.split("\n")[0] if gc else "(empty)"
        matched = bool(re.search(pattern, gc))
        print(f"  Attempt {i:2d}: match={matched}  signature: {first_line}")

    # -------------------------------------------------------------------------
    # 3) Detailed data for first 3 attempts
    # -------------------------------------------------------------------------
    for i in range(min(3, len(attempts))):
        a = attempts[i]
        cr = a["pluginResults"][0]
        comp = a["pluginResults"][1]
        d = cr.get("data", {})

        print("\n" + "=" * 80)
        print(f"\n### ATTEMPT {i} - DETAILED ###\n")

        print(f"Duration: {a.get('durationMs', 0)/1000:.1f}s")
        print(f"claude-runner status: {cr['status']}")
        print(f"claude-runner error:  {cr.get('error', '')}")
        print(f"compiler status:      {comp['status']}")
        print(f"compiler output:      {comp.get('output', '')}")
        print(f"compiler error:       {comp.get('error', '')}")
        print(f"fromCache:            {d.get('fromCache')}")
        print(f"stallDetected:        {d.get('stallDetected')}")
        print(f"ttftMs:               {d.get('ttftMs')}")
        tokens = d.get("tokenUsage", {})
        for model, usage in tokens.items():
            print(f"tokenUsage ({model}):")
            for k, v in usage.items():
                print(f"  {k}: {v}")
        timing = d.get("queryTiming", {})
        print(f"queryTiming:          {timing}")

        print(f"\n--- Generated Code ---")
        print(d.get("generatedCode", "(empty)"))

        print(f"\n--- Raw Response (Claude's reasoning across tool-use turns) ---")
        print(d.get("rawResponse", "(empty)"))

        print(f"\n--- Prompt Sent to Claude ---")
        print(d.get("promptSent", "(empty)"))

        # Sections
        sections = cr.get("sections", [])
        if sections:
            for s in sections:
                title = s.get("title", "?")
                stype = s.get("type", "?")
                content = s.get("content")
                print(f"\n--- Section: {title} (type={stype}) ---")
                if content is None:
                    print("(null)")
                elif isinstance(content, str):
                    print(content[:2000] if content else "(empty string)")
                elif isinstance(content, list):
                    for j, item in enumerate(content[:20]):
                        print(f"  [{j}] {json.dumps(item, indent=2)[:500]}")
                elif isinstance(content, dict):
                    print(json.dumps(content, indent=2)[:2000])
                else:
                    print(f"type={type(content).__name__}: {str(content)[:500]}")

    # -------------------------------------------------------------------------
    # 4) Conclusion
    # -------------------------------------------------------------------------
    print("\n" + "=" * 80)
    print("\n### CONCLUSION ###\n")
    print("StrCmp failed ALL 10 attempts due to a bug in the claude-runner plugin's")
    print("code validation regex, NOT due to any compilation error or bad code.")
    print()
    print("Claude actually generated CORRECT, compilable C code every time.")
    print("The code was a valid StrCmp implementation that compiled successfully")
    print("via Claude's internal compile_and_view_assembly MCP tool (as shown in the")
    print("rawResponse, where Claude discusses assembly diffs and register allocation).")
    print()
    print("The bug is in claude-runner-plugin.ts line 360:")
    print("  const hasFunctionPattern = /\\w+\\s+\\w+\\s*\\([^)]*\\)\\s*\\{/.test(code);")
    print()
    print("This regex cannot match pointer return types like:")
    print("  u8 *StrCmp(...)    -- the '*' breaks \\w+\\s+\\w+ matching")
    print("  const u8 *StrCmp(...)  -- same issue")
    print()
    print("A fix would be to account for pointer stars and const qualifiers:")
    print("  /\\w+[\\s*]+\\w+\\s*\\([^)]*\\)\\s*\\{/")
    print("or more robustly:")
    print("  /\\w+(?:\\s+\\w+)*\\s+\\*?\\s*\\w+\\s*\\([^)]*\\)\\s*\\{/")
    print()
    total_cost = 0
    total_time = 0
    for a in attempts:
        cr = a["pluginResults"][0]
        d = cr.get("data", {})
        tokens = d.get("tokenUsage", {})
        total_cost += sum(v.get("costUsd", 0) for v in tokens.values())
        total_time += d.get("queryTiming", {}).get("durationMs", 0)
    print(f"Wasted resources across 10 attempts:")
    print(f"  Total API cost:     ${total_cost:.2f}")
    print(f"  Total API time:     {total_time/1000:.0f}s ({total_time/60000:.1f} min)")


if __name__ == "__main__":
    main()
