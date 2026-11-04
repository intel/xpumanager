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

# Every AMC path over the MCTP transport, against a scripted AMC (amc_simulator.h) behind the AmcBus interface:
# MCTP control and PLDM discovery, retries with the same Instance ID, reply matching, multi-packet FRU replies.
amc_transport_test = executable(
  'amc_transport_test',
  'amc_transport_test.cpp',
  # pldm.h pulls in the same headers amc_lib is built with (debug.h from hal/core), and the test sets the log level
  include_directories: [global_inc, amc_inc, oal_inc_dirs, include_directories('../../core', '../../fwupd'), logger_inc],
  link_with: amc_lib,
  dependencies: [doctest_dep, levelzero_dep, oal_dep, igsc_dep, nlohmann_json_dep, compat_dep],
  link_args: is_linux ? ['-pie'] : [],
  build_by_default: true,
  install: false,
)

test('amc_transport_test', amc_transport_test, timeout: 60)

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
