#!/usr/bin/env bash
# Usage: build.sh <absolute source dir> [build type]. Prints BUILD_OK or BUILD_FAILED.
set -uo pipefail
SRC=$(realpath "$1"); TYPE=${2:-RelWithDebInfo}
podman run --rm -v "$SRC":/src:Z -v cdmp-fetch:/fetch -w /src localhost/cdmp-xbuild:1 bash -c "
  set -e
  cmake -S . -B build-x -G Ninja -DCMAKE_BUILD_TYPE=$TYPE -DCMAKE_TOOLCHAIN_FILE=/opt/clang-cl-msvc.cmake -DFETCHCONTENT_BASE_DIR=/fetch
  cmake --build build-x -j 16
" && echo BUILD_OK || { echo BUILD_FAILED; exit 1; }
