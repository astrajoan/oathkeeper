cmake_minimum_required(VERSION 3.27)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

include_directories(${GTEST_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(all_tests main.cpp crash_tests.cpp mr_impl_tests.cpp)
target_link_libraries(all_tests PRIVATE mr_master mr_worker GTest::gtest)
gtest_discover_tests(all_tests)
