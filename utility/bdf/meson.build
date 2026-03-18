# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# BDF utility (header-only)

bdf_inc = include_directories('.')

bdf_dep = declare_dependency(
    include_directories: [bdf_inc],
)

# Unit tests (optional — enable with -Dwith_tests=true)
if get_option('with_tests')
    doctest_dep = dependency('doctest', required: true)
    bdf_test = executable(
        'bdf_test',
        'test/bdf_test.cpp',
        include_directories: [bdf_inc],
        dependencies: [doctest_dep],
        link_args: is_linux ? ['-pie'] : [],
        build_by_default: true
    )

    test('bdf_tests', bdf_test)

    message('Unit tests enabled for bdf')
else
    message('Skipping bdf tests (pass -Dwith_tests=true to enable)')
endif
