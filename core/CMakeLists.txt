cmake_minimum_required(VERSION 3.2.0)

project(xpum)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set compilation options
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++14" COMPILER_SUPPORTS_CXX14)
if(COMPILER_SUPPORTS_CXX14)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
else()
  message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support.  Please use a different C++ compiler.")
endif()

set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -Wall -pthread -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")

include(../.cmake/xpum_version.cmake)

# Build test or not
option(BUILD_TEST "build application instead of lib for test" OFF)

option(BUILD_TEST_API "build application for api test" OFF)

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

add_executable(test_xpum_api ${CMAKE_CURRENT_LIST_DIR}/test/test_xpum_api.cpp)
add_library(xpum SHARED)

target_include_directories(
  xpum
  PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
  PRIVATE /usr/local/include/level_zero/ /usr/include/level_zero/
          ${CMAKE_CURRENT_LIST_DIR}/../build/hwloc/include/
          ${CMAKE_CURRENT_LIST_DIR}/../third_party/spdlog/include
          ${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_LIST_DIR}/src/infrastructure )

target_include_directories(
  test_xpum_api
  PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
  PRIVATE /usr/local/include/level_zero/ /usr/include/level_zero/
          ${CMAKE_CURRENT_LIST_DIR}/../build/hwloc/include/ 
          ${CMAKE_CURRENT_LIST_DIR}/../third_party/spdlog/include
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
          ${TOPOLOGY_SRC})

message(STATUS "version ${PROJECT_VERSION}") 
message(STATUS "soversion: ${PROJECT_VERSION_MAJOR}")

set_target_properties(xpum PROPERTIES 
    VERSION ${PROJECT_REAL_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})


if(HAVE_PCIACCESS_H)
    target_link_libraries(xpum PRIVATE ze_loader dl spdlog hwloc stdc++fs pciaccess)
    target_link_libraries(test_xpum_api PRIVATE ze_loader dl spdlog hwloc stdc++fs pciaccess)
else()
    target_link_libraries(xpum PRIVATE ze_loader dl spdlog hwloc stdc++fs)
    target_link_libraries(test_xpum_api PRIVATE ze_loader dl spdlog hwloc stdc++fs)
endif()

unset(BUILD_TEST CACHE)

install(DIRECTORY	
		resources
	DESTINATION .
  PATTERN "config" EXCLUDE
)


install(DIRECTORY	
		resources/config
	DESTINATION .
)