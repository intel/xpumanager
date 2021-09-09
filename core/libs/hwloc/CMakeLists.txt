cmake_minimum_required (VERSION 3.2.0)

project (hwloc)

if(EXISTS "/usr/local/hwloc/lib/libhwloc.a")
	message("found hwloc library")
else()
	message("build hwloc library")
	execute_process(
		COMMAND ${CMAKE_CURRENT_LIST_DIR}/autogen.sh --with-pic
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		RESULT_VARIABLE rv
	)	
	message("autogen='${rv}'")

	execute_process(
		COMMAND ${CMAKE_CURRENT_LIST_DIR}/configure  --prefix=/usr/local/hwloc --enable-static --disable-shared LDFLAGS="--static" CFLAGS="-fPIC"
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		RESULT_VARIABLE rv
	)	
	message("configure='${rv}'")

	execute_process(
		COMMAND make -j
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		RESULT_VARIABLE rv
	)
	message("make='${rv}'")

	execute_process(
		COMMAND make install
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		RESULT_VARIABLE rv
	)
	message("install='${rv}'")
endif()
	


