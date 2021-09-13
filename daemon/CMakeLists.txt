cmake_minimum_required (VERSION 3.2.0)

project (XPUMDAEMON )

# Set compilation options
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++11 -Wall -pthread -lprotobuf -lgrpc++ -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT DEFINED XPUM_VERSION_STRING)
  set(XPUM_VERSION_STRING 0.1.0)
endif()

configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/config.h.in 
  ${CMAKE_CURRENT_LIST_DIR}/config.h 
  @ONLY)

# Specifiy header file location
include_directories(${CMAKE_CURRENT_LIST_DIR}/../core/include)
link_directories(${CMAKE_CURRENT_LIST_DIR}/../core)

# Scan source code files
aux_source_directory(${CMAKE_CURRENT_LIST_DIR} XPUM_DAEMON_SRC)

add_executable(xpumDaemon ${CMAKE_CURRENT_LIST_DIR}/daemon.cc)

target_link_libraries(xpumDaemon protobuf)
target_link_libraries(xpumDaemon grpc++)
target_link_libraries(xpumDaemon DGMCore)

target_sources(
  xpumDaemon
  PUBLIC
  ${XPUM_DAEMON_SRC}
)

