#!/usr/bin/env python3
"""Maintain the AI usage ledger and the badges rendered from it.

Claude Code writes one JSONL transcript per session under ~/.claude/projects/,
and every assistant record carries the exact token counts the API metered. Those
transcripts are local and get pruned, so they are a collection source rather than
a source of truth. This script folds them into `docs/ai-usage.json`, a committed
ledger whose counters only ever grow, then renders `TOKEN_STATS.md` and the
README badge line from that ledger alone: the token totals, their API-equivalent
cost at list prices, and an order-of-magnitude energy and water estimate.

    python tools/token_stats.py            scan transcripts, update the ledger, render
    python tools/token_stats.py --render   rewrite the ledger and the docs, do not scan
    python tools/token_stats.py --check    verify the rendered files match the ledger

Both writing modes rewrite the ledger, so a session added to its `excluded` map
by hand is retired by either one. --check writes nothing and reads nothing but
committed files, which is what lets CI run it.

The ledger never stores an absolute path. Sessions are attributed by the working
directory recorded in their transcript, but only the verdict is written down, so
the file carries no drive letter, home directory, or user name.
"""

import argparse
import collections
import difflib
import glob
import json
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEDGER_JSON = os.path.join(REPO_ROOT, "docs", "ai-usage.json")
STATS_MD = os.path.join(REPO_ROOT, "TOKEN_STATS.md")
README_MD = os.path.join(REPO_ROOT, "README.md")

SCHEMA_VERSION = 1
UNTITLED = "(untitled session)"

# Token classes the transcripts expose, in the order they are stored and shown.
COUNTERS = ("calls", "input", "cacheWrite5m", "cacheWrite1h", "cacheRead", "output")

# Compute weighting per token class, expressed in output-equivalent tokens. A
# fixed proxy for relative serving cost, taken from Anthropic's standard price
# ratios (input 1, 5m write 1.25, 1h write 2, cache read 0.1, output 5). It is
# one table for every model on purpose: the deeper cache-read discounts some
# models carry are pricing decisions, and the energy estimate must not move
# when pricing does. Cost uses PRICES_USD_PER_MTOK instead.
WEIGHTS = {
    "output": 1.0,
    "input": 1 / 5,
    "cacheWrite1h": 2 / 5,
    "cacheWrite5m": 1 / 4,
    "cacheRead": 1 / 50,
}


def _price(base_input, output, read_ratio=0.1):
    """One model's rates from its base input and output price, USD per MTok."""
    return {
        "input": base_input,
        "cacheWrite5m": base_input * 1.25,
        "cacheWrite1h": base_input * 2,
        "cacheRead": base_input * read_ratio,
        "output": output,
    }


# Anthropic first-party list prices in USD per million tokens, as published on
# 2026-11-17. Fast mode and batch pricing are ignored: the cost is a way to read
# the token counts in human terms, not an invoice. Haiku 5.5 is priced at its
# up-to-100k-prompt rate, since the ledger holds no per-request prompt size.
# A model missing from this table stops the render, so add it here first.
PRICES_USD_PER_MTOK = {
    "claude-fable-5": _price(10, 50),
    "claude-fable-5-1": _price(10, 50, read_ratio=0.025),
    "claude-fable-5-2": _price(8, 40, read_ratio=0.025),
    "claude-haiku-4-5": _price(1, 5),
    "claude-haiku-5-5": _price(0.10, 0.50),
    "claude-opus-4-8": _price(5, 25),
    "claude-opus-5": _price(5, 25),
    "claude-opus-5-5": _price(4, 20, read_ratio=0.05),
    "claude-sonnet-5": _price(2, 10),
    "claude-sonnet-5-5": _price(2, 10, read_ratio=0.05),
}

# Energy per 1000 weighted (output-equivalent) tokens, Wh: low / central / high.
WH_PER_1K_WEIGHTED = (0.5, 2.0, 6.0)
# Water per kWh: onsite datacenter cooling, and total including power generation.
WATER_ONSITE_L_PER_KWH = 1.1
WATER_TOTAL_L_PER_KWH = 3.0

# Anything resembling a filesystem location is refused before the ledger is
# written: a drive letter, a POSIX home, a UNC root, or any backslash at all.
PATHLIKE_RE = re.compile(r"[A-Za-z]:[\\/]|/home/|/Users/|\\")


