cmake_minimum_required(VERSION 3.14.0)
cmake_policy(SET CMP0077 OLD)

project(XPUMDAEMON)

include(../.cmake/grpc_common.cmake)
include(../.cmake/xpum_version.cmake)

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

set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -Wall -pthread -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -s")

if(NOT DEFINED XPUM_VERSION_STRING)
  set(XPUM_VERSION_STRING 0.1.0)
endif()

# list(APPEND CMAKE_INSTALL_RPATH "${CMAKE_CURRENT_BINARY_DIR}"
# "${CMAKE_CURRENT_BINARY_DIR}/../core" )

configure_file(${CMAKE_CURRENT_LIST_DIR}/config.h.in
               ${CMAKE_CURRENT_LIST_DIR}/config.h @ONLY)

link_directories(${CMAKE_CURRENT_LIST_DIR}/../build/third_party/spdlog)

# Proto file
get_filename_component(core_proto "${CMAKE_CURRENT_LIST_DIR}/core.proto"
                       ABSOLUTE)
get_filename_component(core_proto_path "${core_proto}" PATH)

message("core_proto: ${core_proto}")
message("core_proto_path: ${core_proto_path}")

# Generated sources
set(core_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/core.pb.cc")
set(core_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/core.pb.h")
set(core_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/core.grpc.pb.cc")
set(core_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/core.grpc.pb.h")
add_custom_command(
  OUTPUT "${core_proto_srcs}" "${core_proto_hdrs}" "${core_grpc_srcs}"
         "${core_grpc_hdrs}"
  COMMAND
    ${_PROTOBUF_PROTOC} ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}" --cpp_out
    "${CMAKE_CURRENT_BINARY_DIR}" -I "${core_proto_path}"
    --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}" "${core_proto}"
  DEPENDS "${core_proto}")

# Scan source code files
aux_source_directory(${CMAKE_CURRENT_LIST_DIR} XPUM_DAEMON_SRC)
set(GRPC_SRC ${core_grpc_srcs} ${core_grpc_hdrs} ${core_proto_srcs}
             ${core_proto_hdrs})

add_executable(xpumd ${CMAKE_CURRENT_LIST_DIR}/daemon.cpp)

target_include_directories(
  xpumd
  PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../core/include
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../core/src/api
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../core/src/amc
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
          ${CMAKE_CURRENT_LIST_DIR}/../third_party/spdlog/include)

set(LibSpd spdlog$<$<CONFIG:Debug>:d>)

target_sources(xpumd PUBLIC ${XPUM_DAEMON_SRC} ${GRPC_SRC})

set(CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR}/../)
if(NOT DEFINED WORK_PATH)
  message(STATUS "WORK_PATH not defined!")
  message("PATH = $ENV{PATH}")
  find_library(
    DGM_LIB
    NAMES libxpum.so
    PATHS ${CMAKE_CURRENT_LIST_DIR}/../core/build
          ${CMAKE_CURRENT_LIST_DIR}/../build/core
    NO_CMAKE_ENVIRONMENT_PATH NO_DEFAULT_PATH NO_SYSTEM_ENVIRONMENT_PATH)

  message(STATUS "DGM_LIB=${DGM_LIB}")
  target_link_libraries(xpumd PRIVATE ${LibSpd} ${DGM_LIB} ${_GRPC_GRPCPP}
                                      ${_PROTOBUF_LIBPROTOBUF})

  add_custom_command(
    TARGET xpumd
    PRE_BUILD
    COMMAND
      /bin/sh -c
      "if [ ! -d ${CMAKE_CURRENT_BINARY_DIR}/../config ] \; then cp -r ${CMAKE_CURRENT_LIST_DIR}/../core/resources/config ${CMAKE_CURRENT_BINARY_DIR}/../\; fi\;"
    COMMAND
      /bin/sh -c
      "if [ ! -d ${CMAKE_CURRENT_BINARY_DIR}/../resources ] \; then cp -r ${CMAKE_CURRENT_LIST_DIR}/../core/resources ${CMAKE_CURRENT_BINARY_DIR}/../\; fi\;"
  )
else()
  target_link_libraries(xpumd PRIVATE xpum ${LibSpd} ${_GRPC_GRPCPP}
                                      ${_PROTOBUF_LIBPROTOBUF})
endif()
