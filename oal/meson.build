# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# OAL (OS Abstraction Layer) build

# Platform-specific source files
oal_sources = []
oal_inc_dirs = [include_directories('.')] # Include the oal folder itself
oal_inc_dirs += [include_directories('../hal/core/logger')]
oal_deps = [thread_dep, levelzero_dep, bdf_dep]

if is_windows
  oal_sources = files(
    'win/dllmain.cpp',
    'win/fs_lock.cpp',
    'win/http_client.cpp',
    'win/i2c_interface.cpp',
    'win/thread.cpp',
    'win/win.cpp',
  )
  oal_inc_dirs += [include_directories('win')]
  oal_deps += windows_deps
elif is_linux
  oal_sources = files(
    'lin/dbg_log.cpp',
    'lin/dmi_reader.cpp',
    'lin/fs_lock.cpp',
    'lin/http_client.cpp',
    'lin/i2c_interface.cpp',
    'lin/lin.cpp',
    'lin/linvf.cpp',
    'lin/pci_database.cpp',
    'lin/topology.cpp',
  )
  oal_inc_dirs += [include_directories('lin')]
  # Add hwloc dependency for Linux topology functionality
  # Treat as system dependency to avoid warnings in hwloc headers
  hwloc_dep = dependency('hwloc', required: true, include_type: 'system')
  oal_deps += [hwloc_dep]
  # Add curl dependency for Linux redfish functionality
  curl_dep = dependency('libcurl', required: true)
  oal_deps += [curl_dep]
  # Add pciaccess dependency for PCI/SRIOV functionality
  pciaccess_dep = dependency('pciaccess', required: true)
  oal_deps += [pciaccess_dep]

  subdir('lin/test')
endif

# Set correct CRT flags for Windows with override capability
oal_cpp_args = []
if is_windows
  oal_cpp_args += ['-DLIBXPUM_EXPORTS']
  if get_option('buildtype').startswith('debug')
    # Use /FORCE to override any previous /MD settings from dependencies
    oal_cpp_args += ['/MDd', '/D_DEBUG']
  else
    oal_cpp_args += ['/MD', '/DNDEBUG']
  endif
endif

# OAL static library
oal_lib = static_library(
  'oal',
  oal_sources,
  include_directories: [global_inc, include_directories('../hal/core')] + oal_inc_dirs,
  dependencies: oal_deps,
  cpp_args: oal_cpp_args,
  install: false,
)

# OAL dependency for other components
oal_dep = declare_dependency(link_with: oal_lib, include_directories: oal_inc_dirs)
