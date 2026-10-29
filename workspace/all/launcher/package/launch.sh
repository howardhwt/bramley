#!/bin/sh
# Bramley installer Pak.
#
# Run it once to install Bramley on a NextUI card, run it again to remove it. Bramley replaces
# NextUI's launcher, its Settings program, its emulator (minarch), its settings library and its
# Wi-Fi start script. Installing keeps each stock file next to ours (nextui-stock.elf,
# settings-stock.elf, minarch-stock.elf, libmsettings-stock.so, wifi_init-stock.sh) and removing
# puts them back. The safety net
# (nextui-wrapper.sh, installed as nextui.elf) falls back to the stock launcher if ours ever
# crashes on start. Music bookmarks and Bramley's settings in .userdata are kept either way.
#
# Installing also sets the start-up screens to Bramley's: the boot logo on the boot partition and
# the firmware's loading picture, keeping the stock ones in .userdata/shared/.bramley; removing puts
# them back.
#
# A NextUI update can restore the stock programs; run this Pak again afterwards to reinstall.

cd "$(dirname "$0")"
PAK="$(pwd)"
BIN="$SYSTEM_PATH/bin"
RES="$SDCARD_PATH/.system/res"
SETTINGS="$SDCARD_PATH/Tools/$PLATFORM/Settings.pak"
WIFI="$SYSTEM_PATH/etc/wifi"
LIB="$SYSTEM_PATH/lib"
LOG="${LOGS_PATH:-$SDCARD_PATH/.userdata/$PLATFORM/logs}/bramley.txt"
STATE="$SDCARD_PATH/.userdata/shared/.bramley"
BOOTDEV=/dev/mmcblk0p1
BOOTMNT=/tmp/bramley-boot
MARK="Bramley: safety-net wrapper"
OLD_MARK="Brick Hammer launcher: safety-net wrapper" # Bramley's name before 2026-10-27
mkdir -p "$(dirname "$LOG")"

say() {
	echo "$(date '+%F %T') $*" >> "$LOG"
}

installed() {
	grep -q -e "$MARK" -e "$OLD_MARK" "$BIN/nextui.elf" 2>/dev/null
}

# The start-up screens: the boot logo on the boot partition and the firmware's loading picture
# (/etc/splash.png). Both are outside the SD card, so the stock ones are kept on the card first and
# each write is read back and checked. A failure leaves the stock picture in place.
same() { [ "$(md5sum < "$1")" = "$(md5sum < "$2")" ]; }

boot_logo() { # install | restore
	mkdir -p "$BOOTMNT" "$STATE"
	mount -t vfat "$BOOTDEV" "$BOOTMNT" 2>/dev/null || { say "boot logo: could not open the boot partition; left as it was"; return; }
	if [ "$1" = install ]; then
		[ -f "$STATE/bootlogo-stock.bmp" ] || cp -f "$BOOTMNT/bootlogo.bmp" "$STATE/bootlogo-stock.bmp"
		if [ -f "$STATE/bootlogo-stock.bmp" ] && cp -f "$PAK/bootlogo.bmp" "$BOOTMNT/bootlogo.bmp.new" && sync &&
			same "$PAK/bootlogo.bmp" "$BOOTMNT/bootlogo.bmp.new" && mv -f "$BOOTMNT/bootlogo.bmp.new" "$BOOTMNT/bootlogo.bmp"; then
			say "boot logo: Bramley's"
		else
			rm -f "$BOOTMNT/bootlogo.bmp.new"
			say "boot logo: could not write; the stock one stays"
		fi
	elif [ -f "$STATE/bootlogo-stock.bmp" ]; then
		cp -f "$STATE/bootlogo-stock.bmp" "$BOOTMNT/bootlogo.bmp" && sync && same "$STATE/bootlogo-stock.bmp" "$BOOTMNT/bootlogo.bmp" &&
			rm -f "$STATE/bootlogo-stock.bmp" && say "boot logo: stock one back"
	fi
	sync
	umount "$BOOTMNT"
}

splash() { # install | restore
	mkdir -p "$STATE"
	if [ "$1" = install ]; then
		[ -f /etc/splash.png ] || return
		[ -f "$STATE/splash-stock.png" ] || cp -f /etc/splash.png "$STATE/splash-stock.png"
		if [ -f "$STATE/splash-stock.png" ] && cp -f "$PAK/splash.png" /etc/splash.png.new && sync &&
			same "$PAK/splash.png" /etc/splash.png.new && chmod 775 /etc/splash.png.new && mv -f /etc/splash.png.new /etc/splash.png; then
			say "splash: Bramley's"
		else
			rm -f /etc/splash.png.new
			say "splash: could not write; the stock one stays"
		fi
	elif [ -f "$STATE/splash-stock.png" ]; then
		cp -f "$STATE/splash-stock.png" /etc/splash.png && sync && same "$STATE/splash-stock.png" /etc/splash.png &&
			rm -f "$STATE/splash-stock.png" && say "splash: stock one back"
	fi
	sync
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
	[ -f "$LIB/libmsettings-stock.so" ] && mv -f "$LIB/libmsettings-stock.so" "$LIB/libmsettings.so"
	# The music daemon outlives the launcher by design, so stop it before its file goes.
	killall muse 2>/dev/null
	rm -f "$BIN/launcher.elf" "$BIN/muse" "$BIN/.bramley-version"
	rm -f "$RES/Inter-Regular.ttf" "$RES/Inter-SemiBold.ttf" "$RES/Inter-Bold.ttf"
	rm -rf "$RES/bramley"
	boot_logo restore
	splash restore
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
if [ -f "$LIB/libmsettings.so" ]; then
	[ -f "$LIB/libmsettings-stock.so" ] || cp -f "$LIB/libmsettings.so" "$LIB/libmsettings-stock.so"
	cp -f "$PAK/libmsettings.so" "$LIB/libmsettings.so"
fi
mkdir -p "$RES/bramley"
cp -f "$PAK"/fonts/*.ttf "$RES/"
cp -rf "$PAK"/res/* "$RES/bramley/"
boot_logo install
splash install
cp -f VERSION "$BIN/.bramley-version"
rm -f /tmp/launcher_crashes
sync
say "installed $(cat VERSION)"
