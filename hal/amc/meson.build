# HAL amc (add-in-card management controller) build

amc_sources = files(
  'common.cpp',
)

amc_inc = include_directories('.')

# Firmware update static library
amc_lib = static_library('amc',
  amc_sources,
  include_directories : [
    global_inc, 
    amc_inc,
    oal_inc_dirs,
    include_directories('../core')
  ],
  dependencies : [levelzero_dep, oal_dep, igsc_dep],
  install : false
)

# FWUPD dependency for other components
amc_dep = declare_dependency(
  link_with : amc_lib,
  include_directories : amc_inc
)
