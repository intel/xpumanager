cmake_minimum_required (VERSION 3.2.0)

project (HWLOC)

add_custom_target(lib_hwloc ALL)

add_custom_command(TARGET lib_hwloc
  PRE_BUILD
  COMMAND /bin/sh -c "${CMAKE_CURRENT_LIST_DIR}/build.sh"
  DEPENDS "include/hwloc.h")
	


