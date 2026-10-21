cmake_minimum_required(VERSION 3.21)
project(coop_hotreload LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "hotreload swaps DLLs inside a running Windows process; it builds on Windows only")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# One static runtime everywhere. The loader and the core each carry their own copy of it, which is why the boundary
# between them (include/hotreload/abi.h) is plain C. A mod that uses the DLL runtime works as well.
if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(HOTRELOAD_MUTATION_TWINS "Compile the deliberately broken variants (HOTRELOAD_MUT=n); the tests need them" OFF)
option(HOTRELOAD_BUILD_EXAMPLES "Build the example cores and the test host" ${PROJECT_IS_TOP_LEVEL})
option(HOTRELOAD_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})

if(PROJECT_IS_TOP_LEVEL)
  # One output folder for every configuration, so a script can find the programs without knowing the generator.
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/bin)
  endforeach()
endif()

function(hr_common target)
  target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
  if(HOTRELOAD_MUTATION_TWINS)
    target_compile_definitions(${target} PRIVATE HOTRELOAD_RUNTIME_MUT)
  endif()
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /EHsc)
  endif()
endfunction()

# ---- libraries -------------------------------------------------------------------------------------------------------
# hotreload_control : the control pipe (JSON lines over a named pipe). No other dependency.
add_library(hotreload_control STATIC src/control_pipe.cpp)
hr_common(hotreload_control)

# hotreload_host    : the permanent half. Link it into your own plugin, or use the DLL below.
add_library(hotreload_host STATIC src/loader_host.cpp)
hr_common(hotreload_host)
target_link_libraries(hotreload_host PUBLIC hotreload_control)

# hotreload_core    : the reloadable half. Link it into your core DLL: state registry, threads, drain, glue.
add_library(hotreload_core STATIC src/reload_state.cpp src/reload_drain.cpp src/core_glue.cpp)
hr_common(hotreload_core)

# hotreload_loader  : the permanent half as a DLL with a plain C API.
add_library(hotreload_loader SHARED src/loader_dll.cpp)
hr_common(hotreload_loader)
target_link_libraries(hotreload_loader PRIVATE hotreload_host)

# ---- example cores and the test host ----------------------------------------------------------------------------------
if(HOTRELOAD_BUILD_EXAMPLES)
  # One source, several builds. Every build differs in bytes (EX_BUILD) and some in behaviour (see example_core.cpp).
  function(hr_example_core name)
    add_library(${name} SHARED examples/core/example_core.cpp)
    hr_common(${name})
    target_compile_definitions(${name} PRIVATE ${ARGN})
    target_link_libraries(${name} PRIVATE hotreload_core hotreload_control)
    set_target_properties(${name} PROPERTIES PREFIX "")
  endfunction()
  hr_example_core(core_v1 EX_BUILD=1 EX_LAYOUT=1)
  hr_example_core(core_v2 EX_BUILD=2 EX_LAYOUT=2 EX_MIGRATE=1)
  hr_example_core(core_v3_nomigrate EX_BUILD=3 EX_LAYOUT=3 EX_MIGRATE=0)
  hr_example_core(core_unregistered EX_BUILD=4 EX_LAYOUT=1 EX_RAW_THREAD=1)
  hr_example_core(core_failsave EX_BUILD=5 EX_LAYOUT=1 EX_FAIL_SAVE=1)
  hr_example_core(core_badinit EX_BUILD=6 EX_LAYOUT=1 EX_FAIL_INIT=1)

  add_executable(hotreload_test_host examples/host/test_host.cpp)
  hr_common(hotreload_test_host)
  target_link_libraries(hotreload_test_host PRIVATE hotreload_control)
endif()

# ---- unit tests --------------------------------------------------------------------------------------------------------
if(HOTRELOAD_BUILD_TESTS)
  add_executable(hotreload_unit_tests tests/unit_tests.cpp)
  hr_common(hotreload_unit_tests)
  target_link_libraries(hotreload_unit_tests PRIVATE hotreload_core hotreload_control)
endif()
