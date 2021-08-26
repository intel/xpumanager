cmake_minimum_required (VERSION 3.2.0)

project (exploreProj)

# Set compilation options
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++11 -Wall -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build test or not

# Specifiy header file location
link_directories(/usr/local/lib)
include_directories(/usr/local/include/level_zero/)
# Scan source code files

#add_executable(test ${CMAKE_CURRENT_LIST_DIR}/test.cpp)
add_executable(try1 ${CMAKE_CURRENT_LIST_DIR}/try1.cpp)
target_link_libraries(try1 ze_loader hwloc)