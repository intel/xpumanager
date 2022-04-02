cmake_minimum_required(VERSION 3.10.0)
set(CMAKE_CXX_STANDARD 11)

find_package(Threads REQUIRED)

# Find Protobuf installation Looks for protobuf-config.cmake file installed by
# Protobuf's cmake installation.
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG)
if(${Protobuf_FOUND})
  message(STATUS "Using protobuf.cmake ${Protobuf_VERSION}")
  set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
else()
  message(STATUS "protobuf.cmake not found, use system provided lib")
  set(_PROTOBUF_LIBPROTOBUF protobuf)
endif()

# Find gRPC installation Looks for gRPCConfig.cmake file installed by gRPC's
# cmake installation.
find_package(gRPC CONFIG)
if(${gRPC_FOUND})
  message(STATUS "Using gRPC.cmake ${gRPC_VERSION}")
  set(_GRPC_GRPCPP gRPC::grpc++)
else()
  message(STATUS "gRPC.cmake not found, use system provided lib")
  set(_GRPC_GRPCPP grpc++)
endif()


# GRPC and Protoc tools
find_program(_PROTOBUF_PROTOC protoc)
find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
message(STATUS "protoc: ${_PROTOBUF_PROTOC}")
message(STATUS "grpc_cpp_plugin: ${_GRPC_CPP_PLUGIN_EXECUTABLE}")
