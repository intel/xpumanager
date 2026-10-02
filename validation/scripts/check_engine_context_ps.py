#!/usr/bin/env python3
# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT
#
# Validates xpu-smi's engine-context process filter against the A/B pair created
# by validation/scripts/engine_context_probe.cpp.
#
# The probe starts two processes that allocate the same VRAM on the same device
# and differ only in whether they submitted a GPU command:
#
#   submitter  drm-cycles-<eng> > 0   -> xpu-smi must list it
#   idler      every counter is 0     -> xpu-smi must omit it
#
# Holding VRAM is not what makes a process a client of a device: zeInit() opens a
# DRM fd and makes small internal allocations on every GPU in the system. Only
# having executed work does, and fdinfo reports that as a non-zero cycle counter.
#
# Guards against a vacuous pass. "Idler absent" means nothing on its own -- an
# empty process list would satisfy it. The check therefore also requires:
#   - the submitter IS listed, on the device it ran on, proving the filter is not
#     rejecting everything
#   - RAW_SYSMAN_IDLER=yes, proving the driver offered the idler and xpu-smi is
#     the component that dropped it
#   - the idler holds non-zero VRAM in fdinfo, so the memory correction that
#     predates the engine filter would have kept it; otherwise the idler's absence
#     would be explained by the older rule and prove nothing about this one
#   - the probe's own counters match its roles, proving the premise held
#
# Called by validation/tests/engine_context_filter.yaml.
#
# Usage:
#   check_engine_context_ps.py --probe-output probe.out ps.json
#
# Exit 0 with "PASS" on success, exit 0 with "SKIP: ..." when the premise did not
# hold on this driver, exit 1 with "FAIL: ..." on regression.

from __future__ import annotations

import argparse
import json
import sys
from typing import Any

Entry = dict[str, Any]

# Root keys xpu-smi has used for the ps process array, newest first.
PROC_LIST_KEYS = ("device_util_by_proc_list", "process_list")


def parse_probe_output(path: str) -> dict[str, str]:
    """Reads the probe's KEY=VALUE protocol into a dict."""
    values: dict[str, str] = {}
    with open(path) as f:
        for line in f:
            key, _, val = line.strip().partition("=")
            if key and _:
                values[key] = val
    return values


def load_procs(path: str) -> list[Entry]:
    with open(path) as f:
        data = json.load(f)
    if isinstance(data, list):
        return data
    for key in PROC_LIST_KEYS:
        if isinstance(data.get(key), list):
            return data[key]
    return []


def pid_of(entry: Entry) -> int:
    return int(entry.get("process_id", entry.get("pid", -1)))


def device_of(entry: Entry) -> Any:
    return entry.get("device_id", "?")


