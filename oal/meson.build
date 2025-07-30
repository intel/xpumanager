# OAL (OS Abstraction Layer) build

# Platform-specific source files
oal_sources = []
oal_inc_dirs = []

if is_windows
  oal_sources = files(
    'win/dllmain.cpp',
    'win/thread.cpp', 
    'win/win.cpp'
  )
  oal_inc_dirs = include_directories('win')
elif is_linux
  oal_sources = files(
    'lin/lin.cpp'
  )
  oal_inc_dirs = include_directories('lin')
endif

# OAL static library
oal_lib = static_library('oal',
  oal_sources,
  include_directories : [
    global_inc, 
    oal_inc_dirs,
    include_directories('../hal/core')  # For debug.h
  ],
  dependencies : [thread_dep, levelzero_dep] + windows_deps,
  cpp_args : is_windows ? ['-DLIBXPUM_EXPORTS'] : [],
  install : false
)

# OAL dependency for other components
oal_dep = declare_dependency(
  link_with : oal_lib,
  include_directories : oal_inc_dirs
)
