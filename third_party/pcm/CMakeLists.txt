cmake_minimum_required(VERSION 3.2.0)

project(pcm)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set compilation options
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++14" COMPILER_SUPPORTS_CXX14)
if(COMPILER_SUPPORTS_CXX14)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
else()
  message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support.  Please use a different C++ compiler.")
endif()

set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -Wall -pthread -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")

set(SRCS pcm-iio-gpu.cpp bw.cpp cpucounters.cpp debug.cpp mmio.cpp msr.cpp pci.cpp resctrl.cpp threadpool.cpp topology.cpp utils.cpp)

add_library(pcm-iio-gpu STATIC
    ${SRCS}
)

target_include_directories(pcm-iio-gpu
    PUBLIC 
      ${PROJECT_SOURCE_DIR}
)