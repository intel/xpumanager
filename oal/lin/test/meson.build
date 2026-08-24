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

  # Crash log pure helper tests (command building and iclg output parsing).
  # Compiles crashlog_lin.cpp directly so the test needs no OAL link.
  crashlog_lin_test = executable(
    'crashlog_lin_test',
    ['crashlog_lin_test.cpp', '../crashlog_lin.cpp'],
    include_directories: [global_inc, include_directories('..')],
    dependencies: [doctest_dep],
    link_args: is_linux ? ['-pie'] : [],
    build_by_default: true,
  )

  test('crashlog_lin_test', crashlog_lin_test)

  # $PATH executable lookup tests. Compiles path_lookup.cpp directly so the
  # test needs no OAL link.
  path_lookup_test = executable(
    'path_lookup_test',
    ['path_lookup_test.cpp', '../path_lookup.cpp'],
    include_directories: [global_inc, include_directories('..')],
    dependencies: [doctest_dep],
    link_args: is_linux ? ['-pie'] : [],
    build_by_default: true,
  )

  test('path_lookup_test', path_lookup_test)

  message('Unit tests enabled for OAL diagnostics')
else
  message('Skipping OAL diagnostic tests (pass -Dwith_tests=true to enable)')
endif
