#!/bin/sh
# Blur Spike, stress run: FULL blur repeated 1, 2, 3, 4, 6, 8, 12, 16 times per frame, 10 s each.
# The largest repeat count that holds 60 fps is the GPU headroom. Takes about 80 seconds.
# Results go to /mnt/SDCARD/spike/blur-spike.log. B or MENU quits early.

cd "$(dirname "$0")"
mkdir -p /mnt/SDCARD/spike
OUT=/mnt/SDCARD/spike/blur-spike-stdout.log
./blur --stress >> "$OUT" 2>&1
echo "stress exit code $?" >> "$OUT"
sync
