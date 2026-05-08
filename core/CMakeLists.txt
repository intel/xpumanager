#
# Copyright (C) 2021-2024 Intel Corporation
# SPDX-License-Identifier: MIT
# @file CMakeLists.txt
#
cmake_minimum_required(VERSION 3.14.0)

project(xpum)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set compilation options
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++14" COMPILER_SUPPORTS_CXX14)
if(COMPILER_SUPPORTS_CXX14)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
else()
  message(
    STATUS
      "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support.  Please use a different C++ compiler."
  )
endif()

set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -Wall -pthread -fPIC -Wl,--exclude-libs,ALL")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -s")

include(../.cmake/xpum_version.cmake)

option(XPUM_ZE_HANDLE_LOCK_LOG "Log ZE Handle Locks" OFF)
if(XPUM_ZE_HANDLE_LOCK_LOG)
  add_definitions(-DXPUM_ZE_HANDLE_LOCK_LOG)
endif(XPUM_ZE_HANDLE_LOCK_LOG)

option(TRACE_SCHEDULED_TASK_RUN "Log XPUM Scheduled Task Trace" OFF)
if(TRACE_SCHEDULED_TASK_RUN)
  add_definitions(-DTRACE_SCHEDULED_TASK_RUN)
endif(TRACE_SCHEDULED_TASK_RUN)

if(NOT DEFINED XPUM_VERSION_STRING)
  set(XPUM_VERSION_STRING 0.1.0)
endif()

include(CheckIncludeFile)
check_include_file(pciaccess.h HAVE_PCIACCESS_H)

configure_file(${CMAKE_CURRENT_LIST_DIR}/src/infrastructure/xpum_config.h.in
               ${CMAKE_CURRENT_LIST_DIR}/src/infrastructure/xpum_config.h @ONLY)

# Specifiy link file location
link_directories(${CMAKE_CURRENT_LIST_DIR}/../build/hwloc/lib)
link_directories(${CMAKE_CURRENT_LIST_DIR}/../build/third_party/spdlog)
link_directories(${CMAKE_CURRENT_LIST_DIR}/../build/third_party/pcm/pcm-iio-gpu)

# Specifiy header file location
include_directories(${CMAKE_CURRENT_LIST_DIR}/../third_party/json/include)

# Scan source code files
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/api API_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/control CONTROL_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/core CORE_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/data_logic DATA_LOGIC_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/device DEVICE_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/device/gpu GPU_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/event EVENT_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/infrastructure INFRAS_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/infrastructure/exception
                     EXCEPTION_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/monitor MONITOR_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/policy POLICY_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/group GROUP_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/health HEALTH_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/diagnostic DIAGNOSTIC_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/topology TOPOLOGY_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/dump_raw_data
                     DUMP_RAW_DATA_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/firmware FIRMWARE_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/amc AMC_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/redfish REDFISH_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/log LOG_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/vgpu VGPU_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/ipmi IPMI_SRC)

add_library(xpum SHARED)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/test)
  add_executable(test_xpum_api ${CMAKE_CURRENT_LIST_DIR}/test/test_xpum_api.cpp)
endif()


target_include_directories(
  xpum
  PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
  PRIVATE /usr/local/include/level_zero/
          /usr/include/level_zero/
          ${CMAKE_INSTALL_PREFIX}/include
          ${CMAKE_CURRENT_LIST_DIR}/../build/hwloc/include/
          ${CMAKE_CURRENT_LIST_DIR}/../third_party/spdlog/include
          ${CMAKE_CURRENT_LIST_DIR}/../third_party/pcm/pcm-iio-gpu/include
          ${CMAKE_CURRENT_LIST_DIR}/src
          ${CMAKE_CURRENT_LIST_DIR}/src/infrastructure)

target_compile_definitions(xpum PRIVATE SPDLOG_COMPILED_LIB)
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/test)
  target_include_directories(
    test_xpum_api
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE /usr/local/include/level_zero/
            /usr/include/level_zero/
            ${CMAKE_INSTALL_PREFIX}/include
            ${CMAKE_CURRENT_LIST_DIR}/../build/hwloc/include/
            ${CMAKE_CURRENT_LIST_DIR}/../third_party/spdlog/include
            ${CMAKE_CURRENT_LIST_DIR}/../third_party/pcm/pcm-iio-gpu/include
            ${CMAKE_CURRENT_LIST_DIR}/src
            ${CMAKE_CURRENT_LIST_DIR}/src/infrastructure)
  target_compile_definitions(test_xpum_api PRIVATE SPDLOG_COMPILED_LIB)
