cmake_minimum_required (VERSION 3.2.0)

project (XPUMCLI)

# Set compilation options
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++11 -Wall -pthread -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Specifiy header file location
link_directories(/usr/local/lib)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/CLI11/include)

# Scan source code files
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src XPUM_SRC)

add_executable(xpumcli ${CMAKE_CURRENT_LIST_DIR}/src/xpumcli.cpp)

target_sources(
  xpumcli
  PUBLIC
  ${XPUM_SRC}

)
