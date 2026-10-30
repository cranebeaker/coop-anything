#!/bin/bash
# After a test: move new crash files out, restore the saves from a backup and verify by hash.
# Backups live in $COOP_BACKUP/<stamp> with a hash list $COOP_BACKUP/hashes_<stamp>.txt.
# Files not in the backup (new save slots) are moved to $COOP_BACKUP/test-modified-<date>/<tag>,
# crash files to $COOP_BACKUP/crash/<date>/<tag>.
# Usage: restore_saves.sh <backup stamp e.g. 20261021-1854> <tag e.g. puppet2>
set -u
: "${COOP_BACKUP:?set COOP_BACKUP to the backup folder}"
B=$COOP_BACKUP/$1; TAG=$2
D="$(cygpath -u "$USERPROFILE")/Documents/Marvel's Spider-Man Remastered"
R="$(cygpath -u "$APPDATA")/Insomniac Games/Marvel's Spider-Man Remastered"
X=$COOP_BACKUP/crash/$(date +%Y%m%d)/$TAG
M=$COOP_BACKUP/test-modified-$(date +%Y%m%d)/$TAG
T=$(mktemp -d)
snap() { rm -rf "$T/s"; mkdir -p "$T/s/Documents" "$T/s/Roaming-Insomniac"; cp -a "$D/." "$T/s/Documents/"; cp -a "$R/." "$T/s/Roaming-Insomniac/"; (cd "$T/s" && find . -type f -exec md5sum {} + | sort -k2) > "$T/now.txt"; }
snap
# files that are new or changed, relative to the backup root layout
diff <(cut -d' ' -f2- $COOP_BACKUP/hashes_$1.txt) <(cut -d' ' -f2- "$T/now.txt") | sed -n 's/^> \*\.\///p' > "$T/new.txt"
while IFS= read -r f; do
  case "$f" in
    Documents/*) src="$D/${f#Documents/}";;
    Roaming-Insomniac/*) src="$R/${f#Roaming-Insomniac/}";;
  esac
  case "$f" in
    */crs/*|*.mdmp) dst="$X/${f#*/}";;
    *) dst="$M/${f#*/}";;
  esac
  mkdir -p "$(dirname "$dst")"; mv "$src" "$dst"
done < "$T/new.txt"
# changed files: copy back from the backup
(cd "$B" && find . -type f -exec md5sum {} + | sort -k2) > "$T/bak.txt"
diff "$T/bak.txt" $COOP_BACKUP/hashes_$1.txt >/dev/null || { echo "backup itself differs from its hash list!"; exit 1; }
snap
awk 'NR==FNR{h[substr($0,37)]=$1;next} (substr($0,37) in h) && h[substr($0,37)]!=$1 {print substr($0,37)}' $COOP_BACKUP/hashes_$1.txt "$T/now.txt" | while IFS= read -r f; do
  case "$f" in
    Documents/*) cp -a "$B/$f" "$D/${f#Documents/}";;
    Roaming-Insomniac/*) cp -a "$B/$f" "$R/${f#Roaming-Insomniac/}";;
  esac
  echo "restored $f"
done
snap
diff $COOP_BACKUP/hashes_$1.txt "$T/now.txt" && echo IDENTICAL
[ -d "$M" ] && echo "new files moved to $M"
[ -d "$X" ] && echo "crash files moved to $X"
rm -rf "$T"
