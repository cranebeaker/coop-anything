cmake_minimum_required(VERSION 3.16)
project(cofr_recorder CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The recorder: one source file, standard library only. Add this folder to your include path and cofr.cpp to your mod.
add_library(cofr STATIC cofr.cpp)
target_include_directories(cofr PUBLIC ${CMAKE_CURRENT_LIST_DIR})
if(NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(cofr PUBLIC Threads::Threads)
endif()

add_executable(cofr_tests tests/test_recorder.cpp)
target_link_libraries(cofr_tests PRIVATE cofr)

# The example from the README, built and run so it cannot go stale.
add_executable(cofr_example example_usage.cpp)
target_link_libraries(cofr_example PRIVATE cofr)

if(MSVC)
  foreach(t cofr cofr_tests cofr_example)
    target_compile_options(${t} PRIVATE /W4 /permissive- /utf-8 /D_CRT_SECURE_NO_WARNINGS)
  endforeach()
else()
  foreach(t cofr cofr_tests cofr_example)
    target_compile_options(${t} PRIVATE -Wall -Wextra)
  endforeach()
endif()

enable_testing()
set(COFR_GOLDEN "${CMAKE_CURRENT_LIST_DIR}/../golden" CACHE PATH "Folder with the checked-in golden traces")
set(COFR_TEST_ENV "COFR_GOLDEN_DIR=${COFR_GOLDEN}")

add_test(NAME recorder_unit COMMAND cofr_tests --group recorder)
add_test(NAME recorder_golden COMMAND cofr_tests --group golden)
add_test(NAME recorder_perf COMMAND cofr_tests --group perf)
add_test(NAME recorder_example COMMAND cofr_example example_out)
set_tests_properties(recorder_unit recorder_golden recorder_perf PROPERTIES ENVIRONMENT "${COFR_TEST_ENV}")

# One test per mutation twin: it re-introduces one bug and must make the matching check fail.
foreach(twin drop_newest no_net_in zero_seq no_trunc_flag no_dropped_count no_applied_flags incremental_skips_update
             buckets_ignore_value detail_drops_key no_derived_digest no_schema_record custom_cuts_payload)
  add_test(NAME twin_${twin} COMMAND cofr_tests --group twin --name ${twin})
endforeach()
