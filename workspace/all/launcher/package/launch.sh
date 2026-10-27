#!/bin/sh
# Bramley installer Pak.
#
# Run it once to install Bramley on a NextUI card, run it again to remove it. Bramley replaces
# NextUI's launcher, its Settings program, its emulator (minarch) and its Wi-Fi start script.
# Installing keeps each stock file next to ours (nextui-stock.elf, settings-stock.elf,
# minarch-stock.elf, wifi_init-stock.sh) and removing puts them back. The safety net (nextui-wrapper.sh, installed as nextui.elf) falls back
# to the stock launcher if ours ever crashes on start. Music bookmarks and Bramley's settings in
# .userdata are kept either way.
#
# A NextUI update can restore the stock programs; run this Pak again afterwards to reinstall.

cd "$(dirname "$0")"
PAK="$(pwd)"
BIN="$SYSTEM_PATH/bin"
RES="$SDCARD_PATH/.system/res"
SETTINGS="$SDCARD_PATH/Tools/$PLATFORM/Settings.pak"
WIFI="$SYSTEM_PATH/etc/wifi"
LOG="${LOGS_PATH:-$SDCARD_PATH/.userdata/$PLATFORM/logs}/bramley.txt"
MARK="Bramley: safety-net wrapper"
OLD_MARK="Brick Hammer launcher: safety-net wrapper" # Bramley's name before 2026-10-27
mkdir -p "$(dirname "$LOG")"

say() {
	echo "$(date '+%F %T') $*" >> "$LOG"
}

installed() {
	grep -q -e "$MARK" -e "$OLD_MARK" "$BIN/nextui.elf" 2>/dev/null
}

# Puts back a stock program kept as <name>-stock.elf, if there is one.
restore() { # dir name
	if [ -f "$1/$2-stock.elf" ]; then
		mv -f "$1/$2-stock.elf" "$1/$2.elf"
	fi
}

# Keeps the stock program once, then copies ours over it.
replace() { # dir name source
	if [ ! -f "$1/$2-stock.elf" ]; then
		cp -f "$1/$2.elf" "$1/$2-stock.elf" || return 1
	fi
	cp -f "$3" "$1/$2.elf" && chmod +x "$1/$2.elf"
}

if [ -z "$SYSTEM_PATH" ] || [ ! -f "$BIN/nextui.elf" ]; then
	say "not run from NextUI (SYSTEM_PATH=$SYSTEM_PATH); nothing changed"
	exit 1
fi

if installed && [ "$(cat "$BIN/.bramley-version" 2>/dev/null)" = "$(cat VERSION)" ]; then
	if [ ! -f "$BIN/nextui-stock.elf" ]; then
		say "remove: nextui-stock.elf is missing, so the stock launcher cannot be restored; nothing changed"
		exit 1
	fi
	mv -f "$BIN/nextui-stock.elf" "$BIN/nextui.elf"
	restore "$BIN" minarch
	restore "$SETTINGS" settings
	[ -f "$WIFI/wifi_init-stock.sh" ] && mv -f "$WIFI/wifi_init-stock.sh" "$WIFI/wifi_init.sh"
	# The music daemon outlives the launcher by design, so stop it before its file goes.
	killall muse 2>/dev/null
	rm -f "$BIN/launcher.elf" "$BIN/muse" "$BIN/.bramley-version"
	rm -f "$RES/Inter-Regular.ttf" "$RES/Inter-SemiBold.ttf" "$RES/Inter-Bold.ttf"
	rm -rf "$RES/bramley"
	sync
	say "removed $(cat VERSION); stock NextUI is back"
	exit 0
fi

# Install, or update an older Bramley (or Brick Hammer Launcher) in place. The stock launcher is
# kept once; a later reinstall after a NextUI update replaces the backup with the newer stock one.
if ! installed; then
	cp -f "$BIN/nextui.elf" "$BIN/nextui-stock.elf" || { say "install: could not back up nextui.elf"; exit 1; }
fi
# A daemon left from an older install would keep running the old muse; the launcher starts the
# new one when it next needs music.
killall muse 2>/dev/null
cp -f "$PAK/launcher.elf" "$PAK/muse" "$BIN/" || { say "install: could not copy the launcher"; exit 1; }
cp -f "$PAK/nextui-wrapper.sh" "$BIN/nextui.elf"
chmod +x "$BIN/nextui.elf" "$BIN/launcher.elf" "$BIN/muse" "$BIN/nextui-stock.elf"
replace "$BIN" minarch "$PAK/minarch.elf" || say "install: could not install minarch; the stock one stays"
if [ -f "$SETTINGS/settings.elf" ]; then
	replace "$SETTINGS" settings "$PAK/settings.elf" || say "install: could not install Settings; the stock one stays"
fi
if [ -f "$WIFI/wifi_init.sh" ]; then
	[ -f "$WIFI/wifi_init-stock.sh" ] || cp -f "$WIFI/wifi_init.sh" "$WIFI/wifi_init-stock.sh"
	cp -f "$PAK/wifi_init.sh" "$WIFI/wifi_init.sh" && chmod +x "$WIFI/wifi_init.sh"
fi
mkdir -p "$RES/bramley"
cp -f "$PAK"/fonts/*.ttf "$RES/"
cp -rf "$PAK"/res/* "$RES/bramley/"
cp -f VERSION "$BIN/.bramley-version"
rm -f /tmp/launcher_crashes
sync
say "installed $(cat VERSION)"
