# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Process-wide hang watchdog and the --timeout option. Linked into the xpu-smi executable only:
# the armed instance is found through a variable in watchdog.cpp, which a second module would
# not share.

watchdog_inc = include_directories('.')

watchdog_lib = static_library(
    'watchdog',
    ['watchdog.cpp', 'timeout_option.cpp'],
    include_directories: [watchdog_inc],
    dependencies: [thread_dep],
    install: false,
)

watchdog_dep = declare_dependency(
    include_directories: [watchdog_inc, global_inc],
    link_with: watchdog_lib,
    dependencies: [thread_dep],
)

# Unit tests (optional - enable with -Dwith_tests=true)
if get_option('with_tests')
    doctest_dep = dependency('doctest', required: true)

    watchdog_test = executable(
        'watchdog_test',
        'test/watchdog_test.cpp',
        dependencies: [doctest_dep, watchdog_dep],
        link_args: is_linux ? ['-pie'] : [],
        build_by_default: true,
    )

    test('watchdog_tests', watchdog_test)

    timeout_option_test = executable(
        'timeout_option_test',
        'test/timeout_option_test.cpp',
        dependencies: [doctest_dep, watchdog_dep],
        link_args: is_linux ? ['-pie'] : [],
        build_by_default: true,
    )

    test('timeout_option_tests', timeout_option_test)

    message('Unit tests enabled for watchdog')
else
    message('Skipping watchdog tests (pass -Dwith_tests=true to enable)')
endif
