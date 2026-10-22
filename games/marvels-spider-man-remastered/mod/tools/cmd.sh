#!/bin/bash
# Send one command to a running instance and wait for it to be picked up. Usage: cmd.sh <pid> <command...>
P=$1; shift
: "${COOP_GAME_DIR:?set COOP_GAME_DIR to the test copy of the game folder}"
F="$COOP_GAME_DIR/coop_cmd_$P.txt"
printf '%s' "$*" > "$F"
for i in $(seq 1 100); do [ -e "$F" ] || exit 0; sleep 0.1; done
rm -f "$F"; echo "not picked up"; exit 1
