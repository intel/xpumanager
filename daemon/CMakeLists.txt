cmake_minimum_required(VERSION 3.10.0)
cmake_policy(SET CMP0077 OLD)

project(XPUMDAEMON)

include(../.cmake/grpc_common.cmake)
include(../.cmake/xpum_version.cmake)
# Set compilation options
set(CMAKE_CXX_FLAGS
    "$ENV{CXXFLAGS} -std=c++11 -Wall -pthread -lprotobuf -lgrpc++ -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED XPUM_VERSION_STRING)
  set(XPUM_VERSION_STRING 0.1.0)
endif()

list(APPEND CMAKE_INSTALL_RPATH 
	"${CMAKE_CURRENT_BINARY_DIR}"
	"${CMAKE_CURRENT_BINARY_DIR}/../core"
)

configure_file(${CMAKE_CURRENT_LIST_DIR}/config.h.in
               ${CMAKE_CURRENT_LIST_DIR}/config.h @ONLY)

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

# Include generated *.pb.h files
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Specifiy header file location
include_directories(${CMAKE_CURRENT_LIST_DIR}/../core/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Scan source code files
aux_source_directory(${CMAKE_CURRENT_LIST_DIR} XPUM_DAEMON_SRC)
set(GRPC_SRC ${core_grpc_srcs} ${core_grpc_hdrs} ${core_proto_srcs}
             ${core_proto_hdrs})

add_executable(xpumd ${CMAKE_CURRENT_LIST_DIR}/daemon.cc)

target_sources(xpumd PUBLIC ${XPUM_DAEMON_SRC} ${GRPC_SRC})


if (NOT DEFINED WORK_PATH)
  message(STATUS "WORK_PATH not defined!")
  find_library(DGM_LIB 
    NAMES libDGMCore.so
    PATHS 
    ${CMAKE_CURRENT_LIST_DIR}/../core/build
    ${CMAKE_CURRENT_LIST_DIR}/../build/core
    )
  
  message(STATUS "DGM_LIB=${DGM_LIB}")
  target_link_libraries(xpumd PRIVATE ${DGM_LIB} ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
  
  add_custom_command( TARGET xpumd 
      PRE_BUILD
      COMMAND /bin/sh -c "if [ ! -d ${CMAKE_CURRENT_BINARY_DIR}/../config ] \; then cp -r ${CMAKE_CURRENT_LIST_DIR}/../core/config ${CMAKE_CURRENT_BINARY_DIR}/../\; fi\;"
      COMMAND /bin/sh -c "if [ ! -d ${CMAKE_CURRENT_BINARY_DIR}/../kernels ] \; then cp -r ${CMAKE_CURRENT_LIST_DIR}/../core/kernels ${CMAKE_CURRENT_BINARY_DIR}/../\; fi\;"
      COMMAND /bin/sh -c "if [ ! -d ${CMAKE_CURRENT_BINARY_DIR}/../mediadata ] \; then cp -r ${CMAKE_CURRENT_LIST_DIR}/../core/mediadata ${CMAKE_CURRENT_BINARY_DIR}/../\; fi\;")
  
else()
  target_link_libraries(xpumd PRIVATE DGMCore ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
endif()

