# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Logger: header-only library
# All types and templates are defined in the headers; no .cpp compilation unit.

logger_inc = include_directories('.')

logger_dep = declare_dependency(
    include_directories: [logger_inc],
    dependencies: [compat_dep],
)

if get_option('with_tests')
    doctest_dep = dependency('doctest', required: true)
    hal_core_inc = include_directories('../../hal/core')
    logger_test = executable(
        'logger_test',
        files('test/logger_test.cpp', '../../hal/core/debug.cpp'),
        include_directories: [global_inc, logger_inc, hal_core_inc],
        dependencies: [doctest_dep, compat_dep],
        link_args: is_linux ? ['-pie'] : [],
        build_by_default: true,
        install: false,
    )
    test('logger_tests', logger_test)
    message('Logger unit tests enabled')
else
    message('Skipping logger tests (pass -Dwith_tests=true to enable)')
endif
