# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Time limits for driver calls that can block forever (header-only)

circuit_breaker_inc = include_directories('.')

circuit_breaker_dep = declare_dependency(
    include_directories: [circuit_breaker_inc],
    dependencies: [thread_dep],
)

# Unit tests (optional - enable with -Dwith_tests=true)
if get_option('with_tests')
    doctest_dep = dependency('doctest', required: true)

    circuit_breaker_test = executable(
        'circuit_breaker_test',
        'test/circuit_breaker_test.cpp',
        dependencies: [doctest_dep, circuit_breaker_dep],
        link_args: is_linux ? ['-pie'] : [],
        build_by_default: true,
    )

    test('circuit_breaker_tests', circuit_breaker_test)
endif
