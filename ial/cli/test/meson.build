# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

# IAL CLI unit tests (doctest)

doctest_dep = dependency('doctest', required: true)

# Builds the top-level parser without cli.cpp: the test defines the cli.cpp functions the
# parser calls, so no Level Zero driver is initialized.
default_parser_test = executable(
  'default_parser_test',
  ['default_parser_test.cpp', '../parser/default_parser.cpp'],
  include_directories: [
    global_inc,
    cli_inc,
    oal_inc_dirs,
    include_directories('../../cmn'),
    include_directories('../../../hal/core'),
    include_directories('../../../hal/fwupd'),
  ],
  dependencies: cli_deps + windows_deps + [doctest_dep],
  link_args: is_linux ? ['-pie'] : [],
  build_by_default: true,
  install: false,
)

test('default_parser_test', default_parser_test)
