#!/bin/sh
# Installed as nextui.elf, which MinUI.pak/launch.sh runs in its loop. Starts the Brick Hammer
# launcher, and switches to the stock launcher (nextui-stock.elf) after two crashes in a row that
# each happen within 5 seconds of starting, so a broken build never locks the user out.
# The crash count lives in /tmp, so a reboot tries the new launcher again.

BIN=$(dirname "$0")
COUNT_FILE=/tmp/launcher_crashes
count=$(cat "$COUNT_FILE" 2>/dev/null || echo 0)

if [ "$count" -ge 2 ] && [ -x "$BIN/nextui-stock.elf" ]; then
	exec "$BIN/nextui-stock.elf" "$@"
fi

start=$(date +%s)
"$BIN/launcher.elf" "$@"
code=$?
elapsed=$(( $(date +%s) - start ))

if [ "$code" -ne 0 ] && [ "$elapsed" -lt 5 ]; then
	count=$((count + 1))
	echo "$count" > "$COUNT_FILE"
	echo "launcher.elf exited with code $code after ${elapsed}s (crash $count)"
	if [ "$count" -ge 2 ] && [ -x "$BIN/nextui-stock.elf" ]; then
		echo "falling back to nextui-stock.elf"
		exec "$BIN/nextui-stock.elf" "$@"
	fi
else
	rm -f "$COUNT_FILE"
fi
