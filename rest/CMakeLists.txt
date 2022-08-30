cmake_minimum_required(VERSION 3.14.0)

configure_file("${CMAKE_CURRENT_LIST_DIR}/xpum_rest.service.in"
               "${CMAKE_CURRENT_LIST_DIR}/xpum_rest.service")

add_custom_target(rest ALL)

add_custom_command(
  TARGET rest
  PRE_BUILD
  COMMAND /bin/sh -c "${CMAKE_CURRENT_LIST_DIR}/gen_grpc_py_files.sh" DEPENDS
          "${CMAKE_CURRENT_LIST_DIR}core_pb2.py")

install(
  DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  DESTINATION .
  PATTERN "*.py"
  PATTERN "*.txt"
  PATTERN "*.md"
  PATTERN "*.pem" EXCLUDE
  PATTERN "__pycache__" EXCLUDE
  PATTERN "conf/rest.conf" EXCLUDE
  PATTERN "xpum_rest.service" EXCLUDE
  PATTERN "xpum_rest.service.in" EXCLUDE
  # PATTERN ".gitignore" EXCLUDE
  PATTERN "CMakeLists.txt" EXCLUDE
  # PATTERN "ifwi_fake" EXCLUDE PATTERN "gen_grpc_py_files.sh" EXCLUDE PATTERN
  # "rest.conf" EXCLUDE
)
