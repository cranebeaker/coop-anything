#!/usr/bin/env bash
# Unattended: replay the recorded quest accept with the server rand state pinned, once per value given.
# Usage: [REPLAY_FILE=name in save-backups/renee-record] [EXTRA_INI="k=v;k=v"] rng_pin_runs.sh <workspace root> <name=rng value> ...   (rng 0 = no pin)   Saves restored before and after every run.
# Layout under the workspace root: harness/ (run_game.sh, echo_test.sh, this script), testinst/game and
# testinst/compatdata (the test copy, see run_game.sh), testinst/save-backups/before-record (the save restore
# point), testinst/save-backups/cdmp.ini.before-human-record (the base cdmp.ini), testinst/save-backups/renee-record
# (replay files built with tools/re/rec_from_capture.py). Never point it at a real install or its saves.
W=$(realpath "$1"); shift
G=$W/testinst/game/bin64; S="$W/testinst/compatdata/pfx/drive_c/users/steamuser/AppData/Local/Pearl Abyss/CD/save"
restore() { rsync -a --delete "$W/testinst/save-backups/before-record/" "$S/" && diff -rq "$W/testinst/save-backups/before-record" "$S" >/dev/null; }
cp "$W/testinst/save-backups/cdmp.ini.before-human-record" "$G/cdmp.ini"
sed -i -e '/^\[net\]/,/^\[/{s/^enabled=1/enabled=0/}' -e 's/^test_walk_s=.*/test_walk_s=0/' -e 's/^test_autoload=.*/test_autoload=1/' \
  -e 's/^test_autoquit=.*/test_autoquit=1/' -e 's/^stage_record=.*/stage_record=0/' -e 's/^stage_replay=.*/stage_replay=1/' "$G/cdmp.ini"
for nv in "$@"; do
  name=${nv%%=*}; val=${nv#*=}; out=$W/testinst/save-backups/rngpin-$name; mkdir -p "$out"
  pgrep -x steam >/dev/null || { echo "$name: STEAM NOT RUNNING, stopped"; exit 1; }
  restore || { echo "$name: restore failed, stopped"; exit 1; }
  sed -i '/^stage_replay_rng=/d; /^mission_watch/d' "$G/cdmp.ini"; sed -i "s/^stage_replay=1/stage_replay=1\nstage_replay_rng=$val/" "$G/cdmp.ini"
  # EXTRA_INI: more [dev] lines, separated by ';' (e.g. "mission_watch=1;mission_watch_tmpl_off=8")
  for kv in ${EXTRA_INI//;/ }; do sed -i "s/^stage_replay=1/stage_replay=1\n$kv/" "$G/cdmp.ini"; done
  cp "$W/testinst/save-backups/renee-record/${REPLAY_FILE:-replay_dialog.txt}" "$G/cdmp_stage_record.txt"
  timeout 900 "$W/harness/run_game.sh" "$W" > "$out/game.log" 2>&1; rc=$?
  sleep 5
  cp "$G/cdmp.log" "$out/"; mv "$G"/cdmp_packets_* "$out/" 2>/dev/null
  restore && saves=restored || saves=RESTORE-FAILED
  echo "$name (rng $val): exit $rc, saves $saves"
  grep -E "QuestRng|MissionWatch|StageReplay: (branch|NPC)|mission 0xFFFFF7F8|RESULT" "$out/cdmp.log" | cut -c16-250
done
rm -f "$G/cdmp_stage_record.txt"; cp "$W/testinst/save-backups/cdmp.ini.before-human-record" "$G/cdmp.ini"
