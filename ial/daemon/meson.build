# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# IAL daemon build

daemon_sources = files('daemon.cpp')
daemon_inc = include_directories('.')

# Daemon dependencies
daemon_deps = [
  libxpum_dep,
  oal_dep,
  ial_cmn_dep, 
  levelzero_dep,
  igsc_dep,
  nlohmann_json_dep
]

if is_linux
  daemon_deps += [pciaccess_dep]
endif

# xpumd executable (daemon)
xpumd_exe = executable('xpumd',
  daemon_sources,
  include_directories : [
    global_inc,
    daemon_inc,
    oal_inc_dirs,
    include_directories('../cmn'),
    include_directories('../../hal/core'),
    include_directories('../../hal/fwupd')
  ],
  dependencies : daemon_deps + windows_deps,
  install : true,
  install_dir : get_option('bindir')
)
