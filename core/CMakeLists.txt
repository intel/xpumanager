cmake_minimum_required (VERSION 3.2.0)

project (DGMCore)

# Set compilation options
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++11 -Wall -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build test or not
option(BUILD_TEST "build application instead of lib for test" OFF)

option(BUILD_TEST_API "build application for api test" OFF)

# set(BUILD_TEST_API OFF BOOL "build application for api test")

# Set debug file name postfix
#set(CMAKE_DEBUG_POSTFIX "_d")

# Do something according to build type
#if(${CMAKE_BUILD_TYPE} == "Debug)
#else()
#endif()

# Specifiy header file location
link_directories(/usr/local/lib)
include_directories(/usr/local/include/level_zero/)
include_directories(${CMAKE_CURRENT_LIST_DIR}/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/api)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/control)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/core)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/data_logic)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/device)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/device/gpu)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/event)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/infrastructure)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/infrastructure/exception)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/monitor)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/policy)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/group)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/health)


# Scan source code files
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/api API_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/control CONTROL_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/core CORE_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/data_logic DATA_LOGIC_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/device DEVICE_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/device/gpu GPU_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/event EVENT_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/infrastructure INFRAS_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/infrastructure/exception EXCEPTION_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/monitor MONITOR_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/policy POLICY_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/group GROUP_SRC)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src/health HEALTH_SRC)


if (BUILD_TEST)

  # Include source code and header files for test 

  # include_directories(${CMAKE_CURRENT_LIST_DIR}/test)

  # aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/test TEST_SRC)

  # add_executable(DGMCore ${TEST_SRC})

  if (BUILD_TEST_API)
    # add_executable(DGMCore ${CMAKE_CURRENT_LIST_DIR}/test/test_api.cpp)
    add_executable(DGMCore ${CMAKE_CURRENT_LIST_DIR}/test/test_xpum_api.cpp)
  else()
    add_executable(DGMCore ${CMAKE_CURRENT_LIST_DIR}/test/test_main.cpp)
  endif()

else()

  add_library (DGMCore SHARED)

endif()

target_sources(
  DGMCore
  PUBLIC
  ${API_SRC}
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
)

target_link_libraries(DGMCore ze_loader)

unset(BUILD_TEST CACHE) 
