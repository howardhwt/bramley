#!/bin/sh
# Blur Spike: runs FULL 60 s, HALF 30 s, QUARTER 30 s, NONE 15 s, then exits.
# Results go to /mnt/SDCARD/spike/blur-spike.log. B or MENU quits early.

cd "$(dirname "$0")"
mkdir -p /mnt/SDCARD/spike
WALLPAPER=/mnt/SDCARD/spike/wallpaper.bmp
if [ -f "$WALLPAPER" ]; then
	./blur --auto --wallpaper "$WALLPAPER" >> /mnt/SDCARD/spike/blur-spike-stdout.log 2>&1
else
	./blur --auto >> /mnt/SDCARD/spike/blur-spike-stdout.log 2>&1
fi
echo "exit code $?" >> /mnt/SDCARD/spike/blur-spike-stdout.log
sync