def projects_root():
    """Directory Claude Code keeps its per-project transcripts in."""
    config_dir = os.environ.get("CLAUDE_CONFIG_DIR") or os.path.expanduser("~/.claude")
    return os.path.join(config_dir, "projects")


def folder_key(text):
    """Collapse a path or project folder name to a comparable key.

    Claude Code names a project folder after its working directory with the
    separators and punctuation replaced by dashes. Normalizing both sides the
    same way matches them without having to reproduce that mangling exactly.
    """
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")


def candidate_project_dirs():
    """Project folders belonging to this checkout, including its worktrees."""
    root = projects_root()
    if not os.path.isdir(root):
        raise SystemExit(f"error: no Claude Code transcripts at {root}")
    repo_key = folder_key(REPO_ROOT)
    dirs = []
    for name in sorted(os.listdir(root)):
        path = os.path.join(root, name)
        if not os.path.isdir(path):
            continue
        key = folder_key(name)
        if key == repo_key or key.startswith(repo_key + "-"):
            dirs.append(path)
    return dirs


def is_inside_repo(path):
    if not path:
        return False
    norm = os.path.normcase(os.path.normpath(path))
    root = os.path.normcase(os.path.normpath(REPO_ROOT))
    return norm == root or norm.startswith(root + os.sep)


def session_files(project_dir):
    """Map each session id to its transcript plus its subagent transcripts."""
    sessions = {}
    for path in sorted(glob.glob(os.path.join(project_dir, "*.jsonl"))):
        sid = os.path.basename(path)[:-6]
        sessions[sid] = [path] + sorted(
            glob.glob(os.path.join(project_dir, sid, "**", "*.jsonl"), recursive=True)
        )
    return sessions


def scan_session(paths):
    """Sum one session's usage across its own transcript and its subagents."""
    requests = {}
    cwds = collections.Counter()
    branches = collections.Counter()
    title = None
    first = last = None
    for path in paths:
        if not os.path.isfile(path):
            continue
        with open(path, encoding="utf-8", errors="replace") as fh:
            for line in fh:
                try:
                    rec = json.loads(line)
                except ValueError:
                    continue
                if rec.get("cwd"):
                    cwds[rec["cwd"]] += 1
                if rec.get("gitBranch"):
                    branches[rec["gitBranch"]] += 1
                if rec.get("customTitle"):
                    title = rec["customTitle"]
                msg = rec.get("message")
                if not isinstance(msg, dict):
                    continue
                usage = msg.get("usage")
                if not isinstance(usage, dict):
                    continue
                model = msg.get("model", "unknown")
                if model == "<synthetic>":
                    continue
                stamp = rec.get("timestamp")
                if stamp:
                    first = stamp if first is None else min(first, stamp)
                    last = stamp if last is None else max(last, stamp)
                # Streaming rewrites one request's record many times with a
                # growing output count, so keep the largest per request. A
                # retry gets its own requestId and rightly counts again.
                key = rec.get("requestId") or (path, msg.get("id") or rec.get("uuid"))
                cache = usage.get("cache_creation") or {}
                written_1h = cache.get("ephemeral_1h_input_tokens") or 0
                written_5m = cache.get("ephemeral_5m_input_tokens") or 0
                written_total = usage.get("cache_creation_input_tokens") or 0
                if not written_1h and not written_5m:
                    # Older records report only the total; treat it as 5m,
                    # which is the default TTL and the cheaper of the two.
                    written_5m = written_total
                out = usage.get("output_tokens") or 0
                prev = requests.get(key)
                if prev is not None and prev[1]["output"] >= out:
                    continue
                requests[key] = (
                    model,
                    {
                        "calls": 1,
                        "input": usage.get("input_tokens") or 0,
                        "cacheWrite5m": written_5m,
                        "cacheWrite1h": written_1h,
                        "cacheRead": usage.get("cache_read_input_tokens") or 0,
                        "output": out,
                    },
                )

    models = collections.defaultdict(collections.Counter)
    for model, counts in requests.values():
        models[model].update(counts)
    return {
        "cwd": cwds.most_common(1)[0][0] if cwds else None,
        "branch": branches.most_common(1)[0][0] if branches else None,
        "title": title,
        "first": first,
        "last": last,
        "models": {m: dict(c) for m, c in models.items()},
    }


