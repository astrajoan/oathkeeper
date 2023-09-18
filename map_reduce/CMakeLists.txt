cmake_minimum_required(VERSION 3.27)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

project(map_reduce)

set(PACKAGES gflags glog Protobuf gRPC asio-grpc)
foreach(package IN LISTS PACKAGES)
    find_package(${package} CONFIG REQUIRED)
endforeach()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()
add_subdirectory(tests)

add_library(mr_proto OBJECT)
target_link_libraries(
    mr_proto PUBLIC protobuf::libprotobuf gRPC::grpc++_unsecure)

asio_grpc_protobuf_generate(
    GENERATE_GRPC GENERATE_MOCK_CODE
    TARGET mr_proto
    USAGE_REQUIREMENT PUBLIC
    OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/proto/map_reduce.proto")

# header-only library since worker is a template class
add_library(mr_worker INTERFACE Worker.h)
target_link_libraries(
    mr_worker INTERFACE mr_proto asio-grpc::asio-grpc gflags glog::glog)

add_library(mr_master Master.cpp)
target_link_libraries(
    mr_master PUBLIC mr_proto asio-grpc::asio-grpc gflags glog::glog)

add_executable(worker worker_main.cpp)
target_link_libraries(worker PRIVATE mr_worker mr_proto)

add_executable(master master_main.cpp)
target_link_libraries(master PRIVATE mr_master)

add_executable(sequential sequential.cpp)
target_link_libraries(sequential PRIVATE mr_master mr_worker)

add_executable(distributed distributed.cpp)
target_link_libraries(distributed PRIVATE mr_master mr_worker)

file(GLOB INPUT_FILES files/*.txt)
file(COPY ${INPUT_FILES} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/tests")
