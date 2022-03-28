cmake_minimum_required(VERSION 3.14.0)

if(NOT DEFINED WORK_PATH)
  set(WORK_PATH ${CMAKE_CURRENT_BINARY_DIR})
endif()

include(../.cmake/xpum_version.cmake)

add_custom_target(doc ALL)

add_custom_command(
  TARGET doc
  PRE_BUILD
  COMMAND /bin/sh -c "${CMAKE_CURRENT_LIST_DIR}/build.sh" DEPENDS
          "${CMAKE_CURRENT_LIST_DIR}/Doxyfile")

install(DIRECTORY build/html/ DESTINATION doc # PATTERN ".buildinfo" EXCLUDE
)
