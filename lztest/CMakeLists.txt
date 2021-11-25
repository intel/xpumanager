cmake_minimum_required(VERSION 3.2.0)

project(lztest)

# Set compilation options
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++14 -Wall -pthread -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O1")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Specifiy header file location

# Scan source code files
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/ LZTEST_SRC)

add_executable(lztest ${CMAKE_CURRENT_LIST_DIR}/lztest.cpp)

target_sources(lztest PRIVATE ${LZTEST_SRC})
target_link_libraries(lztest PRIVATE ze_loader)