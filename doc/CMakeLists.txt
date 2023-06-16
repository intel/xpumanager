cmake_minimum_required(VERSION 3.14.0)
project(xpum_doc)

include(../.cmake/xpum_version.cmake)

find_package(Python3 COMPONENTS Interpreter)

if(NOT Python3_Interpreter_FOUND)
  message(FATAL_ERROR "Python3 not found, please install first")
  endif()
  
file(STRINGS /etc/os-release distro REGEX "^NAME=")
string(REGEX REPLACE "NAME=\"(.*)\"" "\\1" distro "${distro}")

add_subdirectory(smidoc)
add_subdirectory(xpumdoc)
add_subdirectory(windoc)

file(COPY ${CMAKE_CURRENT_LIST_DIR}/img DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/doc)

file(COPY index.rst 
      xpum_index.rst win_index.rst restful.rst ../README.md installation.rst CLI_user_guide.md Install_guide.md 
      smi_index.rst smi_install_guide.md smi_user_guide.md ../SMI_README.md xpum_win_user_guide.md
      DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

configure_file(${CMAKE_CURRENT_LIST_DIR}/../rest/README.md ${CMAKE_CURRENT_BINARY_DIR}/REST_README.md COPYONLY)

configure_file(${CMAKE_CURRENT_LIST_DIR}/../windows/README.md ${CMAKE_CURRENT_BINARY_DIR}/WIN_README.md COPYONLY)

add_custom_target(
  XPUM_DOC ALL
  COMMAND /bin/sh -c ${CMAKE_CURRENT_LIST_DIR}/../rest/gen_grpc_py_files.sh
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/../rest/gen_doc.py > ${CMAKE_CURRENT_BINARY_DIR}/schema.yml
  COMMAND sphinx-build -c ${CMAKE_CURRENT_LIST_DIR}/sphinxconf/ ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/html
  DEPENDS SMI_LIB_CORE_RST XPUM_LIB_CORE_RST WIN_DLL_RST)

if(NOT DAEMONLESS)
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/ DESTINATION lib/xpum/doc # PATTERN ".buildinfo" EXCLUDE
  )
else()
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/ DESTINATION lib/xpu-smi/doc # PATTERN ".buildinfo" EXCLUDE
  )
endif()

