# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# IAL CLI build
cli_sources = files(
  '../../hal/core/debug.cpp',
  'parser/default_parser.cpp',
  'cli.cpp',
)

cli_inc = include_directories('.')

# CLI dependencies
cli_deps = [
  libxpum_dep,
  oal_dep,
  ial_cmn_dep,
  levelzero_dep,
  igsc_dep,
  nlohmann_json_dep,
  cli11_dep,
]

if is_linux
  cli_deps += [pciaccess_dep]
endif

# xpu-smi executable (SMI mode)
xpu_smi_exe = executable(
  'xpu-smi',
  cli_sources,
  include_directories: [
    global_inc,
    cli_inc,
    oal_inc_dirs,
    include_directories('../cmn'),
    include_directories('../../hal/core'),
    include_directories('../../hal/fwupd'),
  ],
  dependencies: cli_deps + windows_deps,
  link_args: is_linux ? ['-pie'] : [],
  install: true,
  install_dir: get_option('bindir'),
)