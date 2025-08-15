# OAL (OS Abstraction Layer) build

# Platform-specific source files
oal_sources = []
oal_inc_dirs = []
oal_deps = [thread_dep, levelzero_dep]

if is_windows
  oal_sources = files(
    'win/i2c_interface.cpp',
    'win/dllmain.cpp',
    'win/thread.cpp', 
    'win/win.cpp'
  )
  oal_inc_dirs = [include_directories('win'), include_directories('.')]
  oal_deps += windows_deps
elif is_linux
  oal_sources = files(
    'lin/pci_database.cpp',
    'lin/i2c_interface.cpp',
    'lin/lin.cpp'
  )
  oal_inc_dirs = [include_directories('lin'), include_directories('.')]
  # Add hwloc dependency for Linux topology functionality
  hwloc_dep = dependency('hwloc', required: true)
  oal_deps += [hwloc_dep]
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
oal_lib = static_library('oal',
  oal_sources,
  include_directories : [
    global_inc, 
    include_directories('../hal/core')  # For debug.h
  ] + oal_inc_dirs,
  dependencies : oal_deps,
  cpp_args : oal_cpp_args,
  install : false
)

# OAL dependency for other components
oal_dep = declare_dependency(
  link_with : oal_lib,
  include_directories : oal_inc_dirs
)
