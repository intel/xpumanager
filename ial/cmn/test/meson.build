# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

# IAL common unit tests (doctest)

doctest_dep = dependency('doctest', required: true)

ial_cmn_test_deps = ial_cmn_deps + [doctest_dep]

metrics_registry_test = executable(
  'metrics_registry_test',
  'metrics_registry_test.cpp',
  include_directories: [
    global_inc,
    ial_cmn_inc,
  ],
  link_with: ial_cmn_lib,
  dependencies: ial_cmn_test_deps,
  link_args: is_linux ? ['-pie'] : [],
  build_by_default: true,
  install: false,
)

test('metrics_registry_test', metrics_registry_test)

if is_linux
  topology_test = executable(
    'topology_test',
    'topology_test.cpp',
    include_directories: [
      global_inc,
      ial_cmn_inc,
    ],
    link_with: [ial_cmn_lib],
    dependencies: ial_cmn_test_deps,
    link_args: ['-pie'],
    build_by_default: true,
    install: false,
  )

  test('topology_test', topology_test)

  dump_test = executable(
    'dump_test',
    'dump_test.cpp',
    include_directories: [
      global_inc,
      ial_cmn_inc,
    ],
    link_with: ial_cmn_lib,
    dependencies: ial_cmn_test_deps,
    cpp_args: ial_cmn_cpp_args,
    link_args: is_linux ? ['-pie'] : [],
    build_by_default: true,
    install: false,
  )

  test('dump_test', dump_test)

  vgpu_test = executable(
    'vgpu_test',
    'vgpu_test.cpp',
    include_directories: [
      global_inc,
      ial_cmn_inc,
    ],
    link_with: [ial_cmn_lib],
    dependencies: ial_cmn_test_deps,
    link_args: ['-pie'],
    build_by_default: true,
    install: false,
  )

  test('vgpu_test', vgpu_test)

endif
