cmake_minimum_required (VERSION 3.2.0)

project (hwloc)

message("build hwloc library")
execute_process(
	COMMAND ${CMAKE_CURRENT_LIST_DIR}/build.sh		
	RESULT_VARIABLE rv
)	
message("build hwloc result:"${rv})
	


