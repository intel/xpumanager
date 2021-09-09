cmake_minimum_required (VERSION 3.2.0)

project (hwloc)

if(EXISTS "/usr/local/hwloc/lib/libhwloc.a")
	message("found hwloc library")
else()
	message("build hwloc library")
	message("exec: ${CMAKE_CURRENT_LIST_DIR}/build.sh")
	execute_process(
		COMMAND ${CMAKE_CURRENT_LIST_DIR}/build.sh		
		RESULT_VARIABLE rv
	)	
	message("hwloc err:"${rv})

	execute_process(
		COMMAND cat ${CMAKE_CURRENT_LIST_DIR}/hwloc.log	
	)
endif()
	


