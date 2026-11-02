#!/usr/bin/env bash
# One-PC echo test: cdmp_peer relay + the test copy of the game; stops the relay afterwards.
# Layout under $1 as in run_game.sh, plus harness/ (these scripts and their logs), cdmp/build-x/bin/cdmp_peer.exe
# and wineprefix-tests/ (a scratch Wine prefix for the relay). STEAM_ROOT: the Steam client folder.
W=$(realpath "$1"); C=${STEAM_ROOT:?}/steamapps/common
export WINEPREFIX=$W/wineprefix-tests WINEDEBUG=-all
"$C/SteamLinuxRuntime_4/run" -- "$C/Proton - Experimental/files/bin/wine" "$W/cdmp/build-x/bin/cdmp_peer.exe" --mode relay --delay ${PEER_DELAY:-50} --offset 0 ${PEER_EXTRA:-} > "$W/harness/peer.log" 2>&1 &
sleep 8
timeout 900 "$W/harness/run_game.sh" "$W" > "$W/harness/game-echo.log" 2>&1; rc=$?
"$C/SteamLinuxRuntime_4/run" -- "$C/Proton - Experimental/files/bin/wineserver" -k 2>/dev/null
wait
echo "game exited: $rc"
