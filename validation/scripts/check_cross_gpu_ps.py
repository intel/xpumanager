#!/usr/bin/env python3
# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT
#
# Validates that xpu-smi ps correctly attributes processes to their actual GPU
# after the cross-GPU phantom process fix.
#
# Before the fix: Level Zero's zesDeviceProcessesGetState reported every
# zeInit'd process on every GPU (because zeInit opens DRM fds to all GPUs).
# After the fix, /proc/<pid>/fdinfo is read per-PID and memSize is zeroed when
# no real allocation is present on a given GPU, so each PID appears with
# significant memory on exactly one device.
#
# Called by validation/tests/cross_gpu_phantom_process.yaml.
#
# Usage:
#   check_cross_gpu_ps.py \
#       --parent-pid N --child-pid N \
#       --parent-alloc-bytes N --child-alloc-bytes N \
#       ps.json
#
# Exit 0 with "PASS" on success, exit 0 with "SKIP: ..." when neither PID is
# visible (driver may not enumerate them), exit 1 with "FAIL: ..." on regression.

from __future__ import annotations

import argparse
import json
import sys
from typing import Any

# Must exceed the minDeviceMemBytes threshold in hal/core/process.cpp (4 MiB).
SIGNIFICANT_KIB = 4 * 1024

Entry = dict[str, Any]


def pid_of(entry: Entry) -> int:
    return int(entry.get("process_id", entry.get("pid", -1)))


def mem_kib(entry: Entry) -> int:
    return int(entry.get("mem_size", 0))


def device_of(entry: Entry) -> Any:
    return entry.get("device_id", "?")


def find_procs(procs: list[Entry], pid: int) -> list[Entry]:
    return [p for p in procs if pid_of(p) == pid]


def significant(entries: list[Entry]) -> list[Entry]:
    return [e for e in entries if mem_kib(e) >= SIGNIFICANT_KIB]


def validate_pid(label: str, pid: int, entries: list[Entry], alloc_kib: int) -> list[str]:
    sig = significant(entries)
    match len(sig):
        case 0:
            return [f"{label} PID {pid} has no significant-memory entries "
                    f"(expected ≈{alloc_kib} KiB on one device)"]
        case 1:
            expected_dev = device_of(sig[0])
            phantoms = [e for e in entries if device_of(e) != expected_dev and mem_kib(e) > 0]
            if phantoms:
                phantom_devs = [device_of(e) for e in phantoms]
                return [f"REGRESSION: {label} PID {pid} has below-threshold entries on "
                        f"devices {phantom_devs} (expected device {expected_dev}) — "
                        "phantom attribution not fully filtered"]
            return []
        case _:
            devs = [device_of(e) for e in sig]
            return [f"REGRESSION: {label} PID {pid} has significant memory on "
                    f"{len(sig)} devices {devs} — phantom entries not filtered"]


def check_separate_devices(
    parent_sig: list[Entry], child_sig: list[Entry],
    parent_pid: int, child_pid: int,
) -> list[str]:
    if len(parent_sig) != 1 or len(child_sig) != 1:
        return []
    p_dev, c_dev = device_of(parent_sig[0]), device_of(child_sig[0])
    if p_dev == c_dev:
        return [f"REGRESSION: parent PID {parent_pid} and child PID {child_pid} "
                f"both reported with significant memory on device {p_dev} — "
                "cross-GPU phantom attribution not corrected"]
    return []


def check_expected_device(
    label: str, pid: int, sig: list[Entry], expected: int | None,
) -> list[str]:
    if expected is None or len(sig) != 1:
        return []
    actual = device_of(sig[0])
    if str(actual) != str(expected):
        return [f"REGRESSION: {label} PID {pid} reported on device {actual}, "
                f"expected device {expected} — attribution swapped"]
    return []


def load_procs(path: str) -> list[Entry]:
    with open(path) as f:
        data = json.load(f)
    return data if isinstance(data, list) else data.get("process_list", [])


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--parent-pid", type=int, required=True)
    ap.add_argument("--child-pid", type=int, required=True)
    ap.add_argument("--parent-alloc-bytes", type=int, required=True)
    ap.add_argument("--child-alloc-bytes", type=int, required=True)
    ap.add_argument("--parent-device", type=int, default=None)
    ap.add_argument("--child-device", type=int, default=None)
    ap.add_argument("ps_json")
    args = ap.parse_args()

    try:
        procs = load_procs(args.ps_json)
    except Exception as e:  # noqa: BLE001
        print(f"FAIL: could not parse ps JSON: {e}")
        sys.exit(1)

    parent_entries = find_procs(procs, args.parent_pid)
    child_entries  = find_procs(procs, args.child_pid)

    match (bool(parent_entries), bool(child_entries)):
        case (False, False):
            print(f"SKIP: neither PID {args.parent_pid} nor {args.child_pid} in ps output "
                  f"({len(procs)} entries); zesDeviceProcessesGetState may not enumerate "
                  "these processes on this driver")
            sys.exit(0)
        case (False, _):
            print(f"FAIL: parent PID {args.parent_pid} not in ps output "
                  f"(child PID {args.child_pid} is present — asymmetric attribution)")
            sys.exit(1)
        case (_, False):
            print(f"FAIL: child PID {args.child_pid} not in ps output "
                  f"(parent PID {args.parent_pid} is present — asymmetric attribution)")
            sys.exit(1)

    for label, pid, entries in (
        ("parent", args.parent_pid, parent_entries),
        ("child ", args.child_pid,  child_entries),
    ):
        for e in entries:
            print(f"  {label} PID {pid}: device={device_of(e)}  mem={mem_kib(e)} KiB")

    parent_sig = significant(parent_entries)
    child_sig  = significant(child_entries)

    failures = (
        validate_pid("parent", args.parent_pid, parent_entries, args.parent_alloc_bytes // 1024)
        + validate_pid("child",  args.child_pid,  child_entries,  args.child_alloc_bytes  // 1024)
        + check_separate_devices(parent_sig, child_sig, args.parent_pid, args.child_pid)
        + check_expected_device("parent", args.parent_pid, parent_sig, args.parent_device)
        + check_expected_device("child",  args.child_pid,  child_sig,  args.child_device)
    )

    if failures:
        for msg in failures:
            print(f"FAIL: {msg}")
        sys.exit(1)

    p_dev, c_dev = device_of(parent_sig[0]), device_of(child_sig[0])
    p_kib, c_kib = mem_kib(parent_sig[0]),   mem_kib(child_sig[0])
    print(f"PASS: parent on device {p_dev} ({p_kib} KiB ≈ {p_kib // 1024} MiB), "
          f"child on device {c_dev} ({c_kib} KiB ≈ {c_kib // 1024} MiB) — "
          "correctly attributed to separate GPUs")


if __name__ == "__main__":
    main()
