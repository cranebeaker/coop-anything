cmake_minimum_required(VERSION 3.15)
project(coop_mut_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Where coop_mut.h comes from. run_tests.py points this at a broken copy to prove that the tests notice a broken header.
set(COOP_MUT_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "folder with coop_mut.h")

# The test shape: twins can be switched on with COOP_MUT.
add_executable(coop_mut_cpp_tests cpp_under_test.cpp)
target_include_directories(coop_mut_cpp_tests PRIVATE "${COOP_MUT_HEADER_DIR}")
target_compile_definitions(coop_mut_cpp_tests PRIVATE COOP_RUNTIME_MUT)

# The shipped shape: no COOP_RUNTIME_MUT, so COOP_MUT_IF is the constant false.
add_executable(coop_mut_cpp_prod cpp_under_test.cpp)
target_include_directories(coop_mut_cpp_prod PRIVATE "${COOP_MUT_HEADER_DIR}")

set_target_properties(coop_mut_cpp_tests coop_mut_cpp_prod PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
