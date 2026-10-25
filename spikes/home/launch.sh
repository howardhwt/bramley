#!/bin/sh
# Home Spike: a normal 60 s run with a snapshot-blur panel every 6 s, then a 40 s stress run.
# Results go to /mnt/SDCARD/spike/home-spike.log. B or MENU ends the current run.

cd "$(dirname "$0")"
mkdir -p /mnt/SDCARD/spike
OUT=/mnt/SDCARD/spike/home-spike-stdout.log
./home >> "$OUT" 2>&1
echo "normal exit code $?" >> "$OUT"
./home --stress >> "$OUT" 2>&1
echo "stress exit code $?" >> "$OUT"
sync