endif()

target_sources(
  xpum
  PRIVATE ${API_SRC}
          ${CONTROL_SRC}
          ${CORE_SRC}
          ${DATA_LOGIC_SRC}
          ${DEVICE_SRC}
          ${GPU_SRC}
          ${EVENT_SRC}
          ${INFRAS_SRC}
          ${EXCEPTION_SRC}
          ${MONITOR_SRC}
          ${POLICY_SRC}
          ${GROUP_SRC}
          ${HEALTH_SRC}
          ${DIAGNOSTIC_SRC}
          ${TOPOLOGY_SRC}
          ${DUMP_RAW_DATA_SRC}
          ${FIRMWARE_SRC}
          ${AMC_SRC}
          ${REDFISH_SRC}
          ${LOG_SRC}
          ${VGPU_SRC}
          ${IPMI_SRC})

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/test)
  target_sources(
    test_xpum_api
    PRIVATE ${API_SRC}
            ${CONTROL_SRC}
            ${CORE_SRC}
            ${DATA_LOGIC_SRC}
            ${DEVICE_SRC}
            ${GPU_SRC}
            ${EVENT_SRC}
            ${INFRAS_SRC}
            ${EXCEPTION_SRC}
            ${MONITOR_SRC}
            ${POLICY_SRC}
            ${GROUP_SRC}
            ${HEALTH_SRC}
            ${DIAGNOSTIC_SRC}
            ${TOPOLOGY_SRC}
            ${DUMP_RAW_DATA_SRC}
            ${FIRMWARE_SRC}
            ${AMC_SRC}
            ${REDFISH_SRC}
            ${LOG_SRC}
            ${VGPU_SRC}
            ${IPMI_SRC})
endif()

message(STATUS "version ${PROJECT_VERSION}")
message(STATUS "soversion: ${PROJECT_VERSION_MAJOR}")

set_target_properties(xpum PROPERTIES VERSION ${PROJECT_REAL_VERSION}
                                      SOVERSION ${PROJECT_VERSION_MAJOR})

set(LibSpd "-Wl,--push-state,--whole-archive" "spdlog$<$<CONFIG:Debug>:d>" "-Wl,--pop-state")

if(HAVE_PCIACCESS_H)
  target_link_libraries(
    xpum
    PRIVATE ze_loader
            dl
            ${LibSpd}
            hwloc
            stdc++fs
            pcm-iio-gpu
            pciaccess
            metee
            igsc)
  if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/test)
    target_link_libraries(
      test_xpum_api
      PRIVATE ze_loader
              dl
              ${LibSpd}
              hwloc
              stdc++fs
              pcm-iio-gpu
              pciaccess
              metee
              igsc)
  endif()
else()
  target_link_libraries(xpum PRIVATE ze_loader dl ${LibSpd} hwloc pcm-iio-gpu
                                     stdc++fs metee igsc)
  if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/test)
    target_link_libraries(test_xpum_api PRIVATE ze_loader dl ${LibSpd} hwloc
                                                pcm-iio-gpu stdc++fs metee igsc)
  endif()
endif()

target_link_directories(xpum PRIVATE ${CMAKE_INSTALL_PREFIX}/lib64 ${CMAKE_INSTALL_PREFIX}/lib)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/test)
  target_link_directories(test_xpum_api PRIVATE ${CMAKE_INSTALL_PREFIX}/lib64 ${CMAKE_INSTALL_PREFIX}/lib)
endif()

unset(BUILD_TEST CACHE)

if(NOT DAEMONLESS)
  install(
    DIRECTORY resources
    DESTINATION lib/xpum
    PATTERN "config" EXCLUDE)
  install(DIRECTORY resources/config DESTINATION lib/xpum)
else()
  install(
    DIRECTORY resources
    DESTINATION lib/xpu-smi
    PATTERN "config" EXCLUDE)
  install(DIRECTORY resources/config DESTINATION lib/xpu-smi)
endif()
