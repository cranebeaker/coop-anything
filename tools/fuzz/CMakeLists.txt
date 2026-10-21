cmake_minimum_required(VERSION 3.16)
project(coopfuzz LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# The targets compiled into the executable. Pass -DCOOPFUZZ_TARGET_SOURCES="my_target.cpp;example/example_target.cpp" (a
# ;-separated list) to use the file that wraps your own sync core, see GUIDE.md. Every listed file registers itself with a
# static coopfuzz::TargetRegistrar. The self tests always build the two example targets.
if(NOT DEFINED COOPFUZZ_TARGET_SOURCES)
  set(COOPFUZZ_TARGET_SOURCES example/example_target.cpp example/skeleton_target.cpp)
endif()
option(COOPFUZZ_TESTS "Build the self tests of the framework and the example target" ON)

add_library(coopfuzz_core STATIC src/net.cpp src/scenario.cpp src/oracles.cpp src/runner.cpp src/targets.cpp)
target_include_directories(coopfuzz_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(coopfuzz src/main.cpp ${COOPFUZZ_TARGET_SOURCES})
target_link_libraries(coopfuzz PRIVATE coopfuzz_core)
set(COOPFUZZ_ALL coopfuzz_core coopfuzz)

if(COOPFUZZ_TESTS)
  add_executable(coopfuzz_tests tests/tests.cpp example/example_target.cpp example/skeleton_target.cpp)
  target_include_directories(coopfuzz_tests PRIVATE tests)
  target_link_libraries(coopfuzz_tests PRIVATE coopfuzz_core)
  list(APPEND COOPFUZZ_ALL coopfuzz_tests)
endif()

if(MSVC)
  foreach(t ${COOPFUZZ_ALL})
    target_compile_options(${t} PRIVATE /W4 /permissive- /utf-8 /D_CRT_SECURE_NO_WARNINGS)
  endforeach()
else()
  foreach(t ${COOPFUZZ_ALL})
    target_compile_options(${t} PRIVATE -Wall -Wextra)
  endforeach()
endif()
