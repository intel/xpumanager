cmake_minimum_required (VERSION 3.2.0)


if (NOT DEFINED WORK_PATH)	
	set(WORK_PATH ${CMAKE_CURRENT_BINARY_DIR})	
endif()

include(.cmake/xpum_version.cmake)

project (XPUManager VERSION ${PROJECT_VERSION})

set(CPACK_SET_DESTDIR OFF)
set(CPACK_RPM_PACKAGE_RELOCATABLE ON)

set(CPACK_XPUM_LIB_INSTALL_DIR "/usr/lib")
set(CPACK_XPUM_INCLUDE_INSTALL_DIR "/usr/include")
set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/xpum")
set(SYSTEMD_UNIT_NAME "${CMAKE_CURRENT_LIST_DIR}/daemon/xpum.service")

set(XPUM_CONFIG_DIR "${CPACK_PACKAGING_INSTALL_PREFIX}/config")

set(CMAKE_PROJECT_VERSION "${PROJECT_VERSION}")
add_definitions(-DLOADER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR})
add_definitions(-DLOADER_VERSION_MINOR=${PROJECT_VERSION_MINOR})
add_definitions(-DLOADER_VERSION_PATCH=${PROJECT_VERSION_PATCH})

message(STATUS "CMAKE_PROJECT_VERSION: ${CMAKE_PROJECT_VERSION}")

# Set compilation options
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++11 -Wall -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(core)
add_subdirectory(cli)
add_subdirectory(daemon)
add_subdirectory(rest)

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
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}.${PROJECT_VERSION}.${GIT_COMMIT}")
SET(CPACK_PACKAGE_DESCRIPTION "Inter XPU Manager")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${BUILD_TIMESTAMP}")

if(CPACK_GENERATOR MATCHES "RPM")
	configure_file("${SYSTEMD_UNIT_NAME}.in" "${SYSTEMD_UNIT_NAME}" COPYONLY)
	configure_file("${CMAKE_CURRENT_LIST_DIR}/rest/xpum-rest.service.in" "${CMAKE_CURRENT_LIST_DIR}/rest/xpum-rest.service" COPYONLY)

	file(WRITE "${CMAKE_BINARY_DIR}/package_file_name" "${CPACK_PACKAGE_FILE_NAME}.rpm")
	set(CPACK_RPM_SPEC_MORE_DEFINE "%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')")
	set(CPACK_RPM_PACKAGE_NAME "${PROJECT_NAME}.${PROJECT_VERSION}.${GIT_COMMIT}")
	set(CPACK_RPM_FILE_NAME "")
	set(CPACK_RPM_COMPRESSION_TYPE "xz")
	set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
	set(CPACK_RPM_PACKAGE_AUTOREQ OFF)
	set(CPACK_RPM_PACKAGE_DESCRIPTION "XPU Manager")
	set(CPACK_RPM_PACKAGE_GROUP "System Environment/Libraries")
	set(CPACK_RPM_PACKAGE_LICENSE "MIT")
	set(CPACK_RPM_PACKAGE_RELEASE 1)
	set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
	set(CPACK_RPM_COMPONENT_INSTALL ON)
	set(CPACK_RPM_PACKAGE_NAME "${PROJECT_NAME}")
	list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
	  "/etc/ld.so.conf.d"
	  "/usr/local"
	  "/usr/local/lib64"
	  "/usr/local/bin"
	  "/usr/local/include"
		"/lib"
		"/usr/lib"
		"/usr/local/lib"
	)

	set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_LIST_DIR}/install/postinst")
	set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_LIST_DIR}/install/prerm")
endif()

if(CPACK_GENERATOR MATCHES "DEB")	
	configure_file("${SYSTEMD_UNIT_NAME}.in" "${SYSTEMD_UNIT_NAME}" @ONLY)
	configure_file("${CMAKE_CURRENT_LIST_DIR}/rest/xpum-rest.service.in" "${CMAKE_CURRENT_LIST_DIR}/rest/xpum-rest.service" @ONLY)

	file(WRITE "${CMAKE_BINARY_DIR}/package_file_name" "${CPACK_PACKAGE_FILE_NAME}.deb")

	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
	#set(CPACK_DEBIAN_PACKAGE_DEPENDS "liblua5.2-0")

	set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
	set(CPACK_DEBIAN_XPUM_PACKAGE_NAME "${PROJECT_NAME}")
  set(CPACK_DEB_COMPONENT_INSTALL ON)
  set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
	
	set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_LIST_DIR}/install/postinst;${CMAKE_CURRENT_LIST_DIR}/install/prerm;")	
endif()

INCLUDE(CPack)

install(DIRECTORY	core/include/
    DESTINATION ${CPACK_XPUM_INCLUDE_INSTALL_DIR}
)

install(DIRECTORY	
		./doc
		tools/rest/		
	DESTINATION .
)

install(TARGETS xpum xpumcli xpumd
	LIBRARY DESTINATION ${CPACK_XPUM_LIB_INSTALL_DIR}
	RUNTIME DESTINATION bin
	ARCHIVE DESTINATION .
)

install(
    FILES
		tools/fwUpgrade/GfxFwFPT
    PERMISSIONS
		OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE
    DESTINATION bin
)

install(FILES
	"${SYSTEMD_UNIT_NAME}"
	"${CMAKE_CURRENT_LIST_DIR}/rest/xpum-rest.service"
	DESTINATION "/lib/systemd/system")

#install(CODE "execute_process(
#    COMMAND bash -c \"set -e
#    cd ${CPACK_XPUM_INSTALL_DIR}/lib
#    echo -n .. Installing: 
#    sudo ln -sv libDGMCore.so.${PROJECT_VERSION_MAJOR} /lib/libDGMCore.so.${PROJECT_VERSION_MAJOR}
#    \")")