def load_ledger():
    """Read the ledger, and cull any session the exclusion list claims.

    The exclusion list is authoritative, so adding an id to it by hand is the
    whole gesture needed to retire a session: the next write drops its counters.
    Returns the ledger and the ids that were culled, so the caller can report
    a file that still has to be rewritten.
    """
    if not os.path.isfile(LEDGER_JSON):
        return {"schemaVersion": SCHEMA_VERSION, "sessions": {}, "excluded": {}}, []
    with open(LEDGER_JSON, encoding="utf-8") as fh:
        ledger = json.load(fh)
    if ledger.get("schemaVersion") != SCHEMA_VERSION:
        raise SystemExit(
            f"error: {LEDGER_JSON} is schema {ledger.get('schemaVersion')}, "
            f"this script writes {SCHEMA_VERSION}"
        )
    ledger.setdefault("sessions", {})
    ledger.setdefault("excluded", {})
    culled = [sid for sid in ledger["sessions"] if sid in ledger["excluded"]]
    for sid in culled:
        del ledger["sessions"][sid]
    return ledger, culled


def assert_no_paths(node, trail="ledger"):
    if isinstance(node, dict):
        for key, value in node.items():
            assert_no_paths(key, trail)
            assert_no_paths(value, f"{trail}.{key}")
    elif isinstance(node, list):
        for item in node:
            assert_no_paths(item, trail)
    elif isinstance(node, str) and PATHLIKE_RE.search(node):
        raise SystemExit(f"error: refusing to write a filesystem path at {trail}: {node!r}")


ENTRY_FIELDS = ("label", "firstActivity", "lastActivity", "branch", "models")


