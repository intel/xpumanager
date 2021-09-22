cmake_minimum_required (VERSION 3.2.0)

project (hwloc)

add_custom_target(temp_target ALL)

add_custom_command(TARGET temp_target
  PRE_BUILD
  COMMAND /bin/sh -c "${CMAKE_CURRENT_LIST_DIR}/build.sh"
  DEPENDS "include/hwloc.h")
	


