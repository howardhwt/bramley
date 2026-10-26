#!/bin/sh
# Brick Hammer Launcher installer Pak.
#
# Run it once to install the launcher, run it again to remove it. Installing keeps the stock
# NextUI launcher as nextui-stock.elf, which the safety net (nextui-wrapper.sh, installed as
# nextui.elf) falls back to if the new launcher ever crashes on start. Removing puts the stock
# launcher back. Music bookmarks and launcher settings in .userdata are kept either way.
#
# A NextUI update can restore the stock nextui.elf; run this Pak again afterwards to reinstall.

cd "$(dirname "$0")"
PAK="$(pwd)"
BIN="$SYSTEM_PATH/bin"
RES="$SDCARD_PATH/.system/res"
LOG="${LOGS_PATH:-$SDCARD_PATH/.userdata/$PLATFORM/logs}/brick-hammer-launcher.txt"
MARK="Brick Hammer launcher: safety-net wrapper"
mkdir -p "$(dirname "$LOG")"

say() {
	echo "$(date '+%F %T') $*" >> "$LOG"
}

installed() {
	grep -q "$MARK" "$BIN/nextui.elf" 2>/dev/null
}

if [ -z "$SYSTEM_PATH" ] || [ ! -f "$BIN/nextui.elf" ]; then
	say "not run from NextUI (SYSTEM_PATH=$SYSTEM_PATH); nothing changed"
	exit 1
fi

if installed; then
	if [ ! -f "$BIN/nextui-stock.elf" ]; then
		say "remove: nextui-stock.elf is missing, so the stock launcher cannot be restored; nothing changed"
		exit 1
	fi
	mv -f "$BIN/nextui-stock.elf" "$BIN/nextui.elf"
	rm -f "$BIN/launcher.elf" "$BIN/muse"
	rm -f "$RES/Inter-Regular.ttf" "$RES/Inter-SemiBold.ttf" "$RES/Inter-Bold.ttf"
	sync
	say "removed $(cat VERSION); the stock NextUI launcher is back"
	exit 0
fi

# Install. A stock nextui.elf is kept once; a later reinstall (after a NextUI update put a new
# stock launcher in place) replaces the backup with that newer stock launcher.
cp -f "$BIN/nextui.elf" "$BIN/nextui-stock.elf" || { say "install: could not back up nextui.elf"; exit 1; }
cp -f "$PAK/launcher.elf" "$PAK/muse" "$BIN/" || { say "install: could not copy the launcher"; exit 1; }
cp -f "$PAK/nextui-wrapper.sh" "$BIN/nextui.elf"
chmod +x "$BIN/nextui.elf" "$BIN/launcher.elf" "$BIN/muse" "$BIN/nextui-stock.elf"
mkdir -p "$RES"
cp -f "$PAK"/fonts/*.ttf "$RES/"
rm -f /tmp/launcher_crashes
sync
say "installed $(cat VERSION)"
