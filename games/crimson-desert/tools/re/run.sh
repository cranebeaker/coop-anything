#!/usr/bin/env bash
# Runs one of these scripts in the hash-pinned analysis image (podman build -t cdmp-re:1 .),
# with the game's bin64 mounted read-only at /game. Usage: GAME_BIN64="<game dir>/bin64" ./run.sh dis.py /game/CrimsonDesert.exe 2a5d330 60
S=$(dirname "$(realpath "$0")")
podman run --rm -v "$S":/s:ro,Z -v "${GAME_BIN64:?set GAME_BIN64 to <game dir>/bin64}":/game:ro localhost/cdmp-re:1 python -I /s/"$@"
