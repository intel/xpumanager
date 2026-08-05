#!/usr/bin/env python3
# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT
import argparse
import json
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Metric:
    label: str
    key: str


METRICS = [
    Metric("Line coverage", "line"),
    Metric("Branch coverage", "branch"),
]


def load(path: Path) -> dict:
    return json.loads(path.read_text())


def fmt(pct: float) -> str:
    return f"{pct:.1f}%"


def direction(delta: float) -> str:
    if delta > 0.5:
        return "increased"
    if delta < -0.5:
        return "decreased"
    return "unchanged"


def fraction(data: dict, kind: str) -> str:
    covered = data.get(f"{kind}_covered", 0)
    total = data.get(f"{kind}_total", 0)
    return "N/A" if total == 0 else f"{covered}/{total}"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("base", type=Path)
    parser.add_argument("pr", type=Path)
    parser.add_argument("--base-ref", default="base", metavar="BRANCH")
    args = parser.parse_args()

    base = load(args.base)
    pr = load(args.pr)

    lines = [
        "## Code Coverage Report",
        "",
        f"Coverage measured by doctest unit tests (`meson test`) compared to `{args.base_ref}`.",
        "",
        f"| Metric | {args.base_ref} | PR | Change | Direction |",
        f"|--------|{''.join('-' for _ in args.base_ref)}--|-----|--------|-----------|",
    ]

    for m in METRICS:
        b_pct = base.get(f"{m.key}_percent", 0.0)
        p_pct = pr.get(f"{m.key}_percent", 0.0)
        delta = p_pct - b_pct
        sign = "+" if delta >= 0 else ""
        lines.append(
            f"| {m.label} | {fmt(b_pct)} ({fraction(base, m.key)}) |"
            f" {fmt(p_pct)} ({fraction(pr, m.key)}) | {sign}{fmt(delta)} | {direction(delta)} |"
        )

    base_files = {f["filename"]: f.get("line_percent", 0.0) for f in base.get("files", [])}
    pr_files = {f["filename"]: f.get("line_percent", 0.0) for f in pr.get("files", [])}

    changed = sorted(
        [
            (fn, base_files.get(fn, 0.0), pr_files.get(fn, 0.0))
            for fn in set(base_files) | set(pr_files)
            if abs(pr_files.get(fn, 0.0) - base_files.get(fn, 0.0)) > 1.0
        ],
        key=lambda x: x[1] - x[2],
        reverse=True,
    )

    if changed:
        lines += [
            "",
            "<details>",
            "<summary>Files with line coverage changes greater than 1%</summary>",
            "",
            f"| File | {args.base_ref} | PR | Change |",
            "|------|-----|----|--------|",
        ]
        for fn, b, p in changed:
            delta = p - b
            sign = "+" if delta >= 0 else ""
            short = fn.split("/", 1)[-1] if "/" in fn else fn
            lines.append(f"| `{short}` | {fmt(b)} | {fmt(p)} | {sign}{fmt(delta)} |")
        lines += ["", "</details>"]
    else:
        lines += ["", "_No individual files changed line coverage by more than 1%._"]

    print("\n".join(lines))


if __name__ == "__main__":
    main()

