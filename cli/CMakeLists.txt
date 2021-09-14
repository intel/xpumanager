cmake_minimum_required (VERSION 3.10.0)
cmake_policy(SET CMP0077 OLD)

project (XPUMCLI)

# Set compilation options
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++11 -Wall -pthread -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT DEFINED XPUM_VERSION_STRING)
  set(XPUM_VERSION_STRING 0.1.0)
endif()

configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/src/config.h.in 
  ${CMAKE_CURRENT_LIST_DIR}/src/config.h 
  @ONLY)

# Find gRPC installation
# Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

set(_GRPC_GRPCPP gRPC::grpc++)

# Find Protobuf installation
# Looks for protobuf-config.cmake file installed by Protobuf's cmake installation.
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")

set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
find_program(_PROTOBUF_PROTOC protoc)

# GRPC and Protoc tools
find_program(_PROTOBUF_PROTOC protoc)
find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
  
# Proto file
get_filename_component(core_proto "../daemon/core.proto" ABSOLUTE)
get_filename_component(core_proto_path "${core_proto}" PATH)

message(${core_proto})
message(${core_proto_path})

# Generated sources
set(core_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/core.pb.cc")
set(core_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/core.pb.h")
set(core_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/core.grpc.pb.cc")
set(core_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/core.grpc.pb.h")
add_custom_command(
      OUTPUT "${core_proto_srcs}" "${core_proto_hdrs}" "${core_grpc_srcs}" "${core_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${core_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${core_proto}"
      DEPENDS "${core_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# Specifiy header file location
link_directories(/usr/local/lib)
include_directories(/usr/local/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/CLI11/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/json/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../core/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Scan source code files
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src XPUM_CLI_SRC)
set(GRPC_SRC 
  ${core_grpc_srcs}
  ${core_grpc_hdrs}
  ${core_proto_srcs}
  ${core_proto_hdrs})

add_executable(xpumcli ${CMAKE_CURRENT_LIST_DIR}/src/xpumcli.cpp)

target_sources(
  xpumcli
  PUBLIC
  ${XPUM_CLI_SRC}
  ${GRPC_SRC}
)

target_link_libraries(
  xpumcli
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

## Test using googletest
add_subdirectory(libs/googletest)

enable_testing()

aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src XPUM_CLI_TESTING_SRC)
list(REMOVE_ITEM XPUM_CLI_TESTING_SRC ${CMAKE_CURRENT_LIST_DIR}/src/xpumcli.cpp)

file( GLOB testing_sources testing/*_unittest.cpp )

foreach( testsourcefile ${testing_sources} )

  get_filename_component(testname ${testsourcefile} NAME_WE)

  add_executable( ${testname} ${testsourcefile} )

  target_sources(
    ${testname}
    PUBLIC
    ${XPUM_CLI_TESTING_SRC}
    ${GRPC_SRC})

  target_link_libraries(
    ${testname}
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    gtest_main
    gmock_main)

  include(GoogleTest)
  gtest_discover_tests(${testname})

endforeach( testsourcefile ${testing_sources} )
