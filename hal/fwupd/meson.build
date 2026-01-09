# HAL fwupd (Firmware Update) build

fwupd_sources = files(
  'amcupd.cpp',
  'fwupd.cpp', 
  'gscupd.cpp'
)

fwupd_inc = include_directories('.')

# Firmware update static library
fwupd_lib = static_library('fwupd',
  fwupd_sources,
  objects : amc_lib.extract_all_objects(recursive : true),
  include_directories : [
    global_inc, 
    fwupd_inc,
    oal_inc_dirs,
    include_directories('../core'),
    include_directories('../amc')
  ],
  dependencies : [levelzero_dep, oal_dep, igsc_dep, nlohmann_json_dep],
  install : false
)

# FWUPD dependency for other components
fwupd_dep = declare_dependency(
  link_with : fwupd_lib,
  include_directories : fwupd_inc
)
