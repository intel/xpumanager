# HAL fwupd (Firmware Update) build

fwupd_sources = files(
  'amcupd.cpp',
  'fwupd.cpp', 
  'gscupd.cpp'
)

fwupd_inc = include_directories('.')

# Set correct CRT flags for Windows with override capability
fwupd_cpp_args = []
if is_windows
  if get_option('buildtype').startswith('debug')
    fwupd_cpp_args += ['/MDd', '/D_DEBUG']
  else
    fwupd_cpp_args += ['/MD', '/DNDEBUG']
  endif
endif

# Firmware update static library
fwupd_lib = static_library('fwupd',
  fwupd_sources,
  include_directories : [
    global_inc, 
    fwupd_inc,
    oal_inc_dirs,
    include_directories('../core')
  ],
  dependencies : [levelzero_dep, oal_dep, igsc_dep],
  cpp_args : fwupd_cpp_args,
  install : false
)

# FWUPD dependency for other components
fwupd_dep = declare_dependency(
  link_with : fwupd_lib,
  include_directories : fwupd_inc
)
