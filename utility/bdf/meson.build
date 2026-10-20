# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# BDF utility (header-only)

bdf_inc = include_directories('.')

# pci_address.h formats through the compat shim, included as "utility/compat/format.h"
bdf_dep = declare_dependency(
    include_directories: [bdf_inc, global_inc],
    dependencies: [compat_dep],
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

    pci_address_test = executable(
        'pci_address_test',
        'test/pci_address_test.cpp',
        dependencies: [doctest_dep, bdf_dep],
        link_args: is_linux ? ['-pie'] : [],
        build_by_default: true
    )

    test('pci_address_tests', pci_address_test)

    # The same tests with the compat shim on {fmt}, as on GCC 12, where {fmt} is available
    if fmt_dep.found()
        pci_address_fmt_test = executable(
            'pci_address_fmt_test',
            'test/pci_address_test.cpp',
            cpp_args: ['-DXPUM_COMPAT_FORMAT_USE_FMT'],
            dependencies: [doctest_dep, bdf_dep],
            link_args: is_linux ? ['-pie'] : [],
            build_by_default: true
        )

        test('pci_address_fmt_tests', pci_address_fmt_test)
    endif

    message('Unit tests enabled for bdf')
else
    message('Skipping bdf tests (pass -Dwith_tests=true to enable)')
endif
