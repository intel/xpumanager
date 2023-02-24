cmake_minimum_required(VERSION 3.18.0)

if(NOT DEFINED WORK_PATH)
  set(WORK_PATH ${CMAKE_CURRENT_BINARY_DIR})
endif()

include(.cmake/xpum_version.cmake)
include(.cmake/os_release_info.cmake)

if(DAEMONLESS)
project(xpu-smi VERSION ${PROJECT_VERSION})
else()
project(xpumanager VERSION ${PROJECT_VERSION})
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_os_release_info(os_name os_version os_code)
message(STATUS "OS_name:=${os_name} OS_vsr=${os_version} OS_code=${os_code}")

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

set(CMAKE_CXX_FLAGS
    "$ENV{CXXFLAGS} -Wall -pthread -fPIC -fno-strict-overflow -fno-delete-null-pointer-checks -fwrapv"
)
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")

set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -z noexecstack -z relro -z now")
set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -z noexecstack -z relro -z now")

set(CPACK_SET_DESTDIR OFF)
set(CPACK_RPM_PACKAGE_RELOCATABLE ON)

include(GNUInstallDirs)

set(CPACK_XPUM_LIB_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}")
set(CPACK_XPUM_BIN_INSTALL_DIR "bin")
set(CPACK_XPUM_INCLUDE_INSTALL_DIR "include")
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
if(NOT DAEMONLESS)
  set(SYSTEMD_UNIT_NAME "${CMAKE_CURRENT_LIST_DIR}/daemon/xpum.service.template")
endif()

if(NOT DAEMONLESS)
  set(XPUM_CONFIG_DIR "${CPACK_PACKAGING_INSTALL_PREFIX}/lib/xpum/config")
  set(XPUM_RESOURCES_DIR "${CPACK_PACKAGING_INSTALL_PREFIX}/lib/xpum/resources")
else()
  set(XPUM_CONFIG_DIR "${CPACK_PACKAGING_INSTALL_PREFIX}/lib/xpu-smi/config")
  set(XPUM_RESOURCES_DIR "${CPACK_PACKAGING_INSTALL_PREFIX}/lib/xpu-smi/resources")
endif()

set(CMAKE_PROJECT_VERSION "${PROJECT_VERSION}")
add_definitions(-DLOADER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR})
add_definitions(-DLOADER_VERSION_MINOR=${PROJECT_VERSION_MINOR})
add_definitions(-DLOADER_VERSION_PATCH=${PROJECT_VERSION_PATCH})

message(STATUS "CMAKE_PROJECT_VERSION: ${CMAKE_PROJECT_VERSION}")

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/third_party/googletest)
  add_subdirectory(third_party/googletest)
endif()
add_subdirectory(third_party/spdlog)
add_subdirectory(third_party/hwloc)
add_subdirectory(third_party/pcm)

add_subdirectory(core)
add_subdirectory(cli)
if(BUILD_AMCMCLI)
  add_subdirectory(amcmcli)
endif()
if(NOT DAEMONLESS)
  add_subdirectory(daemon)
  add_subdirectory(rest)
endif()
if(BUILD_DOC)
  add_subdirectory(doc)
endif()

add_dependencies(xpum lib_hwloc)
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/core/test)
  add_dependencies(test_xpum_api lib_hwloc)
endif()

if(NOT DEFINED CPACK_GENERATOR)
  if(EXISTS "/etc/debian_version")
    set(CPACK_GENERATOR "DEB")
  elseif(EXISTS "/etc/redhat-release")
    set(CPACK_GENERATOR "RPM")
  elseif(EXISTS "/etc/SUSE-brand" OR EXISTS "/etc/SUSE-release")
    set(CPACK_GENERATOR "RPM")
  else()
    set(CPACK_GENERATOR "ZIP")
  endif()
endif()

message("CPACK_PACKAGE_NAME=${PROJECT_NAME}")
set(CPACK_PACKAGE_CONTACT "Intel Corporation")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "Intel")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

