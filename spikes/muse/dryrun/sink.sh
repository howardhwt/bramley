#!/bin/sh
# Called by ALSA's file plugin each time Muse opens the PCM.
exec cat > "/mnt/SDCARD/out-$(date +%s%N).wav"
