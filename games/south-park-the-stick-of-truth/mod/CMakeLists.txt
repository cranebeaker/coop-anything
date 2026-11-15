cmake_minimum_required(VERSION 3.20)
project(south_park_coop CXX)

# The game is 32-bit (PE32 i386), so everything here is Win32 x86.
if (NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "This game is 32-bit: configure with a 32-bit toolchain (i686 MinGW or -A Win32)")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")

add_library(dinput8 SHARED
    src/proxy.cpp
    src/mod.cpp
)
target_compile_definitions(dinput8 PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
# A proxy DLL must be named exactly dinput8.dll and sit next to the game exe.
set_target_properties(dinput8 PROPERTIES
    OUTPUT_NAME "dinput8"
    PREFIX ""
    SUFFIX ".dll"
)
if (MINGW)
    # The export has to keep its name; MinGW marks it with __declspec(dllexport) or a .def.
    target_link_options(dinput8 PRIVATE -Wl,--kill-at)
endif()
