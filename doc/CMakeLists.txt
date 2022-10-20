cmake_minimum_required(VERSION 3.14.0)
project(xpum_doc)

include(../.cmake/xpum_version.cmake)

find_package(Python3 COMPONENTS Interpreter)
find_package(Doxygen)

if(NOT Python3_Interpreter_FOUND)
  message(FATAL_ERROR "Python3 not found, please install first")
  endif()
  
if(NOT DOXYGEN_FOUND)
  message(FATAL_ERROR "Doxygen not found, please install first")
endif()

set(XPUM_DOC_DOXYFILE ${CMAKE_CURRENT_LIST_DIR}/Doxyfile)

file(STRINGS /etc/os-release distro REGEX "^NAME=")
string(REGEX REPLACE "NAME=\"(.*)\"" "\\1" distro "${distro}")

set(DOXYREST_FOLDER ${CMAKE_CURRENT_LIST_DIR}/doxyrest)

if((${distro} MATCHES "CentOS") OR (${distro} MATCHES "openSUSE"))
  set(DOXYREST_PATH ${DOXYREST_FOLDER}/bin/doxyrest-rh)
else()
  set(DOXYREST_PATH ${DOXYREST_FOLDER}/bin/doxyrest)
endif()

set(DOXYGEN_EXTRACT_ALL YES)
if(NOT DAEMONLESS)
  set(DOXYGEN_ENABLED_SECTIONS DAEMON_ONLY)
endif()
set(DOXYGEN_GENERATE_HTML NO)
set(DOXYGEN_GENERATE_LATEX NO)
set(DOXYGEN_GENERATE_XML YES)
set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doxygen-output")

doxygen_add_docs(
  LIB_CORE_XML
  ${CMAKE_CURRENT_LIST_DIR}/../core/include
)

set(LIB_CORE_API_DOC_TITLE "Core Library API")

add_custom_target(
  LIB_CORE_RST
  COMMAND ${DOXYREST_PATH}
            -c ${DOXYREST_FOLDER}/share/doxyrest/frame/doxyrest-config.lua
            -F ${DOXYREST_FOLDER}/share/doxyrest/frame/cfamily 
            -F ${DOXYREST_FOLDER}/share/doxyrest/frame/common 
            -o ${CMAKE_CURRENT_BINARY_DIR}/rst/index.rst 
            -D SORT_GROUPS_BY="originalIdx"
            -D INDEX_TITLE=${LIB_CORE_API_DOC_TITLE}
            ${CMAKE_CURRENT_BINARY_DIR}/doxygen-output/xml/index.xml
  DEPENDS LIB_CORE_XML)

add_custom_target(xpum_doc ALL DEPENDS LIB_CORE_RST)

file(COPY ${CMAKE_CURRENT_LIST_DIR}/img DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/doc)

if(DAEMONLESS)

  configure_file(smi_index.rst ${CMAKE_CURRENT_BINARY_DIR}/index.rst COPYONLY)

  file(COPY smi_install_guide.md smi_user_guide.md ../SMI_README.md
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

  add_custom_command(
    TARGET xpum_doc
    PRE_BUILD
    COMMAND sphinx-build -c ${CMAKE_CURRENT_LIST_DIR}/sphinxconf/ ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/html
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/rst/index.rst)

else()

  file(COPY index.rst restful.rst ../README.md installation.rst CLI_user_guide.md ../REST_README.md Install_guide.md
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_command(
    TARGET xpum_doc
    PRE_BUILD
    COMMAND /bin/sh -c ${CMAKE_CURRENT_LIST_DIR}/../rest/gen_grpc_py_files.sh
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/../rest/gen_doc.py > ${CMAKE_CURRENT_BINARY_DIR}/schema.yml
    COMMAND sphinx-build -c ${CMAKE_CURRENT_LIST_DIR}/sphinxconf/ ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/html
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/rst/index.rst)

endif()

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/ DESTINATION doc # PATTERN ".buildinfo" EXCLUDE
)
