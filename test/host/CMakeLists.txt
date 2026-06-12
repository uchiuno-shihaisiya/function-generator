cmake_minimum_required(VERSION 3.14)
project(fn_gen_host_tests CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(test_logic test_logic.cpp)
target_include_directories(test_logic PRIVATE ${CMAKE_SOURCE_DIR}/../..)

add_test(NAME logic COMMAND test_logic)
