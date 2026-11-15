cmake_minimum_required(VERSION 3.20)
project(south_park_coop C CXX)

# The game is 32-bit (PE32 i386), so everything here is Win32 x86.
if (NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "This game is 32-bit: configure with a 32-bit toolchain (i686 MinGW or -A Win32)")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")

# MinHook (BSD-2, pinned 1.3.4): the engine resolves the d3d11 entry points before our
# GetProcAddress patch is installed, so the function itself has to be hooked inline.
set(MINHOOK "${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook-1.3.4")
set(BRIDGE "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/bridge/include")

add_library(dinput8 SHARED
    src/proxy.cpp
    src/mod.cpp
    src/net.cpp
    src/remote.cpp
    ${MINHOOK}/src/hook.c
    ${MINHOOK}/src/buffer.c
    ${MINHOOK}/src/trampoline.c
    ${MINHOOK}/src/hde/hde32.c
)
target_include_directories(dinput8 PRIVATE ${MINHOOK}/include ${BRIDGE})
target_compile_definitions(dinput8 PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
set_target_properties(dinput8 PROPERTIES
    OUTPUT_NAME "dinput8"
    PREFIX ""
    SUFFIX ".dll"
)
target_link_libraries(dinput8 PRIVATE d3d9 d3d11 dxgi ws2_32)
if (MINGW)
    target_link_options(dinput8 PRIVATE -Wl,--kill-at -static -static-libgcc -static-libstdc++)
endif()

# The transport can be tested without the game: two endpoints on loopback.
add_executable(net_selftest src/net_selftest.cpp src/net.cpp)
target_compile_definitions(net_selftest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(net_selftest PRIVATE ws2_32)
# `-static` also folds in libwinpthread, so the test runs from a shell with no MinGW on PATH.
target_link_options(net_selftest PRIVATE -static)

# The remote body can be tested the same way: snapshots in, poses out, no sockets used.
add_executable(remote_selftest src/remote_selftest.cpp src/remote.cpp src/net.cpp)
target_compile_definitions(remote_selftest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(remote_selftest PRIVATE ws2_32)
target_link_options(remote_selftest PRIVATE -static)