if(CPACK_GENERATOR MATCHES "RPM")
  if(NOT DAEMONLESS)  
    configure_file("${SYSTEMD_UNIT_NAME}.in" "${SYSTEMD_UNIT_NAME}" COPYONLY)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/rest/xpum_rest.service.template.in"
                  "${CMAKE_CURRENT_LIST_DIR}/rest/xpum_rest.service.template" COPYONLY)
  endif()
  set(CPACK_RPM_SPEC_MORE_DEFINE
      "%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')"
  )
  set(CPACK_RPM_COMPRESSION_TYPE "xz")
  set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
  set(CPACK_RPM_PACKAGE_AUTOREQ OFF)
  set(CPACK_RPM_PACKAGE_DESCRIPTION "${PROJECT_NAME}")
  if(EXISTS "/etc/redhat-release")
    if(${os_version} VERSION_GREATER_EQUAL 8)
      set(CPACK_RPM_PACKAGE_REQUIRES "level-zero >= 1.7.9.1, intel-gsc >= 0.8.4, intel-level-zero-gpu >= 1.3.23726")
      set(CPACK_RPM_SPEC_MORE_DEFINE "Recommends: intel-metrics-discovery >= 1.10.141, intel-metrics-library >= 1.0.76, intel-mediasdk-utils >= 22.5.0, intel-media >= 22.5.0, libmfxgen1 >= 22.5.0")
    else()
      set(CPACK_RPM_PACKAGE_REQUIRES "level-zero >= 1.7.9.1, intel-gsc >= 0.8.4, intel-level-zero-gpu >= 1.3.23726, intel-metrics-discovery >= 1.10.141, intel-metrics-library >= 1.0.76, intel-mediasdk-utils >= 22.5.0, intel-media >= 22.5.0, libmfxgen1 >= 22.5.0")
    endif()
  else()
    set(CPACK_RPM_PACKAGE_REQUIRES "level-zero >= 1.7.9.1, intel-gsc >= 0.8.4, intel-level-zero-gpu >= 1.3.23726")
    set(CPACK_RPM_SPEC_MORE_DEFINE "Recommends: intel-metrics-discovery >= 1.10.141, intel-metrics-library >= 1.0.76, libmfx-utils >= 22.5.0, intel-media-driver >= 22.5.0, libmfxgen1 >= 22.5.0")
  endif()
  set(CPACK_RPM_PACKAGE_GROUP "System Environment/Libraries")
  set(CPACK_RPM_PACKAGE_LICENSE "MIT")
  set(CPACK_RPM_PACKAGE_RELEASE "${BUILD_DATE}.${BUILD_TIME}.${GIT_COMMIT}")
  set(CPACK_RPM_PACKAGE_RELEASE_DIST OFF)
  set(CPACK_RPM_COMPONENT_INSTALL ON)
  set(CPACK_RPM_PACKAGE_NAME "${PROJECT_NAME}")
  if(EXISTS "/etc/redhat-release")
    string(REPLACE "." ";" VERSION_LIST ${os_version})
    list(GET VERSION_LIST 0 os_version_major)
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_REAL_VERSION}-${BUILD_DATE}.${BUILD_TIME}.${GIT_COMMIT}.el${os_version_major}.${CPACK_RPM_PACKAGE_ARCHITECTURE}")
  else()
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_REAL_VERSION}-${BUILD_DATE}.${BUILD_TIME}.${GIT_COMMIT}.${CPACK_RPM_PACKAGE_ARCHITECTURE}")
  endif()
  file(WRITE "${CMAKE_BINARY_DIR}/package_file_name" "${CPACK_PACKAGE_FILE_NAME}.rpm")
  list(
    APPEND
    CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    "/etc/ld.so.conf.d"
    "/usr/local"
    "/usr/local/lib64"
    "/usr/local/bin"
    "/usr/local/include"
    "/lib"
    "/usr/lib"
    "/usr/local/lib")

  if(NOT DAEMONLESS)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/install/script/rpm/postin.in" 
                  "${CMAKE_CURRENT_LIST_DIR}/install/script/rpm/postin" @ONLY)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/install/script/rpm/posttrans.in" 
                  "${CMAKE_CURRENT_LIST_DIR}/install/script/rpm/posttrans" @ONLY)
    set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_LIST_DIR}/install/script/rpm/postin")
    set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE
        "${CMAKE_CURRENT_LIST_DIR}/install/script/rpm/preun")
    set(CPACK_RPM_POST_TRANS_SCRIPT_FILE
        "${CMAKE_CURRENT_LIST_DIR}/install/script/rpm/posttrans")
  endif()
endif()

if(CPACK_GENERATOR MATCHES "DEB")
  if(NOT DAEMONLESS)
    configure_file("${SYSTEMD_UNIT_NAME}.in" "${SYSTEMD_UNIT_NAME}" @ONLY)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/rest/xpum_rest.service.template.in"
                 "${CMAKE_CURRENT_LIST_DIR}/rest/xpum_rest.service.template" @ONLY)
  endif()

  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
  set(CPACK_DEBIAN_PACKAGE_RELEASE "${BUILD_DATE}.${BUILD_TIME}.${GIT_COMMIT}")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
  set(CPACK_DEBIAN_XPUM_PACKAGE_NAME "${PROJECT_NAME}.${os_name}")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "level-zero (>= 1.7.9.1), intel-gsc (>=0.8.4), intel-level-zero-gpu (>=1.3.23726)")
  set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "intel-metrics-discovery (>=1.10.141), intel-metrics-library (>=1.0.76), libmfx-tools (>=22.5.0), intel-media-va-driver-non-free (>=22.5.0), libmfxgen1 (>=22.5.0)")
  set(CPACK_DEB_COMPONENT_INSTALL ON)
  set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
  if(NOT DAEMONLESS)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/install/script/deb/postinst.in" 
                  "${CMAKE_CURRENT_LIST_DIR}/install/script/deb/postinst" @ONLY)
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_LIST_DIR}/install/script/deb/postinst;${CMAKE_CURRENT_LIST_DIR}/install/script/deb/prerm;"
    )
  endif()

  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}_${PROJECT_REAL_VERSION}_${BUILD_DATE}.${BUILD_TIME}.${GIT_COMMIT}~u${os_version}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
  file(WRITE "${CMAKE_BINARY_DIR}/package_file_name" "${CPACK_PACKAGE_FILE_NAME}.deb")
endif()

install(DIRECTORY core/include/ DESTINATION ${CPACK_XPUM_INCLUDE_INSTALL_DIR})

if(NOT DAEMONLESS)
  install(DIRECTORY install/tools/rest/ DESTINATION lib/xpum)
endif()

if(DAEMONLESS)
  install(
    TARGETS xpum xpumanager
    LIBRARY DESTINATION ${CPACK_XPUM_LIB_INSTALL_DIR}
    RUNTIME DESTINATION ${CPACK_XPUM_BIN_INSTALL_DIR}
    ARCHIVE DESTINATION .)
else()
  install(
    TARGETS xpum xpumcli xpumd
    LIBRARY DESTINATION ${CPACK_XPUM_LIB_INSTALL_DIR}
    RUNTIME DESTINATION ${CPACK_XPUM_BIN_INSTALL_DIR}
    ARCHIVE DESTINATION .)
endif()

if(NOT DAEMONLESS)
install(FILES "${SYSTEMD_UNIT_NAME}"
              "${CMAKE_CURRENT_LIST_DIR}/rest/xpum_rest.service.template"
        DESTINATION lib/xpum)
endif()
include(CPack)
