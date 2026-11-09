#!/bin/sh
# Builds DDCoop.exe with MinGW-w64: cross-compiling on Linux (CT116), or natively in MSYS2 UCRT64 with CROSS=.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/win
VERSION="${DDC_VERSION:-1.0.0}"
# DDC_TEST_NOSAVE=1: a test kit; the host's game also runs with -nosave, so tests never change the host's save.
NOSAVE="${DDC_TEST_NOSAVE:-0}"
# Tool prefix; set CROSS= (empty) in an MSYS2 UCRT64 shell on Windows, where the tools are plain gcc/windres/strip.
P="${CROSS-x86_64-w64-mingw32-}"
"${P}windres" -I launcher/src launcher/src/ddcoop.rc -O coff -o build/win/ddcoop.res
"${P}gcc" -std=c11 -O2 -Wall -Wextra -Werror -municode -mwindows -Ilauncher/src \
    -D_WIN32_WINNT=0x0601 -DDDC_VERSION="\"$VERSION\"" -DDDC_TEST_NOSAVE="$NOSAVE" \
    -o build/win/DDCoop.debug.exe \
    launcher/src/main.c launcher/src/lr_install.c launcher/src/lr_steam.c launcher/src/lr_core.c launcher/src/lr_relay.c \
    launcher/src/lr_wire.c launcher/src/lr_files.c launcher/src/lr_sock.c launcher/src/lr_owners.c \
    launcher/src/lr_roster.c launcher/src/lr_snapshot.c launcher/src/lr_prepare_win.c build/win/ddcoop.res \
    -lws2_32 -lbcrypt -luuid -lwinmm -lshell32 -lole32 -lcomctl32 -luser32 -lgdi32 -ladvapi32 -static -static-libgcc -g
"${P}strip" -o build/win/DDCoop.exe build/win/DDCoop.debug.exe
echo "built build/win/DDCoop.exe ($VERSION, test nosave=$NOSAVE)"
