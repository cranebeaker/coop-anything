#!/usr/bin/env bash
# Launch a TEST copy of Crimson Desert (never the real install) through Proton outside the Steam UI.
# Layout under $1: testinst/game (hardlinked copy of the install with a real bin64 copy), testinst/compatdata (copied prefix).
# Windowed 1280x720 comes from the test prefix's user_engine_option_save.xml (_displayType Windowed, _viewportSize 1280 720).
# STEAM_ROOT: the Steam client folder; STEAM_HOME: the HOME Steam runs with (differs for snap/flatpak Steam).
set -uo pipefail
W=$(realpath "$1"); STEAM=${STEAM_ROOT:?}; C=$STEAM/steamapps/common
export HOME=${STEAM_HOME:-$HOME}
export STEAM_COMPAT_CLIENT_INSTALL_PATH=$STEAM STEAM_COMPAT_DATA_PATH=$W/testinst/compatdata
export SteamAppId=3321460 SteamGameId=3321460
export PROTON_LOG=1 PROTON_LOG_DIR=$W/harness
export WINEDLLOVERRIDES="winmm=n,b"   # Ultimate ASI Loader as winmm.dll
cd "$W/testinst/game/bin64"
exec "$C/SteamLinuxRuntime_4/run" -- "$C/Proton - Experimental/proton" waitforexitandrun "$W/testinst/game/bin64/CrimsonDesert.exe"
