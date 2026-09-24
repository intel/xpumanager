#!/usr/bin/env python3
# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT
import argparse
import json
import sys
from pathlib import Path


def parse(
    profiles: list[Path], strip_prefix: str
) -> tuple[dict[str, list[int]], dict[str, list[int]]]:
    """Sums the statements and the covered statements of every file and module."""
    # The profiles of several modules, or of packages instrumented with
    # "-coverpkg", may include the same block multiple times: count each of them
    # once, in the module of the profile it was first seen in.
    blocks: dict[tuple[str, str], tuple[int, bool, str]] = {}

    for profile in profiles:
        # The directory of the profile is the module, e.g. "xpumd/exporter/intelxpuinfo/api"
        module = str(profile.parent)
        for line in profile.read_text().splitlines():
            # The first line is the counter mode, the rest are coverage blocks:
            # "<import path>/<file>:<from>.<col>,<to>.<col> <statements> <count>"
            if not line or line.startswith("mode:"):
                continue
            position, statements, count = line.rsplit(" ", 2)
            name, _, block = position.partition(":")
            if strip_prefix and name.startswith(strip_prefix):
                name = name[len(strip_prefix):]
            covered = int(count) > 0
            previous = blocks.get((name, block))
            blocks[(name, block)] = (
                int(statements),
                covered or (previous is not None and previous[1]),
                module if previous is None else previous[2],
            )

    files: dict[str, list[int]] = {}
    modules: dict[str, list[int]] = {}
    for (name, _), (statements, covered, module) in blocks.items():
        for totals in (files.setdefault(name, [0, 0]), modules.setdefault(module, [0, 0])):
            totals[0] += statements if covered else 0
            totals[1] += statements
    return files, modules


def entries(key: str, totals: dict[str, list[int]]) -> list[dict]:
    return [
        {
            key: name,
            "line_covered": c,
            "line_total": t,
            "line_percent": percent(c, t),
        }
        for name, (c, t) in sorted(totals.items())
    ]


def percent(covered: int, total: int) -> float:
    return round(100.0 * covered / total, 2) if total else 0.0


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Summarizes Go coverage profiles in the JSON format of 'gcovr --json-summary'."
        " To be run in the root of the coverage directory: the directory of each profile,"
        " e.g. 'xpumd/exporter/intelxpuinfo/api/coverage.out', is the name of its module.",
    )
    parser.add_argument("profiles", nargs="+", type=Path)
    parser.add_argument(
        "--strip-prefix",
        default="",
        metavar="PATH",
        help="module path to drop from the file names, e.g. 'github.com/intel/level-zero-go/'",
    )
    args = parser.parse_args()

    files, modules = parse(args.profiles, args.strip_prefix)
    covered = sum(c for c, _ in files.values())
    total = sum(t for _, t in files.values())
    if not total:
        sys.exit(f"{parser.prog}: no coverage data in the given profiles")

    json.dump(
        {
            "line_covered": covered,
            "line_total": total,
            "line_percent": percent(covered, total),
            "branch_covered": 0,
            "branch_total": 0,
            "branch_percent": 0.0,
            "files": entries("filename", files),
            "modules": entries("module", modules),
        },
        sys.stdout,
        indent=2,
    )
    print()


if __name__ == "__main__":
    main()
