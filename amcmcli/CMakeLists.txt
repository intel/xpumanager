cmake_minimum_required(VERSION 3.14.0)

project(AMCMCLI)

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

include_directories(${CMAKE_CURRENT_LIST_DIR}/../third_party/CLI11/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../core/src)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/../core/src/ipmi)
  message(STATUS "amcfw lib src found")
  aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/../core/src/ipmi IPMI_SRC)
  add_library(amcfw STATIC ${IPMI_SRC} ${INFRAS_SRC} ${EXCEPTION_SRC})

  target_include_directories(
    amcfw
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../core/include
            ${CMAKE_CURRENT_LIST_DIR}/../third_party/spdlog/include
            ${CMAKE_CURRENT_LIST_DIR}/../core/src
            ${CMAKE_CURRENT_LIST_DIR}/../core/src/infrastructure)

else()
  add_library(amcfw STATIC IMPORTED)
  set_target_properties(
    amcfw PROPERTIES IMPORTED_LOCATION
                     ${CMAKE_CURRENT_LIST_DIR}/../core/resources/amcfw/libamcfw.a)
endif()


add_executable(amcmcli ${CMAKE_CURRENT_LIST_DIR}/main.cpp)

target_link_libraries(amcmcli PRIVATE amcfw)