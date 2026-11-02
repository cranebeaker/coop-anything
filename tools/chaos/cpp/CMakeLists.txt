cmake_minimum_required(VERSION 3.16)
project(coopchaos CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-only C++ mirror of tools/chaos/impair.py plus a cross-check driver.
# tools/chaos/xcheck.py runs the driver and the Python model on the same script and compares every result.
add_executable(chaos_xcheck xcheck.cpp)
target_include_directories(chaos_xcheck PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Twin builds: each COOPCHAOS_MUTANT value breaks one behaviour in the C++ mirror. The cross-check must then fail.
foreach(m 1 2 3 4)
  add_executable(chaos_xcheck_m${m} xcheck.cpp)
  target_include_directories(chaos_xcheck_m${m} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(chaos_xcheck_m${m} PRIVATE COOPCHAOS_MUTANT=${m})
endforeach()

if(MSVC)
  foreach(t chaos_xcheck chaos_xcheck_m1 chaos_xcheck_m2 chaos_xcheck_m3 chaos_xcheck_m4)
    target_compile_options(${t} PRIVATE /W4 /permissive- /utf-8 /fp:precise)
  endforeach()
else()
  # No fused multiply-add and no fast-math: the mirror must round exactly like Python.
  foreach(t chaos_xcheck chaos_xcheck_m1 chaos_xcheck_m2 chaos_xcheck_m3 chaos_xcheck_m4)
    target_compile_options(${t} PRIVATE -Wall -Wextra -ffp-contract=off)
  endforeach()
endif()
