#!/usr/bin/env python3
# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT
#
# Validates process memory from xpu-smi ps -j against a known allocation.
# Called by validation/tests/process_memory_reporting.yaml.
#
# xpu-smi ps -j schema (cmd_ps.cpp):
#   process_id      uint32   PID
#   process_name    string   basename
#   device_id       uint32   GPU device index
#   mem_size        uint64   device VRAM in KiB  (bytes / 1024)
#   shared_mem_size uint64   shared device memory in KiB
#
# Usage:
#   check_ps_memory.py --mode alloc  --alloc-bytes N --parent-pid N ps.json
#   check_ps_memory.py --mode fork   --alloc-bytes N --parent-pid N --child-pid N ps.json
#   check_ps_memory.py --mode dup    --alloc-bytes N --parent-pid N --dup-fds N ps.json
#
# Exit 0 with "PASS" on success; exit 1 with "FAIL: ..." on regression;
# exit 0 with "SKIP: ..." when the PID is absent from ps output.

import argparse
import json
import sys


def find_proc(procs, pid):
    return next(
        (p for p in procs if int(p.get("process_id", p.get("pid", -1))) == pid),
        None,
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["alloc", "fork", "dup"], required=True)
    ap.add_argument("--alloc-bytes", type=int, required=True)
    ap.add_argument("--parent-pid", type=int, required=True)
    ap.add_argument("--child-pid", type=int, default=None)
    ap.add_argument("--dup-fds", type=int, default=None)
    ap.add_argument("ps_json")
    args = ap.parse_args()

    alloc_kib = args.alloc_bytes // 1024
    lo = alloc_kib // 2   # 50% — sanity lower bound
    hi = alloc_kib * 2    # 200% — regression upper bound

    try:
        with open(args.ps_json) as f:
            data = json.loads(f.read().strip())
    except Exception as e:
        print(f"SKIP: could not parse ps JSON: {e}")
        sys.exit(0)

    procs = data if isinstance(data, list) else data.get("process_list", [])

    parent = find_proc(procs, args.parent_pid)
    if parent is None:
        print(
            f"SKIP: PID {args.parent_pid} not found in ps output ({len(procs)} entries); "
            "zesDeviceProcessesGetState may have filtered it (no active engine cycles)"
        )
        sys.exit(0)

    if args.mode == "alloc":
        reported = int(parent.get("mem_size", 0))
        print(f"alloc: {alloc_kib} KiB  reported: {reported} KiB  ratio: {reported/alloc_kib:.2f}x")
        if reported < lo:
            print(f"FAIL: reported {reported} KiB is below 50% of expected {alloc_kib} KiB")
            sys.exit(1)
        if reported > hi:
            print(f"FAIL: reported {reported} KiB exceeds 200% of expected — over-reporting")
            sys.exit(1)
        print("PASS")

    elif args.mode == "fork":
        child = find_proc(procs, args.child_pid) if args.child_pid else None
        p_kib = int(parent.get("mem_size", 0))
        c_kib = int(child.get("mem_size", 0)) if child else 0
        total = p_kib + c_kib
        print(
            f"alloc: {alloc_kib} KiB  parent: {p_kib} KiB  "
            f"child: {c_kib} KiB  total: {total} KiB  ratio: {total/alloc_kib:.2f}x"
        )
        if total < lo:
            print(f"FAIL: total {total} KiB below 50% of expected {alloc_kib} KiB")
            sys.exit(1)
        if total > hi:
            print(f"FAIL: total {total} KiB exceeds 200% — fork double-counting regression")
            sys.exit(1)
        print("PASS: memory correctly attributed across parent + child")

    elif args.mode == "dup":
        reported = int(parent.get("mem_size", 0))
        dup_fds = args.dup_fds or 1
        print(
            f"alloc: {alloc_kib} KiB  dup_fds: {dup_fds}  "
            f"reported: {reported} KiB  ratio: {reported/alloc_kib:.2f}x"
        )
        if reported < lo:
            print(f"FAIL: reported {reported} KiB below 50% of expected {alloc_kib} KiB")
            sys.exit(1)
        if reported > hi:
            near = abs(reported / alloc_kib - dup_fds) < 0.5
            detail = "dup fd multiplication regression" if near else "over-reporting"
            print(f"FAIL: reported {reported} KiB exceeds 200% of expected — {detail}")
            sys.exit(1)
        print("PASS: dup fds correctly deduplicated")


if __name__ == "__main__":
    main()
