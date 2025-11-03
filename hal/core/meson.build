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
  'file_io.cpp'
)

hal_core_inc = include_directories('.')

# Shared library
libxpum_shared = shared_library('xpum',
  hal_core_sources,
  include_directories : [
    global_inc,
    hal_core_inc,
    oal_inc_dirs,
    include_directories('../fwupd'),
    include_directories('../amc')
  ],
  dependencies : [
    levelzero_dep, 
    igsc_dep, 
    oal_dep,
    fwupd_dep,
    nlohmann_json_dep
  ] + windows_deps,
  version : meson.project_version(),
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
    include_directories('../amc')
  ],
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
    'vf.h'
  )
  
  install_headers(hal_headers, subdir : 'xpum')
endif
