cmake_minimum_required (VERSION 3.10.0)

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

# Specifiy header file location
link_directories(/usr/local/lib)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/CLI11/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/json/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../daemon)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../core/include)

# Scan source code files
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src XPUM_CLI_SRC)
set(GRPC_SRC 
  ${CMAKE_CURRENT_LIST_DIR}/../daemon/core.grpc.pb.cc
  ${CMAKE_CURRENT_LIST_DIR}/../daemon/core.pb.cc
)

add_executable(xpumcli ${CMAKE_CURRENT_LIST_DIR}/src/xpumcli.cpp)

target_sources(
  xpumcli
  PUBLIC
  ${XPUM_CLI_SRC}
  ${GRPC_SRC}
)

target_link_libraries(xpumcli protobuf)
target_link_libraries(xpumcli grpc++)

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
    protobuf
    grpc++
    gtest_main
    gmock_main)

  include(GoogleTest)
  gtest_discover_tests(${testname})

endforeach( testsourcefile ${testing_sources} )
