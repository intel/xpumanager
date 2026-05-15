# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# HAL core (Hardware Abstraction Layer Core) build

hal_core_sources = files(
  'device.cpp',
  'diagnostic.cpp',
  'debug.cpp',
  'driver.cpp',
  'ecc.cpp',
  'enginegroup.cpp',
  'fabric.cpp',
  'fan.cpp',
  'firmware.cpp',
  'frequency.cpp',
  'memory.cpp',
  'metric.cpp',
  'pci.cpp',
  'performance.cpp',
  'power.cpp',
  'process.cpp',
  'ras.cpp',
  'scheduler.cpp',
  'standby.cpp',
  'sysman.cpp',
  'temperature.cpp',
  'threading.cpp',
  'vf.cpp',
  'file_io.cpp',
)

hal_core_inc = include_directories('.')

# extensions-only sources (dev branch only)
# These files are only included when building with -Dextensions=true
extensions_enabled = get_option('extensions')
extensions_inc_dirs = []

if extensions_enabled
  message('Extensions: Building with extended features enabled')
  subdir('extensions')
  hal_core_sources += hal_core_extension_sources
  extensions_inc_dirs = [hal_core_extension_inc]
else
  # External build uses rasexp.cpp (returns base ecc)
  hal_core_sources += files('rasexp.cpp')
  # External build uses offline_page.cpp
  hal_core_sources += files('offline_page.cpp')
endif

# Shared library
libxpum_shared = shared_library('xpum',
  hal_core_sources,
  include_directories : [
    global_inc,
    hal_core_inc,
    oal_inc_dirs,
    include_directories('../fwupd'),
    include_directories('../amc'),
    include_directories('../../ial/cli')  # For version.h
  ] + extensions_inc_dirs,
  dependencies : [
    levelzero_dep,
    igsc_dep,
    oal_dep,
    fwupd_dep,
    nlohmann_json_dep
  ] + windows_deps,
  version : xpum_version,
  soversion : xpum_version.split('.')[0],
  install : true,
  install_dir : get_option('libdir'),
)

# Static library
libxpum_static = static_library('xpum',
  hal_core_sources,
  include_directories : [
    global_inc,
    hal_core_inc,
    oal_inc_dirs,
    include_directories('../fwupd'),
    include_directories('../amc'),
    include_directories('../../ial/cli')  # For version.h
  ] + extensions_inc_dirs,
  dependencies : [
    levelzero_dep,
    igsc_dep,
    oal_dep,
    fwupd_dep,
    amc_dep,
    nlohmann_json_dep
  ] + windows_deps,
  install : get_option('static_library'),
  install_dir : get_option('libdir')
)

# Create dependency object for the HAL core library
libxpum_dep = declare_dependency(
  link_with : get_option('shared_library') ? libxpum_shared : libxpum_static,
  include_directories : hal_core_inc,
  dependencies : [levelzero_dep, igsc_dep, nlohmann_json_dep]
)

# On Windows builds, copy the DLL next to xpu-smi.exe so it can be
# found at runtime without modifying PATH
# Alternatively, `meson devenv -C {builddir}` will set the path automatically
# NOTE: if it fails, it may need to be ran in cmd or with `meson devenv -C {builddir} --shell=cmd`
if is_windows and get_option('shared_library')
  python = import('python').find_installation(required: true)
  custom_target('copy_xpum_dll',
    input: libxpum_shared,
    output: '.xpum_dll_copy.stamp',
    command: [
      python, '-c',
      'import shutil, sys, pathlib; dst = sys.argv[2]; pathlib.Path(dst).parent.mkdir(parents=True, exist_ok=True); shutil.copy2(sys.argv[1], dst); open(sys.argv[3], "w").close()',
      '@INPUT0@',
      meson.project_build_root() / 'ial' / 'cli' / ('xpum-' + major + '.dll'),
      '@OUTPUT0@',
    ],
    build_by_default: true,
    install: false,
  )
endif

# Install headers (optional)
if get_option('install_headers')
  hal_headers = files(
    'debug.h',
    'device.h',
    'diagnostic.h',
    'driver.h',
    'ecc.h',
    'enginegroup.h',
    'fabric.h',
    'fan.h',
    'firmware.h',
    'frequency.h',
    'jsonwrapper.h',
    'memory.h',
    'metric.h',
    'pci.h',
    'performance.h',
    'power.h',
    'ras.h',
    'scheduler.h',
    'standby.h',
    'sysman.h',
    'sysprocess.h',
    'temperature.h',
    'vf.h',
    'offline_page.h'
  )
  install_headers(hal_headers, subdir : 'xpum')

  logger_headers = files(
    'logger/log_level.h',
    'logger/log_record.h',
    'logger/formatters.h',
    'logger/sink_base.h',
    'logger/ostream_sink.h',
    'logger/filestream_sink.h',
    'logger/async_sink.h',
    'logger/logger.h',
  )
  install_headers(logger_headers, subdir : 'xpum/logger')
  
endif

# Unit tests for the Logger (optional — enable with -Dwith_tests=true)
if get_option('with_tests')
    doctest_dep = dependency('doctest', required: true)
    logger_test = executable(
        'logger_test',
        # Compile debug.cpp directly so dbgLvl and the extended helpers are
        # defined without linking the full xpum library.
        files('logger/test/logger_test.cpp', 'debug.cpp'),
        include_directories: [global_inc, hal_core_inc],
        dependencies: [doctest_dep],
        link_args: is_linux ? ['-pie'] : [],
        build_by_default: true,
        install: false,
    )
    test('logger_tests', logger_test)
    message('Logger unit tests enabled')
else
    message('Skipping logger tests (pass -Dwith_tests=true to enable)')
endif
