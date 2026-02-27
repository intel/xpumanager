# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Table utility library

# Include directories
table_inc = include_directories('.')

# Source files
table_sources = files(
    'table_builder.cpp',
)

# Table library
table_lib = static_library(
    'table',
    sources: table_sources,
    include_directories: [global_inc, table_inc],
    dependencies: [nlohmann_json_dep],
    install: false
)

# Dependency for other parts of the project
table_dep = declare_dependency(
    include_directories: [table_inc],
    link_with: table_lib,
    dependencies: [nlohmann_json_dep]
)

# Example application demonstrating GPU monitoring output
table_example = executable(
    'table_example',
    'test/example.cpp',
    include_directories: [global_inc, table_inc],
    link_with: table_lib,
    dependencies: [nlohmann_json_dep],
    link_args : is_linux ? ['-pie'] : [],
    build_by_default: true,
    install: false
)

# Unit tests (optional, requires boost-ext-ut — enable with -Dwith_tests=true)
if get_option('with_tests')
    ut_dep = dependency('boost-ext-ut', required: true)
    table_test = executable(
        'table_builder_test',
        'test/table_builder_test.cpp',
        include_directories: [global_inc, table_inc],
        link_with: table_lib,
        dependencies: [nlohmann_json_dep, ut_dep],
        link_args: is_linux ? ['-pie'] : [],
        build_by_default: true
    )

    # Register test with meson
    test('table_builder_tests', table_test)

    message('Unit tests enabled for table_builder')
else
    message('Skipping table_builder tests (pass -Dwith_tests=true to enable)')
endif