def write_ledger(ledger):
    assert_no_paths(ledger)

    def ordered_entry(entry):
        out = {k: entry[k] for k in ENTRY_FIELDS if k in entry}
        out.update({k: v for k, v in entry.items() if k not in ENTRY_FIELDS})
        out["models"] = {
            model: {k: counts.get(k, 0) for k in COUNTERS}
            for model, counts in sorted(entry.get("models", {}).items())
        }
        return out

    ordered = {
        "schemaVersion": SCHEMA_VERSION,
        "sessions": {
            sid: ordered_entry(entry)
            for sid, entry in sorted(
                ledger["sessions"].items(),
                key=lambda kv: (kv[1].get("firstActivity") or "", kv[0]),
            )
        },
        "excluded": dict(sorted(ledger["excluded"].items())),
    }
    with open(LEDGER_JSON, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(ordered, fh, indent=2, ensure_ascii=False)
        fh.write("\n")


def merge_scan(ledger, scanned):
    """Fold a scan into the ledger. Counters only ever grow."""
    added, updated, skipped = [], [], []
    for sid, found in sorted(scanned.items()):
        if sid in ledger["excluded"]:
            continue
        if sid not in ledger["sessions"] and not is_inside_repo(found["cwd"]):
            ledger["excluded"][sid] = {"reason": "outside-repo"}
            skipped.append((sid, found["cwd"]))
            continue
        if not found["models"]:
            continue
        entry = ledger["sessions"].get(sid)
        if entry is None:
            entry = {"label": UNTITLED, "models": {}}
            ledger["sessions"][sid] = entry
            added.append(sid)
        before = json.dumps(entry, sort_keys=True)

        # A hand-edited label wins; a title that only arrived later fills in.
        if found["title"] and entry.get("label", UNTITLED) == UNTITLED:
            entry["label"] = found["title"]
        if found["first"]:
            entry["firstActivity"] = min(
                found["first"], entry.get("firstActivity") or found["first"]
            )
        if found["last"]:
            entry["lastActivity"] = max(
                found["last"], entry.get("lastActivity") or found["last"]
            )
        if found["branch"] and not entry.get("branch"):
            entry["branch"] = found["branch"]
        for model, counts in found["models"].items():
            stored = entry["models"].setdefault(model, {k: 0 for k in COUNTERS})
            for key in COUNTERS:
                stored[key] = max(stored.get(key, 0), counts.get(key, 0))
        entry["models"] = dict(sorted(entry["models"].items()))
        if sid not in added and json.dumps(entry, sort_keys=True) != before:
            updated.append(sid)
    return added, updated, skipped


def weighted(counts):
    return sum(counts.get(k, 0) * w for k, w in WEIGHTS.items())


def cost_usd(model, counts):
    """API-equivalent cost of one model's counters at list prices."""
    prices = PRICES_USD_PER_MTOK.get(model)
    if prices is None:
        raise SystemExit(
            f"error: no list price for model {model!r}, add it to PRICES_USD_PER_MTOK "
            f"in tools/token_stats.py"
        )
    return sum(counts.get(k, 0) * rate for k, rate in prices.items()) / 1e6


def fmt(n):
    return f"{int(n):,}"


def fmt_compact(n):
    for div, suffix in ((1e9, "B"), (1e6, "M"), (1e3, "k")):
        if n >= div:
            return f"{n / div:.2g}{suffix}" if n < 10 * div else f"{n / div:.0f}{suffix}"
    return str(int(n))


def date_range(entry):
    first = (entry.get("firstActivity") or "")[:10]
    last = (entry.get("lastActivity") or "")[:10]
    if first and last and first != last:
        return f"{first} to {last}"
    return first or last or "unknown"


def render(ledger):
    """Build the two generated blocks. A pure function of the ledger."""
    sessions = sorted(
        ledger["sessions"].items(), key=lambda kv: (kv[1].get("firstActivity") or "", kv[0])
    )
    totals = collections.defaultdict(collections.Counter)
    for _, entry in sessions:
        for model, counts in entry.get("models", {}).items():
            totals[model].update(counts)

    session_rows = [
        "| Session | Dates | Models | API calls | Output | Cache read |",
        "|---|---|---|---|---|---|",
    ]
    for _, entry in sessions:
        merged = collections.Counter()
        for counts in entry.get("models", {}).values():
            merged.update(counts)
        models = ", ".join(sorted(entry.get("models", {})))
        session_rows.append(
            f"| {entry.get('label', UNTITLED)} | {date_range(entry)} | {models} "
            f"| {fmt(merged['calls'])} | {fmt(merged['output'])} | {fmt(merged['cacheRead'])} |"
        )

    total_rows = [
        "| Model | API calls | Input | Cache write | Cache read | Output | Weighted | API-equiv. cost |",
        "|---|---|---|---|---|---|---|---|",
    ]
    grand = collections.Counter()
    total_cost = 0.0
    for model, counts in sorted(totals.items()):
        grand.update(counts)
        cost = cost_usd(model, counts)
        total_cost += cost
        writes = counts["cacheWrite5m"] + counts["cacheWrite1h"]
        total_rows.append(
            f"| {model} | {fmt(counts['calls'])} | {fmt(counts['input'])} | {fmt(writes)} "
            f"| {fmt(counts['cacheRead'])} | {fmt(counts['output'])} | {fmt(weighted(counts))} "
            f"| ${cost:,.0f} |"
        )
    grand_writes = grand["cacheWrite5m"] + grand["cacheWrite1h"]
    gw = weighted(grand)
    total_rows.append(
        f"| **total** | {fmt(grand['calls'])} | {fmt(grand['input'])} | {fmt(grand_writes)} "
        f"| {fmt(grand['cacheRead'])} | {fmt(grand['output'])} | **{fmt(gw)}** "
        f"| **${total_cost:,.0f}** |"
    )

    est_rows = [
        "| Scenario | Wh per 1k weighted tokens | Energy | Water (onsite cooling) | Water (incl. generation) |",
        "|---|---|---|---|---|",
    ]
    for label, wh in zip(("low", "central", "high"), WH_PER_1K_WEIGHTED):
        kwh = gw / 1000 * wh / 1000
        est_rows.append(
            f"| {label} | {wh} | {kwh:,.0f} kWh | {kwh * WATER_ONSITE_L_PER_KWH:,.0f} L "
            f"| {kwh * WATER_TOTAL_L_PER_KWH:,.0f} L |"
        )

    through = max((e.get("lastActivity") or "" for _, e in sessions), default="")[:10]
    central_kwh = gw / 1000 * WH_PER_1K_WEIGHTED[1] / 1000
    stats_block = "\n".join(
        [f"Generated by `tools/token_stats.py` from `docs/ai-usage.json`, "
         f"covering {len(sessions)} sessions through {through or 'no recorded activity'}.", ""]
        + ["## Per-session usage", ""] + session_rows + [""]
        + ["## Totals per model", ""] + total_rows + [""]
        + ["## Energy and water estimate", ""] + est_rows
    )

    def badge(label, value, color):
        esc = lambda s: s.replace("-", "--").replace(" ", "_").replace("/", "%2F")
        return f"![{label}](https://img.shields.io/badge/{esc(label)}-{esc(value)}-{color})"

    badge_block = " ".join([
        badge("AI tokens",
              f"{fmt_compact(grand['output'])} out / {fmt_compact(grand['cacheRead'])} read",
              "blueviolet"),
        badge("API-equiv. cost", f"~${fmt_compact(total_cost)}", "green"),
    ]) + "\n(cost is the API-equivalent at list prices, see [TOKEN_STATS.md](TOKEN_STATS.md))"

    return badge_block, stats_block, grand, gw, central_kwh, total_cost


def substitute(text, marker, content, path):
    begin, end = f"<!-- {marker}:BEGIN -->", f"<!-- {marker}:END -->"
    pattern = re.compile(re.escape(begin) + r".*?" + re.escape(end), re.DOTALL)
    if not pattern.search(text):
        raise SystemExit(f"error: marker {marker} not found in {path}")
    return pattern.sub(lambda _: begin + "\n" + content + "\n" + end, text)


def rendered_files(ledger):
    """Map each generated file to the text it should hold for this ledger."""
    badge_block, stats_block, _, _, _, _ = render(ledger)
    out = {}
    with open(STATS_MD, encoding="utf-8") as fh:
        stats = fh.read()
    stats = substitute(stats, "AI_USAGE_BADGES", badge_block, STATS_MD)
    out[STATS_MD] = substitute(stats, "TOKEN_STATS", stats_block, STATS_MD)
    with open(README_MD, encoding="utf-8") as fh:
        out[README_MD] = substitute(fh.read(), "AI_USAGE_BADGES", badge_block, README_MD)
    return out


def write_rendered(ledger):
    for path, text in rendered_files(ledger).items():
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)


