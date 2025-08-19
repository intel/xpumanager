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
  'vf.cpp'
)

hal_core_inc = include_directories('.')

# Determine correct CRT linker arguments for MSVC
xpum_link_args = []
xpum_cpp_args = []

if is_windows
  # Set correct CRT flags at target level
  if get_option('buildtype').startswith('debug')
    xpum_cpp_args += ['/MDd', '/D_DEBUG']  # Dynamic debug CRT for debug builds
    xpum_link_args = []        # No CRT exclusion for debug builds
  else
    xpum_cpp_args += ['/MD', '/DNDEBUG']   # Dynamic release CRT for release builds
    xpum_link_args = ['/NODEFAULTLIB:LIBCMT']  # Exclude LIBCMT for release builds
  endif

  xpum_cpp_args += ['-DLIBXPUM_EXPORTS']
endif

# Print cpp_args for debugging
message('HAL Core xpum_cpp_args: ' + ' '.join(xpum_cpp_args))

# Shared library
libxpum_shared = shared_library('xpum',
  hal_core_sources,
  include_directories : [
    global_inc,
    hal_core_inc,
    oal_inc_dirs,
    include_directories('../fwupd')
  ],
  dependencies : [
    levelzero_dep, 
    igsc_dep, 
    oal_dep, 
    fwupd_dep,
    nlohmann_json_dep
  ] + windows_deps,
  cpp_args : xpum_cpp_args,
  version : meson.project_version(),
  install : true,
  install_dir : get_option('libdir'),
  link_args : xpum_link_args
)

# Static library
libxpum_static = static_library('xpum',
  hal_core_sources,
  include_directories : [
    global_inc,
    hal_core_inc, 
    oal_inc_dirs,
    include_directories('../fwupd')
  ],
  dependencies : [
    levelzero_dep,
    igsc_dep,
    oal_dep,
    fwupd_dep,
    nlohmann_json_dep
  ] + windows_deps,
  cpp_args : xpum_cpp_args,
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
