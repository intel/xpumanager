# HAL amc (add-in-card management controller) build

amc_sources = files(
  'common.cpp',
  'mctp_control.cpp',
  'mctp_control_resp.cpp',
  'mctp_init.cpp',
  'mctp_protocol.cpp',
  'pldm_discovery.cpp',
  'pldm_discovery_resp.cpp',
  'pldm_fwpackage.cpp',
  'pldm_fwupdate.cpp',
  'pldm_fwupdate_helper.cpp',
  'pldm_fwupdate_resp.cpp',
  'pldm_init.cpp',
  'pldm_protocol.cpp',
  'pldm_amc_gpu_reset.cpp',
  'oem_vrsync.cpp',
  'amclib.cpp',
  'redfish.cpp',
  'pldm_fru.cpp',
  'pldm_fru_helper.cpp',
  'pldm_fru_resp.cpp',
  'pldm_platform.cpp',
  'pldm_pdr_manager.cpp',
)

amc_inc = include_directories('.')

# Firmware update static library
amc_lib = static_library('amc',
  amc_sources,
  include_directories : [
    global_inc, 
    amc_inc,
    oal_inc_dirs,
    include_directories('../core'),
    include_directories('../fwupd')
  ],
  dependencies : [levelzero_dep, oal_dep, igsc_dep],
  install : false
)

# FWUPD dependency for other components
amc_dep = declare_dependency(
  link_with : amc_lib,
  include_directories : amc_inc
)
