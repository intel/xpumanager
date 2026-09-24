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


def load(path: Path) -> dict:
    return json.loads(path.read_text())


def fmt(pct: float | None) -> str:
    return "N/A" if pct is None else f"{pct:.1f}%"


def direction(delta: float) -> str:
    if delta > 0:
        return "increased"
    if delta < 0:
        return "decreased"
    return "unchanged"


def fraction(data: dict, kind: str) -> str:
    covered = data.get(f"{kind}_covered", 0)
    total = data.get(f"{kind}_total", 0)
    return "N/A" if total == 0 else f"{covered}/{total}"


def change(base_pct: float | None, pr_pct: float | None) -> str:
    if base_pct is None:
        return "added"
    if pr_pct is None:
        return "removed"
    delta = pr_pct - base_pct
    return f"{'+' if delta >= 0 else ''}{fmt(delta)}"


def module_table(base: dict, pr: dict, base_ref: str) -> list[str]:
    """Lists the coverage of every module, if the summaries have per-module data."""
    base_modules = {m["module"]: m for m in base.get("modules") or []}
    pr_modules = {m["module"]: m for m in pr.get("modules") or []}
    if not base_modules and not pr_modules:
        return []

    lines = [
        "",
        f"| Module | {base_ref} | PR | Change | Direction |",
        f"|--------|{''.join('-' for _ in base_ref)}--|-----|--------|-----------|",
    ]
    for name in sorted(set(base_modules) | set(pr_modules)):
        b, p = base_modules.get(name), pr_modules.get(name)
        b_pct = b.get("line_percent", 0.0) if b else None
        p_pct = p.get("line_percent", 0.0) if p else None
        b_cell = f"{fmt(b_pct)} ({fraction(b, 'line')})" if b else "N/A"
        p_cell = f"{fmt(p_pct)} ({fraction(p, 'line')})" if p else "N/A"
        dir_cell = "" if b is None or p is None else direction(p_pct - b_pct)
        lines.append(
            f"| `{name}` | {b_cell} | {p_cell} | {change(b_pct, p_pct)} | {dir_cell} |"
        )
    return lines


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("base", type=Path)
    parser.add_argument("pr", type=Path)
    parser.add_argument("--base-ref", default="base", metavar="BRANCH")
    parser.add_argument("--title", default="## Code Coverage Report", metavar="MARKDOWN")
    parser.add_argument(
        "--measured-by",
        default="doctest unit tests (`meson test`)",
        metavar="TEXT",
        help="what produced the coverage data",
    )
    parser.add_argument(
        "--line-label",
        default="Line coverage",
        metavar="TEXT",
        help="name of the line metric, e.g. 'Statement coverage' for Go",
    )
    args = parser.parse_args()

    base = load(args.base)
    pr = load(args.pr)

    lines = [
        args.title,
        "",
        f"Coverage measured by {args.measured_by} compared to `{args.base_ref}`.",
        "",
        f"| Metric | {args.base_ref} | PR | Change | Direction |",
        f"|--------|{''.join('-' for _ in args.base_ref)}--|-----|--------|-----------|",
    ]

    metrics = [
        Metric(args.line_label, "line"),
        Metric("Branch coverage", "branch"),
    ]

    for m in metrics:
        # Skip metrics that neither side measures, e.g. branches on Go code.
        if not base.get(f"{m.key}_total", 0) and not pr.get(f"{m.key}_total", 0):
            continue
        b_pct = base.get(f"{m.key}_percent", 0.0)
        p_pct = pr.get(f"{m.key}_percent", 0.0)
        delta = p_pct - b_pct
        sign = "+" if delta >= 0 else ""
        lines.append(
            f"| {m.label} | {fmt(b_pct)} ({fraction(base, m.key)}) |"
            f" {fmt(p_pct)} ({fraction(pr, m.key)}) | {sign}{fmt(delta)} | {direction(delta)} |"
        )

    lines += module_table(base, pr, args.base_ref)

    base_files = {f["filename"]: f.get("line_percent", 0.0) for f in base.get("files", [])}
    pr_files = {f["filename"]: f.get("line_percent", 0.0) for f in pr.get("files", [])}

    # Sort files by how much they regressed. Files added or removed have no delta so sort by filename.
    def sort_key(row: tuple[str, float | None, float | None]) -> tuple:
        fn, base_pct, pr_pct = row
        if base_pct is None or pr_pct is None:
            return (1, fn)
        return (0, pr_pct - base_pct)

    changed = sorted(
        [
            (fn, base_files.get(fn), pr_files.get(fn))
            for fn in set(base_files) | set(pr_files)
            if fn not in base_files
            or fn not in pr_files
            or abs(pr_files[fn] - base_files[fn]) > 1.0
        ],
        key=sort_key,
    )

    metric = args.line_label.lower()
    if changed:
        lines += [
            "",
            "<details>",
            f"<summary>Files with {metric} changes greater than 1%</summary>",
            "",
            f"| File | {args.base_ref} | PR | Change |",
            "|------|-----|----|--------|",
        ]
        for fn, b, p in changed:
            lines.append(f"| `{fn}` | {fmt(b)} | {fmt(p)} | {change(b, p)} |")
        lines += ["", "</details>"]
    else:
        lines += ["", f"_No individual files changed {metric} by more than 1%._"]

    print("\n".join(lines))


if __name__ == "__main__":
    main()

