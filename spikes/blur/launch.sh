#!/bin/sh
# Blur Spike: two passes of FULL 60 s, HALF 30 s, QUARTER 30 s, NONE 15 s.
# Pass 1 runs with vsync, pass 2 asks for no vsync to see how far past 60 fps the GPU goes.
# Results go to /mnt/SDCARD/spike/blur-spike.log. B or MENU ends the current pass.

cd "$(dirname "$0")"
mkdir -p /mnt/SDCARD/spike
OUT=/mnt/SDCARD/spike/blur-spike-stdout.log
WALLPAPER=/mnt/SDCARD/spike/wallpaper.bmp
ARGS=""
[ -f "$WALLPAPER" ] && ARGS="--wallpaper $WALLPAPER"

./blur --auto $ARGS >> "$OUT" 2>&1
echo "pass 1 exit code $?" >> "$OUT"
./blur --auto --novsync $ARGS >> "$OUT" 2>&1
echo "pass 2 exit code $?" >> "$OUT"
sync
