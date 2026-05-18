# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# IAL common (Command implementations) build

ial_cmn_sources = files(
  '../../hal/core/debug.cpp',
  'metrics/temperature_metrics.cpp',
  'metrics/utilization.cpp',
  'metrics/pci.cpp',
  'metrics/power.cpp',
  'cmd_agentset.cpp',
  'cmd_amc.cpp',
  'cmd_config.cpp',
  'cmd_diag.cpp',
  'cmd_discovery.cpp',
  'cmd_dump.cpp',
  'cmd_group.cpp',
  'cmd_health.cpp',
  'cmd_listgpu.cpp',
  'cmd_log.cpp',
  'cmd_policy.cpp',
  'cmd_ps.cpp',
  'cmd_smi.cpp',
  'cmd_stats.cpp',
  'cmd_topdown.cpp',
  'cmd_topology.cpp',
  'cmd_updatefw.cpp',
  'cmd_vgpu.cpp',
  'cmds.cpp',
  'metrics_registry.cpp',
  'printer.cpp',
  'metrics/eu_array.cpp',
  'metrics/fan.cpp',
  'metrics/memory.cpp',
)

ial_cmn_inc = include_directories('.')

# Platform-specific dependencies
ial_cmn_deps = [
  libxpum_dep,
  oal_dep,
  nlohmann_json_dep,
  amc_dep,
  fwupd_dep,
  table_dep,
  bdf_dep,
]
if is_linux
  ial_cmn_deps += [pciaccess_dep]
endif

# Windows-specific compiler flags
ial_cmn_cpp_args = []
if is_windows
  ial_cmn_cpp_args += [
    '/wd4100', # Disable C4100: unreferenced parameter (from hwloc headers)
    '-D_CRT_SECURE_NO_WARNINGS', # Disable CRT security warnings
  ]
endif

# Common commands static library
ial_cmn_lib = static_library(
  'ial_cmn',
  ial_cmn_sources,
  include_directories: [
    global_inc,
    ial_cmn_inc,
    oal_inc_dirs,
    include_directories('../../hal/core'),
    include_directories('../../hal/fwupd'),
    include_directories('../../hal/amc'),
    include_directories('../../utility/table'),
  ],
  dependencies: ial_cmn_deps,
  cpp_args: ial_cmn_cpp_args,
  install: false,
)

# IAL common dependency for CLI and daemon
ial_cmn_dep = declare_dependency(
  link_with: ial_cmn_lib,
  include_directories: ial_cmn_inc,
  dependencies: [table_dep],
)

if get_option('with_tests')
  subdir('test')
endif
