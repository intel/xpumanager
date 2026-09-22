# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT
#
# Integration test helpers: standalone Level Zero programs used by the YAML
# validation suite (validation/tests/) to create controlled GPU workloads.
#

process_memstress_exe = executable(
  'process_memstress',
  'process_memstress.cpp',
  include_directories: [global_inc],
  dependencies: [levelzero_dep, compat_dep],
  link_args: ['-pie'],
  build_by_default: true,
  install: false,
)

cross_gpu_alloc_exe = executable(
  'cross_gpu_alloc',
  'cross_gpu_alloc.cpp',
  include_directories: [global_inc],
  dependencies: [levelzero_dep, compat_dep],
  link_args: ['-pie'],
  build_by_default: true,
  install: false,
)