# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

"""gcovr filter / exclude patterns for the xpu-smi relevant-coverage report.

Centralised so that ``run_coverage.py`` and any CI integrations import the
same list.  Patterns are gcovr-style regexes matched against repo-relative
source paths (gcovr is invoked with ``--root=<repo_root>``).

Excluded categories
-------------------
- Tests themselves
- Legacy / deprecated subcommands slated for removal
  (agentset, group, policy, topdown, amc) and the C API shim (xpum_api).
- AMC + firmware-update paths needing out-of-band MCTP/PLDM HW or real
  FW images
- Daemon-mode-only code (cmd_smi.cpp — only reachable via xpumd)
- Dead / unwired code (metrics_registry)
- Hardware-state-dependent HAL (diagnostic, fabric, fan, vf, ras, standby,
  scheduler, performance, sysman, enginegroup, firmware, file_io, ecc,
  pci.cpp, metric) — needs specific HW / real GPU workloads
- CLI orchestrators over excluded HAL (cmd_diag) that would otherwise
  inflate the denominator without adding testable logic
- OS-specific platform layer (Linux syscall / sysfs wrappers)
- Infrastructure / template-instantiation artifacts in headers that gcov
  counts per-instantiation but which do not represent real uncovered
  logic paths
"""

# Source filter: only consider these top-level dirs.
FILTERS = [
    r"(ial|oal|hal|utility)/",
]

# Source excludes (applied after FILTERS).
EXCLUDES = [
    # ---- test sources themselves ----
    r".*test.*",
    r".*_test\.cpp",
    # ---- legacy / deprecated subcommands ----
    r"ial/cmn/cmd_agentset\.",
    r"ial/cmn/cmd_group\.",
    r"ial/cmn/cmd_policy\.",
    r"ial/cmn/cmd_topdown\.",
    r"ial/cmn/cmd_amc\.",
    r"ial/api/xpum_api\.cpp",
    # ---- daemon-only code (only reachable via xpumd) ----
    r"ial/cmn/cmd_smi\.cpp",
    # ---- dead / unwired code ----
    r"ial/cmn/metrics_registry\.",
    # ---- AMC + firmware-update paths needing real FW images ----
    r"hal/amc/",
    r"hal/fwupd/",
    # ---- hardware-state-dependent HAL code (needs specific HW) ----
    r"hal/core/diagnostic\.",
    r"hal/core/fabric\.",
    r"hal/core/fan\.",
    r"hal/core/vf\.",
    r"hal/core/ras\.",
    r"hal/core/standby\.",
    r"hal/core/scheduler\.",
    r"hal/core/performance\.",
    r"hal/core/sysman\.",
    r"hal/core/enginegroup\.",
    r"hal/core/firmware\.",
    r"hal/core/file_io\.",
    r"hal/core/ecc\.",
    r"hal/core/pci\.cpp",
    r"hal/core/metric\.",
    # ---- CLI orchestrators over excluded HAL ----
    r"ial/cmn/cmd_diag\.",
    # ---- OS-specific platform layer (Linux syscall wrappers) ----
    r"oal/lin/lin\.cpp",
    r"oal/lin/lin\.h",
    r"oal/lin/linvf\.",
    r"oal/lin/diag_lin\.",
    r"oal/lin/i2c_interface\.",
    r"oal/lin/topology\.",
    r"oal/lin/fs_lock\.",
    r"oal/lin/oslin\.h",
    r"oal/lin/dmi_reader\.",
    r"oal/lin/http_client\.",
    r"oal/lin/pci_database\.",
    r"oal/i2c_interface\.h",
    r"oal/os\.h",
    r"oal/osvf\.h",
    # ---- infrastructure / template instantiation artifacts ----
    # NOTE: logger.h, filestream_sink.h and table_builder.h are now
    # exercised by the doctest unit-test phase (`meson test`) that
    # run_coverage.py runs before the e2e phase, so they are no longer
    # excluded.  Keep ``threading.`` excluded \u2014 it's a header-only
    # template helper with no dedicated test.
    r"hal/core/threading\.",
]


def gcovr_filter_args() -> list[str]:
    """Return the ``--filter`` / ``--exclude`` argv slice for gcovr."""
    args: list[str] = []
    for pat in FILTERS:
        args += ["--filter", pat]
    for pat in EXCLUDES:
        args += ["--exclude", pat]
    return args