def describe(entry: Entry) -> str:
    return (
        f"device={entry.get('device_id', '?')} "
        f"mem={entry.get('mem_size', 0)} KiB "
        f"type={entry.get('type', '?')} "
        f"engines={entry.get('engines', '?')}"
    )


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--probe-output", required=True,
                    help="stdout captured from engine_context_probe")
    ap.add_argument("ps_json")
    args = ap.parse_args()

    try:
        probe = parse_probe_output(args.probe_output)
    except OSError as e:
        print(f"FAIL: could not read probe output: {e}")
        sys.exit(1)

    # A broken or stale helper, not an unsupported driver: the caller only runs
    # this checker after the probe prints READY, so by now the whole protocol must
    # be there. Skipping instead would let a malformed binary pass the regression
    # without either process ever being examined.
    required = ("SUBMITTER_PID", "IDLER_PID", "SUBMITTER_CYCLES", "IDLER_CYCLES",
                "IDLER_VRAM_KIB")
    missing = [k for k in required if k not in probe]
    if missing:
        print(f"FAIL: probe reached READY but its output lacks "
              f"{', '.join(missing)}; the helper is incompatible with this check")
        sys.exit(1)

    submitter_pid = int(probe["SUBMITTER_PID"])
    idler_pid = int(probe["IDLER_PID"])
    submitter_cycles = int(probe["SUBMITTER_CYCLES"])
    idler_cycles = int(probe["IDLER_CYCLES"])
    idler_vram_kib = int(probe["IDLER_VRAM_KIB"])

    # --- premise checks: did the probe actually build the intended A/B? ---
    if submitter_cycles == 0:
        print(f"SKIP: submitter PID {submitter_pid} reports 0 engine cycles; its "
              "GPU submission did not register, so there is no positive case")
        sys.exit(0)
    if idler_vram_kib == 0:
        # Without VRAM the idler would be dropped by the memory correction that
        # predates the engine filter, so its absence would say nothing about the
        # behaviour under test. The idler has to be a process the memory rule alone
        # would have kept.
        print(f"SKIP: idler PID {idler_pid} holds 0 KiB in fdinfo; the memory "
              "correction would drop it regardless, so its absence would not "
              "isolate engine-context filtering")
        sys.exit(0)
    if idler_cycles != 0:
        print(f"SKIP: idler PID {idler_pid} reports {idler_cycles} engine cycles; "
              "something submitted work on its behalf, so it is not an idle holder")
        sys.exit(0)

    raw_idler = probe.get("RAW_SYSMAN_IDLER", "unknown")
    if raw_idler != "yes":
        print(f"SKIP: RAW_SYSMAN_IDLER={raw_idler}; zesDeviceProcessesGetState did "
              f"not report idler PID {idler_pid}, so its absence from xpu-smi does "
              "not demonstrate filtering")
        sys.exit(0)

    try:
        procs = load_procs(args.ps_json)
    except Exception as e:  # noqa: BLE001
        print(f"FAIL: could not parse ps JSON: {e}")
        sys.exit(1)

    device = probe.get("DEVICE")
    submitter_entries = [p for p in procs if pid_of(p) == submitter_pid]
    idler_entries = [p for p in procs if pid_of(p) == idler_pid]

    # The submitter must appear on the device it actually ran on. Matching by PID
    # across every device would let the test pass on a multi-GPU host when the
    # submitter is missing from its own GPU and shows up only as a phantom on
    # another. The idler is still checked across all devices, since it ran nowhere
    # and so belongs in no device's list.
    submitter_here = [p for p in submitter_entries
                      if device is None or str(device_of(p)) == str(device)]

    print(f"  probe: submitter PID {submitter_pid} device={device} "
          f"cycles={submitter_cycles} "
          f"vram={probe.get('SUBMITTER_VRAM_KIB', '?')} KiB")
    print(f"  probe: idler     PID {idler_pid} device={device} "
          f"cycles={idler_cycles} "
          f"vram={probe.get('IDLER_VRAM_KIB', '?')} KiB")
    for e in submitter_entries:
        print(f"  ps: submitter {describe(e)}")
    for e in idler_entries:
        print(f"  ps: idler     {describe(e)}")

    failures: list[str] = []

    # The positive case must hold, or "idler absent" is trivially true.
    if not submitter_here:
        elsewhere = [device_of(p) for p in submitter_entries]
        seen = f"; it appears only on device(s) {elsewhere}" if elsewhere else ""
        failures.append(
            f"submitter PID {submitter_pid} is absent from device {device} in ps "
            f"output ({len(procs)} entries) despite {submitter_cycles} engine "
            f"cycles and {probe.get('SUBMITTER_VRAM_KIB', '?')} KiB of VRAM{seen} "
            "-- the filter is dropping real clients")

    if idler_entries:
        devices = [e.get("device_id", "?") for e in idler_entries]
        failures.append(
            f"REGRESSION: idler PID {idler_pid} is listed on device(s) {devices} "
            f"with {probe.get('IDLER_VRAM_KIB', '?')} KiB of VRAM but zero engine "
            "cycles -- a VRAM holder that never ran work is being reported as an "
            "active process")

    if failures:
        for msg in failures:
            print(f"FAIL: {msg}")
        sys.exit(1)

    print(f"PASS: submitter PID {submitter_pid} listed ({submitter_cycles} cycles), "
          f"idler PID {idler_pid} omitted (0 cycles, "
          f"{probe.get('IDLER_VRAM_KIB', '?')} KiB VRAM, offered by sysman) -- "
          "process listing follows engine context, not VRAM ownership")


if __name__ == "__main__":
    main()
