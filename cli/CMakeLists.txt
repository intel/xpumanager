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

# Scan source code files
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src XPUM_CLI_SRC)

add_executable(xpumcli ${CMAKE_CURRENT_LIST_DIR}/src/xpumcli.cpp)

target_sources(
  xpumcli
  PUBLIC
  ${XPUM_CLI_SRC}
)

## Test using googletest
add_subdirectory(libs/googletest)

enable_testing()

aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src XPUM_CLI_TESTING_SRC)
list(REMOVE_ITEM XPUM_CLI_TESTING_SRC ${CMAKE_CURRENT_LIST_DIR}/src/xpumcli.cpp)

file( GLOB testing_sources testing/*_unittest.cpp )

foreach( testsourcefile ${testing_sources} )

  get_filename_component(testname ${testsourcefile} NAME_WE)

  add_executable( ${testname} ${testsourcefile} )

  target_link_libraries( ${testname})

  target_sources(
    ${testname}
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/testing/testing_common_util.cpp
    ${XPUM_CLI_TESTING_SRC}
    )

  target_link_libraries(
    ${testname}
    gtest_main)

  include(GoogleTest)
  gtest_discover_tests(${testname})

endforeach( testsourcefile ${testing_sources} )
