# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Unit tests for XPU Manager components

if get_option('with_tests')
  doctest_dep = dependency('doctest', required: true)

  # OAL diagnostics log tests
  # Tests filesystem operations, stream handling, and error scenarios
  dbg_log_test = executable(
    'dbg_log_test',
    'dbg_log_test.cpp',
    include_directories: [global_inc],
    dependencies: [doctest_dep],
    link_args: is_linux ? ['-pie'] : [],
    build_by_default: true,
  )

  # Register tests with meson
  test('dbg_log_tests', dbg_log_test)

  message('Unit tests enabled for OAL diagnostics')
else
  message('Skipping OAL diagnostic tests (pass -Dwith_tests=true to enable)')
endif
