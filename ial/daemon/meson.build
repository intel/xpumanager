# IAL daemon build

daemon_sources = files('daemon.cpp')
daemon_inc = include_directories('.')

# Daemon dependencies
daemon_deps = [
  libxpum_dep,
  oal_dep,
  ial_cmn_dep, 
  levelzero_dep,
  igsc_dep,
  nlohmann_json_dep
]

if is_linux
  daemon_deps += [pciaccess_dep]
endif

# Set correct CRT flags for Windows daemon executable
daemon_cpp_args = ['-DDAEMONMODE']
daemon_link_args = []

if is_windows
  if get_option('buildtype').startswith('debug')
    daemon_cpp_args += ['/MDd', '/D_DEBUG']  # Dynamic debug CRT for debug builds
    daemon_link_args = []                    # No CRT exclusion for debug builds
  else
    daemon_cpp_args += ['/MD', '/DNDEBUG']   # Dynamic release CRT for release builds
    daemon_link_args = ['/NODEFAULTLIB:LIBCMT']  # Exclude LIBCMT for release builds
  endif
endif

# xpumd executable (daemon)
xpumd_exe = executable('xpumd',
  daemon_sources,
  include_directories : [
    global_inc,
    daemon_inc,
    oal_inc_dirs,
    include_directories('../cmn'),
    include_directories('../../hal/core'),
    include_directories('../../hal/fwupd')
  ],
  dependencies : daemon_deps + windows_deps,
  cpp_args : daemon_cpp_args,
  link_args : daemon_link_args,
  install : true,
  install_dir : get_option('bindir')
)
