cmake_minimum_required(VERSION 3.2.0)

project(xpum)

# Set compilation options
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++11 -Wall -pthread -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(../.cmake/xpum_version.cmake)

if(NOT DEFINED XPUM_VERSION_STRING)
  set(XPUM_VERSION_STRING 0.1.0)
endif()

include(CheckIncludeFile)
check_include_file(pciaccess.h HAVE_PCIACCESS_H)

configure_file(${CMAKE_CURRENT_LIST_DIR}/src/infrastructure/xpum_config.h.in
               ${CMAKE_CURRENT_LIST_DIR}/src/infrastructure/xpum_config.h @ONLY)

# set(BUILD_TEST_API OFF BOOL "build application for api test")

# Set debug file name postfix set(CMAKE_DEBUG_POSTFIX "_d")

# Do something according to build type if(${CMAKE_BUILD_TYPE} == "Debug) else()
# endif()

# Specifiy link file location
link_directories(/usr/local/lib)
link_directories(${CMAKE_CURRENT_LIST_DIR}/libs/hwloc/install/lib)

# Specifiy header file location

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

add_library(xpum SHARED)

target_include_directories(
  xpum
  PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
  PRIVATE /usr/local/include/level_zero/ /usr/include/level_zero/
          ${CMAKE_CURRENT_LIST_DIR}/libs/hwloc/install/include/
          ${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_LIST_DIR}/src/infrastructure )
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
          ${TOPOLOGY_SRC})

add_subdirectory(./libs/spdlog)
add_subdirectory(./libs/hwloc)

#set_target_properties（DGMCore PROPERTIES SUFFIX ".so.${PROJECT_VERSION_MAJOR}")
message(STATUS "version ${PROJECT_VERSION}") 
message(STATUS "soversion: ${PROJECT_VERSION_MAJOR}")

set_target_properties(xpum PROPERTIES 
    VERSION ${PROJECT_REAL_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

add_dependencies(xpum lib_hwloc)

if(HAVE_PCIACCESS_H)
  target_link_libraries(xpum PRIVATE ze_loader dl spdlog hwloc stdc++fs
                                        pciaccess)
else()
  target_link_libraries(xpum PRIVATE ze_loader dl spdlog hwloc stdc++fs)
endif()

unset(BUILD_TEST CACHE)

