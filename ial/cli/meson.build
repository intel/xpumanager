# IAL CLI build

cli_sources = files(
    'cli.cpp',
    '../../hal/core/debug.cpp',
)
cli_inc = include_directories('.')

# CLI dependencies
cli_deps = [
  libxpum_dep,
  oal_dep, 
  ial_cmn_dep,
  levelzero_dep,
  igsc_dep,
  nlohmann_json_dep
]

if is_linux
  cli_deps += [pciaccess_dep]
endif

# Set correct CRT flags for Windows CLI executables
cli_cpp_args = []
cli_link_args = []

if is_windows
  if get_option('buildtype').startswith('debug')
    cli_cpp_args += ['/MDd', '/D_DEBUG']  # Dynamic debug CRT for debug builds
    cli_link_args = []                    # No CRT exclusion for debug builds
  else
    cli_cpp_args += ['/MD', '/DNDEBUG']   # Dynamic release CRT for release builds
    cli_link_args = ['/NODEFAULTLIB:LIBCMT']  # Exclude LIBCMT for release builds
  endif
endif

# xpu-smi executable (SMI mode)
xpu_smi_exe = executable('xpu-smi',
  cli_sources,
  include_directories : [
    global_inc,
    cli_inc,
    oal_inc_dirs,
    include_directories('../cmn'),
    include_directories('../../hal/core'),
    include_directories('../../hal/fwupd')
  ],
  dependencies : cli_deps + windows_deps,
  cpp_args : cli_cpp_args,
  link_args : cli_link_args,
  install : true,
  install_dir : get_option('bindir')
)

# xpumcli executable (CLI/daemon mode)
xpumcli_exe = executable('xpumcli',
  cli_sources,
  include_directories : [
    global_inc,
    cli_inc,
    oal_inc_dirs,
    include_directories('../cmn'),
    include_directories('../../hal/core'),
    include_directories('../../hal/fwupd')
  ],
  dependencies : cli_deps + windows_deps,
  cpp_args : cli_cpp_args + ['-DDAEMONMODE'],
  link_args : cli_link_args,
  install : true,
  install_dir : get_option('bindir')
)
