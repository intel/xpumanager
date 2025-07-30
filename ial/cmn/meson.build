# IAL common (Command implementations) build

ial_cmn_sources = files(
  'cmd_agentset.cpp',
  'cmd_amcsensor.cpp', 
  'cmd_config.cpp',
  'cmd_diag.cpp',
  'cmd_discovery.cpp',
  'cmd_dump.cpp',
  'cmd_group.cpp',
  'cmd_health.cpp',
  'cmd_log.cpp',
  'cmd_oob.cpp',
  'cmd_policy.cpp',
  'cmd_ps.cpp',
  'cmd_stats.cpp',
  'cmd_topdown.cpp',
  'cmd_topology.cpp',
  'cmd_updatefw.cpp',
  'cmd_vgpu.cpp',
  'cmds.cpp'
)

ial_cmn_inc = include_directories('.')

# Platform-specific dependencies
ial_cmn_deps = [libxpum_dep, oal_dep]
if is_linux
  ial_cmn_deps += [pciaccess_dep]
endif

# Common commands static library
ial_cmn_lib = static_library('ial_cmn',
  ial_cmn_sources,
  include_directories : [
    global_inc,
    ial_cmn_inc,
    oal_inc_dirs,
    include_directories('../../hal/core'),
    include_directories('../../hal/fwupd')
  ],
  dependencies : ial_cmn_deps,
  install : false
)

# IAL common dependency for CLI and daemon
ial_cmn_dep = declare_dependency(
  link_with : ial_cmn_lib,
  include_directories : ial_cmn_inc
)
