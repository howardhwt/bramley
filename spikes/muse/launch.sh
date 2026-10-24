#!/bin/sh
# Muse Spike: does TortOS's Muse daemon run under NextUI on the Brick's stock FFmpeg?
# Plays spike/test.flac and spike/test.m4b for 15 seconds each and logs everything.

cd "$(dirname "$0")"
PAK_DIR="$(pwd)"
SPIKE_DIR=/mnt/SDCARD/spike
LOG="$SPIKE_DIR/muse-spike.log"
SOCK=/tmp/muse.sock

mkdir -p "$SPIKE_DIR"
: > "$LOG"
log() { echo "=== $(date '+%F %T') $*" >> "$LOG"; }

# Stock /usr/lib first so the system FFmpeg is the one that gets loaded.
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH
export LOGS_PATH="$SPIKE_DIR"

log "environment"
uname -a >> "$LOG" 2>&1
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH" >> "$LOG"
ls -l /usr/lib/libavformat* /usr/lib/libavcodec* /usr/lib/libavfilter* /usr/lib/libavutil* \
	/usr/lib/libswresample* /usr/lib/libasound* >> "$LOG" 2>&1
ls -l "$SPIKE_DIR/test.flac" "$SPIKE_DIR/test.m4b" >> "$LOG" 2>&1

log "ldd muse"
if command -v ldd >/dev/null 2>&1; then
	ldd ./muse >> "$LOG" 2>&1
else
	/lib/ld-linux-aarch64.so.1 --list ./muse >> "$LOG" 2>&1
fi

log "starting muse"
rm -f "$SOCK"
./muse >> "$LOG" 2>&1 &
MUSE_PID=$!

i=0
while [ ! -S "$SOCK" ] && [ $i -lt 50 ]; do
	if ! kill -0 $MUSE_PID 2>/dev/null; then
		wait $MUSE_PID
		log "muse exited before the socket appeared, exit code $?"
		sync
		exit 1
	fi
	sleep 0.1
	i=$((i + 1))
done
if [ ! -S "$SOCK" ]; then
	log "no $SOCK after 5 seconds"
	kill $MUSE_PID 2>/dev/null
	sync
	exit 1
fi
log "socket ready after about $((i * 100)) ms"

for f in test.flac test.m4b; do
	log "PLAY $f"
	"$PAK_DIR/musectl" -t 15 "PLAY\tpath=$SPIKE_DIR/$f" >> "$LOG" 2>&1
	log "musectl exit code $?"
done

log "STOP"
"$PAK_DIR/musectl" -t 1 'STATUS' 'STOP' >> "$LOG" 2>&1

kill $MUSE_PID 2>/dev/null
wait $MUSE_PID 2>/dev/null
log "muse stopped, exit code $?"
sync
