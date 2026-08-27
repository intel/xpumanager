# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

doctest_dep = dependency('doctest', required: true)

scope_exit_test = executable(
    'scope_exit_test',
    'scope_exit_test.cpp',
    include_directories: [include_directories('..')],
    dependencies: [doctest_dep],
    link_args: is_linux ? ['-pie'] : [],
    build_by_default: true,
)

test('scope_exit_tests', scope_exit_test)
