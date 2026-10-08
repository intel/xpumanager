#!/usr/bin/env python3
# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT
import argparse
import json
from pathlib import Path


def fmt(data: dict) -> str:
    return f"{data['line_percent']:.1f}% ({data['line_covered']}/{data['line_total']})"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("summary", type=Path)
    parser.add_argument("--title", default="## Code Coverage Report", metavar="MARKDOWN")
    parser.add_argument("--line-label", default="Line coverage", metavar="TEXT")
    parser.add_argument("--measured-by", default="unit tests", metavar="TEXT")
    args = parser.parse_args()

    data = json.loads(args.summary.read_text())
    lines = [
        args.title,
        "",
        f"Coverage measured by {args.measured_by}.",
        "",
        f"{args.line_label}: **{fmt(data)}**",
    ]
    modules = sorted(data.get("modules") or [], key=lambda m: m["module"])
    if modules:
        lines += ["", f"| Module | {args.line_label} |", "|--------|-----|"]
        lines += [f"| {m['module']} | {fmt(m)} |" for m in modules]
    print("\n".join(lines))


if __name__ == "__main__":
    main()
