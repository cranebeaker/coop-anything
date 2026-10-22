#!/usr/bin/env bash
# One-PC relay test of quest accept sharing: cdmp_peer relays everything back; the stage and
# quest feeds send a recorded meeting scene + accept as the "host's", the guest half applies them.
# Usage: echo_quest_test.sh <workspace root> <name> [feed file in save-backups/renee-record]
# Layout under the workspace root: harness/ (run_game.sh, echo_test.sh, this script), testinst/game and
# testinst/compatdata (the test copy, see run_game.sh), testinst/save-backups/before-record (the save restore
# point), testinst/save-backups/cdmp.ini.before-human-record (the base cdmp.ini), testinst/save-backups/renee-record
# (replay files built with tools/re/rec_from_capture.py). Never point it at a real install or its saves.
W=$(realpath "$1"); name=$2; feed=${3:-share_feed.txt}
G=$W/testinst/game/bin64; S="$W/testinst/compatdata/pfx/drive_c/users/steamuser/AppData/Local/Pearl Abyss/CD/save"
out=$W/testinst/save-backups/echo-$name; mkdir -p "$out"
restore() { rsync -a --delete "$W/testinst/save-backups/before-record/" "$S/" && diff -rq "$W/testinst/save-backups/before-record" "$S" >/dev/null; }
pgrep -x steam >/dev/null || { echo "STEAM NOT RUNNING, stopped"; exit 1; }
restore || { echo "restore failed, stopped"; exit 1; }
cp "$W/testinst/save-backups/cdmp.ini.before-human-record" "$G/cdmp.ini"
sed -i -e 's/^stage_sync=.*/stage_sync=1\nquest_share=1/' -e 's/^test_walk_s=.*/test_walk_s=0/' \
  -e 's/^test_autoload=.*/test_autoload=1/' -e 's/^test_autoquit=.*/test_autoquit=1/' -e 's/^stage_record=.*/stage_record=0/' \
  -e 's/^stage_replay=.*/stage_replay=0/' -e 's/^stage_sync_feed=.*/stage_sync_feed=1\nquest_share_feed=1/' "$G/cdmp.ini"
cp "$W/testinst/save-backups/renee-record/$feed" "$G/cdmp_stage_record.txt"
"$W/harness/echo_test.sh" "$W"
sleep 5
cp "$G/cdmp.log" "$W/harness/peer.log" "$out/"; mv "$G"/cdmp_packets_* "$out/" 2>/dev/null
restore && saves=restored || saves=RESTORE-FAILED
rm -f "$G/cdmp_stage_record.txt"; cp "$W/testinst/save-backups/cdmp.ini.before-human-record" "$G/cdmp.ini"
echo "$name: saves $saves"
grep -E "Quest share|QuestShare|Stage sync|StageSync|Co-op: (connected|linked)|version" "$out/cdmp.log" | cut -c16-260 | tail -40
