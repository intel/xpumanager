cmake_minimum_required (VERSION 3.2.0)

project (HWLOC)

add_custom_target(lib_hwloc ALL)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(BUILD_TYPE_DEFINE "Debug")
else()    
  set(BUILD_TYPE_DEFINE "Release")
endif()

Message(STATUS "Build hwloc mode:${BUILD_TYPE_DEFINE}")

add_custom_command(TARGET lib_hwloc
  PRE_BUILD
  COMMAND /bin/sh -c "${CMAKE_CURRENT_LIST_DIR}/build.sh ${CMAKE_BINARY_DIR} ${BUILD_TYPE_DEFINE}"
  DEPENDS "include/hwloc.h")
	


