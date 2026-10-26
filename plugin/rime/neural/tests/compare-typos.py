# SPDX-License-Identifier: MIT
"""Compare --probe captures from COPIED Rime data. No ADB or phone writes.

This is a small diagnostic set, not an estimate of general typing accuracy.
The 'ambiguous' row is retained for inspection and excluded from the score.
"""
import argparse
import csv
import json
from pathlib import Path
import re


def read_capture(path):
    records = {}
    current = None
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        match = re.match(r"PROBE input=(\S+) total_us=(\d+)", line)
        if match:
            current = {"total_us": int(match[2]), "candidates": []}
            records[match[1]] = current
        match = re.match(r"CAND (\d+) (\S+) (\d+):(\d+) (.*)", line)
        if match and current is not None:
            current["candidates"].append({"text": match[5], "end": int(match[4])})
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument("--cases", type=Path, default=Path(__file__).with_name("typos.tsv"))
    args = parser.parse_args()
    before, after = read_capture(args.before), read_capture(args.after)
    results = []
    with args.cases.open(encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            key, gold = row["input"].strip(), row["gold"]
            record = {"kind": row["kind"], "input": key, "gold": gold}
            for label, source in (("before", before), ("after", after)):
                if key not in source:
                    raise ValueError(f"{label}: missing probe for {key}")
                candidates = source[key]["candidates"]
                record[label] = candidates[0]["text"] if candidates else ""
                record[label + "_covered"] = any(
                    c["text"] == gold and c["end"] == len(key) for c in candidates[:16]
                )
                record[label + "_total_us"] = source[key]["total_us"]
            results.append(record)
    scored = [r for r in results if r["kind"] != "ambiguous"]
    summary = {"diagnostic_cases": len(scored), "excluded_ambiguous": len(results) - len(scored)}
    for label in ("before", "after"):
        summary[label + "_top1"] = sum(r[label] == r["gold"] for r in scored)
        summary[label + "_covered_at16"] = sum(r[label + "_covered"] for r in scored)
    summary["repaired"] = sum(r["before"] != r["gold"] == r["after"] for r in scored)
    summary["regressed"] = sum(r["before"] == r["gold"] != r["after"] for r in scored)
    print(json.dumps({"summary": summary, "cases": results}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
