cmake_minimum_required(VERSION 3.14.0)
cmake_policy(SET CMP0077 OLD)

project(XPUMCLI)

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

if(NOT DEFINED GIT_COMMIT)
  set(GIT_COMMIT abcdefg0)
endif()

configure_file(${CMAKE_CURRENT_LIST_DIR}/src/config.h.in
               ${CMAKE_CURRENT_LIST_DIR}/src/config.h @ONLY)

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
  OUTPUT "${core_proto_srcs}" "${core_proto_hdrs}" "${core_grpc_srcs}"
         "${core_grpc_hdrs}"
  COMMAND
    ${_PROTOBUF_PROTOC} ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}" --cpp_out
    "${CMAKE_CURRENT_BINARY_DIR}" -I "${core_proto_path}"
    --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}" "${core_proto}"
  DEPENDS "${core_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# Specifiy header file location
include_directories(/usr/local/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../core/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../core/src/api)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../third_party/CLI11/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../third_party/json/include)
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/test)
  include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/../third_party/googletest/googletest/include)
endif()
include_directories(${CMAKE_CURRENT_BINARY_DIR})

link_directories(/usr/local/lib)
link_directories(${CMAKE_CURRENT_LIST_DIR}/../build/lib)

# Scan source code files
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src XPUM_CLI_SRC)
set(GRPC_SRC ${core_grpc_srcs} ${core_grpc_hdrs} ${core_proto_srcs}
             ${core_proto_hdrs})

add_executable(xpumcli ${CMAKE_CURRENT_LIST_DIR}/src/xpumcli.cpp)

target_sources(
  xpumcli
  PRIVATE ${XPUM_CLI_SRC} ${GRPC_SRC}
          ${CMAKE_CURRENT_LIST_DIR}/../core/src/api/internal_dump_raw_data.cpp)

target_link_libraries(xpumcli ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF} igsc)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/test)
  enable_testing()
  if(NOT BUILD_TESTING)
    set(BUILD_TESTING
        "Off"
        CACHE STRING "Choose On or off" FORCE)
  endif()

  if(BUILD_TESTING)

    set(Libraries gtest$<$<CONFIG:Debug>:d> gmock$<$<CONFIG:Debug>:d>
                  gtest_main$<$<CONFIG:Debug>:d> gmock_main$<$<CONFIG:Debug>:d>)

    aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src XPUM_CLI_TEST_SRC)
    list(REMOVE_ITEM XPUM_CLI_TEST_SRC
         ${CMAKE_CURRENT_LIST_DIR}/src/xpumcli.cpp)

    file(GLOB test_sources test/*_test.cpp)

    foreach(testsourcefile ${test_sources})

      get_filename_component(testname ${testsourcefile} NAME_WE)

      add_executable(${testname} ${testsourcefile})

      target_sources(${testname} PUBLIC ${XPUM_CLI_TEST_SRC} ${GRPC_SRC})

      target_link_libraries(${testname} ${_GRPC_GRPCPP}
                            ${_PROTOBUF_LIBPROTOBUF} ${Libraries})

      include(GoogleTest)
      gtest_discover_tests(${testname})

    endforeach(testsourcefile ${test_sources})
  endif()
endif()
