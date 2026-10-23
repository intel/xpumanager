# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

# HAL amc unit tests (doctest)

doctest_dep = dependency('doctest', required: true)

# MCTP packetization / reassembly (DSP0236, DSP0237). Links amc_lib only for crc8Smbus().
mctp_packet_test = executable(
  'mctp_packet_test',
  'mctp_packet_test.cpp',
  include_directories: [global_inc, amc_inc, oal_inc_dirs],
  link_with: amc_lib,
  dependencies: [doctest_dep, levelzero_dep, oal_dep, igsc_dep, nlohmann_json_dep, compat_dep],
  link_args: is_linux ? ['-pie'] : [],
  build_by_default: true,
  install: false,
)

test('mctp_packet_test', mctp_packet_test)

# SIGINT/SIGTERM handling around AMC firmware updates. Uses fork(), so Linux only.
if is_linux
  amc_update_interrupt_test = executable(
    'amc_update_interrupt_test',
    'amc_update_interrupt_test.cpp',
    include_directories: [global_inc, amc_inc, oal_inc_dirs],
    link_with: amc_lib,
    dependencies: [doctest_dep, levelzero_dep, oal_dep, igsc_dep, nlohmann_json_dep, compat_dep],
    link_args: ['-pie'],
    build_by_default: true,
    install: false,
  )

  test('amc_update_interrupt_test', amc_update_interrupt_test)
endif