def check_rendered(ledger, culled):
    stale = False
    for path, expected in rendered_files(ledger).items():
        with open(path, encoding="utf-8") as fh:
            actual = fh.read()
        if actual == expected:
            continue
        stale = True
        rel = os.path.relpath(path, REPO_ROOT).replace(os.sep, "/")
        print(f"{rel} does not match docs/ai-usage.json:")
        sys.stdout.writelines(
            difflib.unified_diff(
                actual.splitlines(keepends=True),
                expected.splitlines(keepends=True),
                fromfile=f"{rel} (committed)",
                tofile=f"{rel} (from ledger)",
            )
        )
    if culled:
        stale = True
        print(f"docs/ai-usage.json still holds {len(culled)} session(s) its excluded list retires:")
        for sid in culled:
            print(f"  {sid}")
    if stale:
        raise SystemExit("error: run `python tools/token_stats.py --render` and commit the result")
    print("docs/ai-usage.json, TOKEN_STATS.md and README.md agree")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--render", action="store_true",
                      help="render from the ledger without scanning transcripts")
    mode.add_argument("--check", action="store_true",
                      help="verify the rendered files match the ledger, and exit nonzero if not")
    args = parser.parse_args()

    ledger, culled = load_ledger()

    if args.check:
        check_rendered(ledger, culled)
        return

    if not args.render:
        scanned = {}
        for project_dir in candidate_project_dirs():
            for sid, paths in session_files(project_dir).items():
                scanned[sid] = scan_session(paths)
        if not scanned:
            raise SystemExit(f"error: no transcripts found for this checkout under {projects_root()}")
        added, updated, skipped = merge_scan(ledger, scanned)
        for sid, cwd in skipped:
            print(f"excluded {sid}: worked in {cwd}")
        print(f"scanned {len(scanned)} sessions: {len(added)} new, {len(updated)} updated, "
              f"{len(skipped)} newly excluded")

    for sid in culled:
        print(f"culled {sid}: listed in excluded")
    # Both writing modes rewrite the ledger, so a hand-added exclusion is
    # persisted by either one rather than only by a full scan.
    write_ledger(ledger)
    write_rendered(ledger)
    _, _, grand, gw, central_kwh, total_cost = render(ledger)
    print(f"total output {fmt(grand['output'])}, cache read {fmt(grand['cacheRead'])}, "
          f"weighted {fmt(gw)}, API-equivalent cost ${total_cost:,.0f}, "
          f"central estimate {central_kwh:,.0f} kWh")


if __name__ == "__main__":
    main()
