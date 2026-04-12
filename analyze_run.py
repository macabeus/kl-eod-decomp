#!/usr/bin/env python3
"""Analyze mizuchi run results for token usage, cost, timing, and infrastructure issues."""

import json
import sys

JSON_PATH = "/Users/macabeus/ApenasMeu/decompiler/klonoa-empire-of-dreams/run-results-2026-03-20T02-58-23.json"

def main():
    with open(JSON_PATH) as f:
        data = json.load(f)

    results = data["results"]

    # Accumulators
    total_input = 0
    total_output = 0
    total_cache_read = 0
    total_cache_creation = 0
    total_cost = 0.0

    total_duration_ms = 0
    total_api_ms = 0
    total_ttft_ms = 0
    ttft_count_valid = 0  # non-timed-out attempts with ttft > 0

    ttft_timeouts = 0
    soft_timeouts = 0
    stall_detections = 0

    total_attempts = 0
    wasted_attempts = 0  # TTFT timeouts (0 useful output)

    per_function = []  # list of dicts

    for result in results:
        func_name = result["functionName"]
        func_input = 0
        func_output = 0
        func_cache_read = 0
        func_cache_creation = 0
        func_cost = 0.0
        func_duration_ms = 0
        func_api_ms = 0
        func_attempts = 0
        func_ttft_timeouts = 0
        func_soft_timeouts = 0
        func_stalls = 0
        func_success = result.get("success", False)

        for attempt in result["attempts"]:
            func_attempts += 1
            total_attempts += 1

            # Find claude-runner plugin result
            cr = None
            for pr in attempt["pluginResults"]:
                if pr["pluginId"] == "claude-runner":
                    cr = pr
                    break

            if cr is None:
                continue

            d = cr["data"]

            # Token usage (sum across all models in the dict)
            for model, usage in d.get("tokenUsage", {}).items():
                inp = usage.get("inputTokens", 0)
                out = usage.get("outputTokens", 0)
                cr_read = usage.get("cacheReadInputTokens", 0)
                cr_create = usage.get("cacheCreationInputTokens", 0)
                cost = usage.get("costUsd", 0.0)

                total_input += inp
                total_output += out
                total_cache_read += cr_read
                total_cache_creation += cr_create
                total_cost += cost

                func_input += inp
                func_output += out
                func_cache_read += cr_read
                func_cache_creation += cr_create
                func_cost += cost

            # Timing
            qt = d.get("queryTiming", {})
            dur = qt.get("durationMs", 0)
            api = qt.get("durationApiMs", 0)
            total_duration_ms += dur
            total_api_ms += api
            func_duration_ms += dur
            func_api_ms += api

            ttft = d.get("ttftMs", 0)
            ttft_timed_out = d.get("ttftTimedOut", False)
            soft_timeout = d.get("softTimeoutTriggered", False)
            stall = d.get("stallDetected", False)

            if ttft_timed_out:
                ttft_timeouts += 1
                func_ttft_timeouts += 1
                wasted_attempts += 1
            else:
                if ttft and ttft > 0:
                    total_ttft_ms += ttft
                    ttft_count_valid += 1

            if soft_timeout:
                soft_timeouts += 1
                func_soft_timeouts += 1

            if stall:
                stall_detections += 1
                func_stalls += 1

        per_function.append({
            "name": func_name,
            "success": func_success,
            "attempts": func_attempts,
            "input_tokens": func_input,
            "output_tokens": func_output,
            "cache_read": func_cache_read,
            "cache_creation": func_cache_creation,
            "cost_usd": func_cost,
            "duration_ms": func_duration_ms,
            "api_ms": func_api_ms,
            "ttft_timeouts": func_ttft_timeouts,
            "soft_timeouts": func_soft_timeouts,
            "stalls": func_stalls,
        })

    # Derived metrics
    avg_ttft_ms = total_ttft_ms / ttft_count_valid if ttft_count_valid > 0 else 0
    avg_tokens_per_sec = (total_output / (total_api_ms / 1000.0)) if total_api_ms > 0 else 0
    overhead_ms = total_duration_ms - total_api_ms

    # Print report
    sep = "=" * 72

    print(sep)
    print("  MIZUCHI RUN ANALYSIS")
    print(f"  File: {JSON_PATH.split('/')[-1]}")
    print(f"  Functions: {len(results)}  |  Total attempts: {total_attempts}")
    print(sep)

    print()
    print("1. TOKEN USAGE (all attempts, all models)")
    print("-" * 50)
    print(f"  Input tokens:          {total_input:>12,}")
    print(f"  Output tokens:         {total_output:>12,}")
    print(f"  Cache read tokens:     {total_cache_read:>12,}")
    print(f"  Cache creation tokens: {total_cache_creation:>12,}")
    total_all = total_input + total_output + total_cache_read + total_cache_creation
    print(f"  ---")
    print(f"  Grand total tokens:    {total_all:>12,}")

    print()
    print("2. TOTAL COST")
    print("-" * 50)
    print(f"  Total cost:  ${total_cost:,.4f} USD")
    avg_cost_per_attempt = total_cost / total_attempts if total_attempts > 0 else 0
    avg_cost_per_function = total_cost / len(results) if results else 0
    print(f"  Avg cost/attempt:   ${avg_cost_per_attempt:,.4f}")
    print(f"  Avg cost/function:  ${avg_cost_per_function:,.4f}")

    print()
    print("3. DURATION BREAKDOWN")
    print("-" * 50)
    print(f"  Total pipeline duration: {total_duration_ms / 1000:>10,.1f} s  ({total_duration_ms / 60000:>6.1f} min)")
    print(f"  Total API duration:      {total_api_ms / 1000:>10,.1f} s  ({total_api_ms / 60000:>6.1f} min)")
    print(f"  Total TTFT (valid):      {total_ttft_ms / 1000:>10,.1f} s  ({total_ttft_ms / 60000:>6.1f} min)")
    print(f"  Overhead (pipeline-API): {overhead_ms / 1000:>10,.1f} s  ({overhead_ms / 60000:>6.1f} min)")

    print()
    print("4. TTFT TIMEOUTS")
    print("-" * 50)
    print(f"  TTFT timeouts: {ttft_timeouts} / {total_attempts} attempts ({100*ttft_timeouts/total_attempts:.1f}%)")

    print()
    print("5. SOFT TIMEOUTS")
    print("-" * 50)
    print(f"  Soft timeouts triggered: {soft_timeouts} / {total_attempts} attempts ({100*soft_timeouts/total_attempts:.1f}%)")

    print()
    print("6. STALL DETECTIONS")
    print("-" * 50)
    print(f"  Stalls detected: {stall_detections} / {total_attempts} attempts ({100*stall_detections/total_attempts:.1f}%)")

    print()
    print("7. PER-FUNCTION SUMMARY")
    print("-" * 100)
    header = f"  {'Function':<30} {'OK':>3} {'Att':>4} {'OutTok':>9} {'Cost($)':>9} {'Dur(s)':>8} {'TTFT-TO':>7} {'SoftTO':>6} {'Stall':>5}"
    print(header)
    print("  " + "-" * 96)
    for fn in sorted(per_function, key=lambda x: -x["cost_usd"]):
        ok = "Y" if fn["success"] else "N"
        print(
            f"  {fn['name']:<30} {ok:>3} {fn['attempts']:>4} "
            f"{fn['output_tokens']:>9,} ${fn['cost_usd']:>8.2f} "
            f"{fn['duration_ms']/1000:>8.1f} {fn['ttft_timeouts']:>7} "
            f"{fn['soft_timeouts']:>6} {fn['stalls']:>5}"
        )

    print()
    print("8. THROUGHPUT")
    print("-" * 50)
    print(f"  Total output tokens:     {total_output:>12,}")
    print(f"  Total API duration:      {total_api_ms / 1000:>12,.1f} s")
    print(f"  Avg output tokens/sec:   {avg_tokens_per_sec:>12,.1f}")

    print()
    print("9. AVERAGE TTFT (non-timed-out attempts)")
    print("-" * 50)
    print(f"  Valid TTFT measurements:  {ttft_count_valid}")
    print(f"  Average TTFT:             {avg_ttft_ms / 1000:.2f} s  ({avg_ttft_ms:.0f} ms)")

    print()
    print("10. WASTED ATTEMPTS (infrastructure issues)")
    print("-" * 50)
    print(f"  TTFT timeout attempts (0 useful output): {wasted_attempts} / {total_attempts} ({100*wasted_attempts/total_attempts:.1f}%)")
    # Estimate wasted cost and time: attempts where ttft timed out
    wasted_cost = 0.0
    wasted_tokens = 0
    wasted_time_ms = 0
    for result in results:
        for attempt in result["attempts"]:
            for pr in attempt["pluginResults"]:
                if pr["pluginId"] == "claude-runner" and pr["data"].get("ttftTimedOut", False):
                    d = pr["data"]
                    for model, usage in d.get("tokenUsage", {}).items():
                        wasted_cost += usage.get("costUsd", 0.0)
                        wasted_tokens += usage.get("inputTokens", 0) + usage.get("cacheReadInputTokens", 0) + usage.get("cacheCreationInputTokens", 0)
                    wasted_time_ms += d.get("queryTiming", {}).get("durationMs", 0)
    print(f"  Cost on wasted attempts:                 ${wasted_cost:,.4f}")
    print(f"  Input+cache tokens on wasted attempts:   {wasted_tokens:,}")
    print(f"  Time wasted on TTFT timeouts:            {wasted_time_ms/1000:,.1f} s  ({wasted_time_ms/60000:.1f} min)")
    print()
    print("  Note: TTFT timeouts report 0 tokens and $0 cost from the API (the")
    print("  request returned no output), but each consumed ~180s of wall time.")

    print()
    print(sep)
    print("  END OF REPORT")
    print(sep)


if __name__ == "__main__":
    main()
